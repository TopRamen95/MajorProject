# serial_bridge.py
import serial
import time
import requests
import sys
import argparse
from datetime import datetime

DEFAULT_PORT = None   # set to e.g. "COM3" or "/dev/ttyUSB0" if you want
BAUDRATE = 115200
PREDICT_URL = 'http://127.0.0.1:5000/predict'
READ_TIMEOUT = 1.0

def parse_phase_line(line):
    # expects e.g. "L1:230.5V 0.32A 72.3W"
    try:
        line = line.strip()
        if not line:
            return None
        if ':' not in line:
            return None
        tag, rest = line.split(':', 1)
        tag = tag.strip()
        # find numbers using splits
        # rest like "230.5V 0.32A 72.3W"
        parts = rest.strip().split()
        v = float(parts[0].rstrip('Vv'))
        c = float(parts[1].rstrip('Aa'))
        p = float(parts[2].rstrip('Ww'))
        return tag.upper(), {'v': v, 'c': c, 'p': p, 'e': None}
    except Exception:
        return None

def parse_energy_line(line):
    # "Energy: 5.21kWh"
    try:
        line = line.strip()
        if line.lower().startswith('energy'):
            val = ''.join(ch for ch in line if (ch.isdigit() or ch=='.' or ch=='-'))
            return float(val)
    except Exception:
        pass
    return None

def parse_cost_line(line):
    # "Cost: Rs 41.68"
    try:
        line = line.strip()
        if 'cost' in line.lower():
            val = ''.join(ch for ch in line if (ch.isdigit() or ch=='.' or ch=='-'))
            return float(val)
    except Exception:
        pass
    return None

def run(port_name):
    while True:
        try:
            print("Connecting to serial port", port_name, "...")
            with serial.Serial(port_name, BAUDRATE, timeout=READ_TIMEOUT) as ser:
                print("Connected. Reading lines...")
                block = {'L1': None, 'L2': None, 'L3': None, 'Energy': None, 'Cost': None}
                partial = ''
                while True:
                    raw = ser.readline().decode('utf-8', errors='ignore')
                    if not raw:
                        continue
                    line = raw.strip()
                    if not line:
                        continue
                    # parse
                    ph = parse_phase_line(line)
                    if ph:
                        tag, obj = ph
                        if tag in ('L1','L2','L3'):
                            block[tag] = obj
                    else:
                        e = parse_energy_line(line)
                        if e is not None:
                            block['Energy'] = e
                        else:
                            c = parse_cost_line(line)
                            if c is not None:
                                block['Cost'] = c

                    # if we have L1,L2,L3 and Energy => form sample
                    if block['L1'] and block['L2'] and block['L3'] and block['Energy'] is not None:
                        sample = {
                            't': datetime.utcnow().isoformat(),
                            'L1': block['L1'],
                            'L2': block['L2'],
                            'L3': block['L3'],
                            'totalPower': (block['L1']['p'] or 0) + (block['L2']['p'] or 0) + (block['L3']['p'] or 0),
                            'totalEnergy': block['Energy'],
                            'cost': block['Cost'] if block['Cost'] is not None else (block['Energy'] * 8.0)
                        }
                        # POST to predictor
                        try:
                            r = requests.post(PREDICT_URL, json=sample, timeout=3)
                            if r.ok:
                                print("Posted sample:", sample['t'], "pred resp:", r.json())
                            else:
                                print("POST failed:", r.status_code, r.text)
                        except Exception as ex:
                            print("Post error:", ex)
                        # reset block
                        block = {'L1': None, 'L2': None, 'L3': None, 'Energy': None, 'Cost': None}
        except serial.SerialException as e:
            print("SerialException:", e)
            print("Retrying in 3s...")
            time.sleep(3)
        except KeyboardInterrupt:
            print("Exiting on user request")
            sys.exit(0)
        except Exception as e:
            print("Bridge error:", e)
            time.sleep(3)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Serial to /predict bridge')
    parser.add_argument('--port', help='Serial port (e.g. COM3 or /dev/ttyUSB0)', default=DEFAULT_PORT)
    parser.add_argument('--url', help='Predictor URL', default=PREDICT_URL)
    args = parser.parse_args()
    if args.port is None:
        print("No --port provided. Please provide the serial port, e.g. --port COM3 or /dev/ttyUSB0")
        print("List ports: on Linux use ls /dev/ttyUSB* or /dev/ttyACM*; on Windows check Device Manager")
        sys.exit(1)
    PREDICT_URL = args.url
    run(args.port)
