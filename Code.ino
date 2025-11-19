// === Blynk Template Info ===
#define BLYNK_TEMPLATE_ID "TMPL3fExQMeu9"
#define BLYNK_TEMPLATE_NAME "SmartEnergyMeter"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>

// === WiFi & Blynk Credentials ===
char ssid[] = "yourwifiname";
char pass[] = "yourwifipassword";
char auth[] = "JDZCI0o_eamM_rfOcqMSV_Q1TinO0yLs"; // Blynk Auth Token

// === SoftwareSerial Setup for 3 PZEM Modules ===
// ESP-12E safe pins
SoftwareSerial pzem1Serial(14, 12); // D5 (TX), D6 (RX)
SoftwareSerial pzem2Serial(13, 15); // D7 (TX), D8 (RX)
SoftwareSerial pzem3Serial(5, 4);   // D1 (TX), D2 (RX)

PZEM004Tv30 pzem1(pzem1Serial);
PZEM004Tv30 pzem2(pzem2Serial);
PZEM004Tv30 pzem3(pzem3Serial);

// === Phase Values ===
float voltage1, current1, power1, energy1, frequency1, pf1, va1, VAR1;
float voltage2, current2, power2, energy2, frequency2, pf2, va2, VAR2;
float voltage3, current3, power3, energy3, frequency3, pf3, va3, VAR3;
float voltage3ph, current3ph, power3ph, energy3ph, frequency3ph, pf3ph, va3ph, VAR3ph;

// === Functions ===
float zeroIfNan(float v);
void WiFiCheck();

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println("\nSmart 3-Phase Meter Booting...");

  // Connect WiFi + Blynk Cloud
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 8080);

  Serial.println("WiFi + Blynk Connected!");
  delay(1000);
}

// === MAIN LOOP ===
void loop() {
  Blynk.run();
  WiFiCheck();

  // === Read Phase 1 ===
  voltage1 = zeroIfNan(pzem1.voltage());
  current1 = zeroIfNan(pzem1.current());
  power1   = zeroIfNan(pzem1.power());
  energy1  = zeroIfNan(pzem1.energy() / 1000); // kWh
  frequency1 = zeroIfNan(pzem1.frequency());
  pf1 = zeroIfNan(pzem1.pf());
  va1 = (pf1 == 0) ? 0 : power1 / pf1;
  VAR1 = (pf1 == 0) ? 0 : power1 / pf1 * sqrt(1 - sq(pf1));

  // === Read Phase 2 ===
  voltage2 = zeroIfNan(pzem2.voltage());
  current2 = zeroIfNan(pzem2.current());
  power2   = zeroIfNan(pzem2.power());
  energy2  = zeroIfNan(pzem2.energy() / 1000);
  frequency2 = zeroIfNan(pzem2.frequency());
  pf2 = zeroIfNan(pzem2.pf());
  va2 = (pf2 == 0) ? 0 : power2 / pf2;
  VAR2 = (pf2 == 0) ? 0 : power2 / pf2 * sqrt(1 - sq(pf2));

  // === Read Phase 3 ===
  voltage3 = zeroIfNan(pzem3.voltage());
  current3 = zeroIfNan(pzem3.current());
  power3   = zeroIfNan(pzem3.power());
  energy3  = zeroIfNan(pzem3.energy() / 1000);
  frequency3 = zeroIfNan(pzem3.frequency());
  pf3 = zeroIfNan(pzem3.pf());
  va3 = (pf3 == 0) ? 0 : power3 / pf3;
  VAR3 = (pf3 == 0) ? 0 : power3 / pf3 * sqrt(1 - sq(pf3));

  // === 3-Phase Calculation ===
  voltage3ph = sqrt(sq(voltage1) + sq(voltage2) + sq(voltage3));
  current3ph = (current1 + current2 + current3) / 3.0;
  power3ph = power1 + power2 + power3;
  energy3ph = energy1 + energy2 + energy3;
  va3ph = va1 + va2 + va3;
  VAR3ph = VAR1 + VAR2 + VAR3;
  frequency3ph = (frequency1 + frequency2 + frequency3) / 3.0;
  pf3ph = (pf1 + pf2 + pf3) / 3.0;

  // === Send to Blynk ===
  Blynk.virtualWrite(V1, voltage3ph);
  Blynk.virtualWrite(V2, current3ph);
  Blynk.virtualWrite(V3, power3ph);
  Blynk.virtualWrite(V4, energy3ph);
  Blynk.virtualWrite(V5, frequency3ph);
  Blynk.virtualWrite(V6, pf3ph);
  Blynk.virtualWrite(V7, va3ph);
  Blynk.virtualWrite(V8, VAR3ph);

  // === Serial Debug (Normal) ===
  Serial.printf("3PH => V:%.1fV  I:%.2fA  P:%.1fW  F:%.1fHz  PF:%.2f  E:%.2fkWh\n",
                voltage3ph, current3ph, power3ph, frequency3ph, pf3ph, energy3ph);

  // === REQUIRED: Serial output for Python Bridge ===
  Serial.printf("L1:%.1fV %.2fA %.1fW\n", voltage1, current1, power1);
  Serial.printf("L2:%.1fV %.2fA %.1fW\n", voltage2, current2, power2);
  Serial.printf("L3:%.1fV %.2fA %.1fW\n", voltage3, current3, power3);

  Serial.printf("Energy: %.3fkWh\n", energy3ph);

  float cost = energy3ph * 8.0; // ₹8 per unit
  Serial.printf("Cost: %.2f\n", cost);

  delay(2000);
}

// === Utility Functions ===
float zeroIfNan(float v) {
  return isnan(v) ? 0 : v;
}

void WiFiCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    delay(2000);
  }
}
