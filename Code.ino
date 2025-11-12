/***************************************************
   3-PHASE SMART ENERGY METER
   ESP8266 (NodeMCU / ESP-12E) + 3×PZEM004T + OLED + Blynk Cloud
   Modified by Kay & GPT-5
***************************************************/
// === Blynk Template Info ===
#define BLYNK_TEMPLATE_ID "TMPL34VHUQzsv"
#define BLYNK_TEMPLATE_NAME "SmartEnergyMeter"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



// === WiFi & Blynk Credentials ===
char ssid[] = "Your-WIFI-Name";
char pass[] = "Your-Wifi-Password";
char auth[] = "JDZCI0o_eamM_rfOcqMSV_Q1TinO0yLs"; // Blynk Auth Token

// === OLED Display ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_SDA 13  // D7
#define OLED_SCL 3   // RX
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === PZEM Connections ===
PZEM004Tv30 pzem1(4, 5);   // D2 -> TX, D1 -> RX
PZEM004Tv30 pzem2(2, 0);   // D4 -> TX, D3 -> RX
PZEM004Tv30 pzem3(12, 14); // D6 -> TX, D5 -> RX

// === Phase Values ===
float voltage1, current1, power1, energy1, frequency1, pf1, va1, VAR1;
float voltage2, current2, power2, energy2, frequency2, pf2, va2, VAR2;
float voltage3, current3, power3, energy3, frequency3, pf3, va3, VAR3;
float voltage3ph, current3ph, power3ph, energy3ph, frequency3ph, pf3ph, va3ph, VAR3ph;

// === Function Prototypes ===
void SetupDisplay();
float zeroIfNan(float v);
void WiFiCheck();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nSmart 3-Phase Meter Booting...");

  SetupDisplay();

  // Connect WiFi + Blynk Cloud
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 8080);

  display.clearDisplay();
  display.setCursor(10, 20);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("WiFi + Blynk Connected!");
  display.display();
  delay(1000);
}

void SetupDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("Initializing...");
  display.display();
}

// === MAIN LOOP ===
void loop() {
  Blynk.run();
  WiFiCheck();

  // === Read Phase 1 ===
  voltage1 = zeroIfNan(pzem1.voltage());
  current1 = zeroIfNan(pzem1.current());
  power1   = zeroIfNan(pzem1.power());
  energy1  = zeroIfNan(pzem1.energy() / 1000);
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

  // === Print on Serial (for Debug) ===
  Serial.printf("3PH => V:%.1fV  I:%.2fA  P:%.1fW  F:%.1fHz  PF:%.2f  E:%.2fkWh\n",
                voltage3ph, current3ph, power3ph, frequency3ph, pf3ph, energy3ph);

  // === Display on OLED ===
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("3PH V: "); display.println(voltage3ph);
  display.print("3PH I: "); display.println(current3ph);
  display.print("3PH P: "); display.println(power3ph);
  display.print("3PH PF: "); display.println(pf3ph);
  display.print("3PH E: "); display.println(energy3ph);
  display.display();

  delay(2000);
}

// === Functions ===
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
