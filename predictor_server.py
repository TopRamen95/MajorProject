# predictor_server.py
from flask import Flask, request, jsonify
from flask_cors import CORS
import pandas as pd
import os, threading, time
from sklearn.ensemble import IsolationForest
from sklearn.linear_model import LinearRegression
import numpy as np
from datetime import datetime
import traceback

app = Flask(__name__)
CORS(app)

SAMPLES_CSV = 'samples.csv'
DAILY_HISTORY = 'daily_history.csv'   # optional: daily totals for training (date,totalEnergy)
LOCK = threading.Lock()

# Append received sample to CSV
def append_sample(sample):
    df = pd.DataFrame([{
        't': sample.get('t'),
        'totalPower': sample.get('totalPower'),
        'totalEnergy': sample.get('totalEnergy'),
        'L1_p': sample.get('L1',{}).get('p'),
        'L2_p': sample.get('L2',{}).get('p'),
        'L3_p': sample.get('L3',{}).get('p'),
    }])
    header = not os.path.exists(SAMPLES_CSV)
    df.to_csv(SAMPLES_CSV, mode='a', index=False, header=header)

# Prediction model (Linear Regression trained from DAILY_HISTORY)
_model = None
_model_ts = 0

def train_model_if_available():
    global _model, _model_ts
    try:
        if not os.path.exists(DAILY_HISTORY):
            _model = None
            return
        mtime = os.path.getmtime(DAILY_HISTORY)
        if _model and _model_ts == mtime:
            return
        df = pd.read_csv(DAILY_HISTORY)
        df['date'] = pd.to_datetime(df['date'])
        df = df.sort_values('date')
        vals = df['totalEnergy'].values
        X, y = [], []
        for i in range(7, len(vals)):
            X.append(vals[i-7:i])
            y.append(vals[i])
        if len(X) < 5:
            _model = None
            return
        X = np.array(X); y = np.array(y)
        mdl = LinearRegression()
        mdl.fit(X, y)
        _model = mdl
        _model_ts = mtime
        print("✅ Trained LinearRegression model from", DAILY_HISTORY)
    except Exception:
        print("train_model_if_available error:")
        traceback.print_exc()
        _model = None

# Build IsolationForest anomaly model from recent samples
_anom_model = None
def build_anomaly_model():
    global _anom_model
    try:
        if not os.path.exists(SAMPLES_CSV):
            _anom_model = None
            return
        df = pd.read_csv(SAMPLES_CSV).tail(800)
        if len(df) < 50:
            _anom_model = None
            return
        X = df[['totalPower','L1_p','L2_p','L3_p']].fillna(0).values
        iso = IsolationForest(contamination=0.01, random_state=1)
        iso.fit(X)
        _anom_model = iso
        print("✅ Built anomaly model from samples")
    except Exception:
        print("build_anomaly_model error:")
        traceback.print_exc()
        _anom_model = None

def periodic_train():
    while True:
        try:
            train_model_if_available()
            build_anomaly_model()
        except Exception:
            pass
        time.sleep(60)

threading.Thread(target=periodic_train, daemon=True).start()

@app.route('/')
def home():
    return ("<h3>Energy Predictor Backend Running</h3>"
            "<p>POST JSON to <code>/predict</code></p>")

@app.route('/predict', methods=['POST'])
def predict():
    data = request.get_json(force=True)
    if not data:
        return jsonify({'error':'no data'}), 400

    # append sample (thread-safe)
    with LOCK:
        try:
            append_sample(data)
        except Exception:
            print("append_sample error:")
            traceback.print_exc()

    # Attempt prediction using trained model
    pred_value = None
    reason = ''
    try:
        if _model is not None and os.path.exists(DAILY_HISTORY):
            df = pd.read_csv(DAILY_HISTORY)
            df = df.sort_values('date')
            vals = df['totalEnergy'].values
            if len(vals) >= 7:
                x = vals[-7:].reshape(1, -1)
                pred_value = float(_model.predict(x)[0])
                reason = 'model'
    except Exception:
        print("prediction (model) error:")
        traceback.print_exc()
        pred_value = None

    # fallback heuristics
    try:
        if pred_value is None:
            if os.path.exists(SAMPLES_CSV):
                df2 = pd.read_csv(SAMPLES_CSV)
                # try to get daily mean power -> daily kWh
                if 't' in df2.columns:
                    df2['t'] = pd.to_datetime(df2['t'])
                    df2['date'] = df2['t'].dt.date
                    daily = df2.groupby('date')['totalPower'].mean().reset_index()
                    if len(daily) >= 3:
                        mean_power = daily['totalPower'].tail(3).mean()
                        pred_value = mean_power * 24.0 / 1000.0
                        reason = 'moving_avg'
                if pred_value is None:
                    last = df2.tail(1)
                    if not last.empty:
                        p = float(last['totalPower'].iloc[0])
                        pred_value = p * 24.0 / 1000.0
                        reason = 'last_sample'
            else:
                p = float(data.get('totalPower',0))
                pred_value = p * 24.0 / 1000.0
                reason = 'instant_estimate'
    except Exception:
        traceback.print_exc()
        pred_value = None

    # anomaly detection
    anomaly = False
    anom_reason = ''
    try:
        if _anom_model is not None:
            X = np.array([[ data.get('totalPower',0),
                             data.get('L1',{}).get('p',0),
                             data.get('L2',{}).get('p',0),
                             data.get('L3',{}).get('p',0) ]])
            pred = _anom_model.predict(X)
            if pred[0] == -1:
                anomaly = True
                anom_reason = 'isolation_forest'
        else:
            # fallback z-score
            if os.path.exists(SAMPLES_CSV):
                df3 = pd.read_csv(SAMPLES_CSV)
                arr = df3['totalPower'].tail(200).fillna(0).values
                if len(arr) >= 30:
                    mean = arr.mean(); std = arr.std()
                    cur = float(data.get('totalPower',0))
                    if std > 0 and abs((cur-mean)/std) > 3:
                        anomaly = True
                        anom_reason = 'zscore'
    except Exception:
        traceback.print_exc()

    out = {'prediction': None, 'anomaly': anomaly, 'reason': anom_reason}
    if pred_value is not None:
        out['prediction'] = float(pred_value)

    return jsonify(out)

if __name__ == '__main__':
    print("Starting predictor server on http://127.0.0.1:5000")
    app.run(host='127.0.0.1', port=5000)
