/*
 * ================================================================
 *  مراقب الصحة — Arduino Nano 33 IoT
 *  غيّر WIFI_SSID و WIFI_PASSWORD فقط ثم ارفع الكود
 * ================================================================
 */

#include <WiFiNINA.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include "Protocentral_MAX30205.h"

// ================================================================
//  ⚠️  غيّر هذين السطرين فقط
// ================================================================
const char* WIFI_SSID     = "mohamed omer";
const char* WIFI_PASSWORD = "mohamed@2025";
// ================================================================

WiFiServer server(80);

// ===== AD8232 =====
const int ECG_PIN  = A0;
const int LO_PLUS  = 1;
const int LO_MINUS = 2;

// ===== MAX30102 =====
MAX30105 particleSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t  heartRate;
int8_t   validHeartRate;
int32_t  spo2;
int8_t   validSPO2;

// ===== MAX30205 =====
MAX30205 tempSensor;

// ===== القيم المشتركة =====
float  g_temp     = 0.0;
int    g_hr       = 0;
int    g_spo2     = 0;
float  g_ecgVolt  = 0.0;
int    g_ecgRaw   = 0;
bool   g_leadsOff = true;
bool   g_validHR  = false;
bool   g_validSPO2= false;

unsigned long lastTempRead = 0;
unsigned long lastHRRead   = 0;

// ================================================================
void setupWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);

  int attempts = 0;
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    attempts++;
    if (attempts > 30) {
      Serial.println("\nFailed — check SSID/password and restart.");
      while (true);
    }
  }

  Serial.println("\n✓ WiFi Connected!");
  Serial.print("✓ IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("-----------------------------");
  Serial.println("أدخل هذا IP في تطبيق الموبايل");
  Serial.println("-----------------------------");
}

// ================================================================
void sendJSON(WiFiClient& client) {
  String json = "{";
  json += "\"heartRate\":"   + String(g_hr)            + ",";
  json += "\"spo2\":"        + String(g_spo2)           + ",";
  json += "\"temperature\":" + String(g_temp,    2)     + ",";
  json += "\"ecgVoltage\":"  + String(g_ecgVolt, 3)     + ",";
  json += "\"ecgRaw\":"      + String(g_ecgRaw)         + ",";
json += "\"leadsOff\":";
json += (g_leadsOff ? "true" : "false");
json += ",";

json += "\"validHR\":";
json += (g_validHR ? "true" : "false");
json += ",";

json += "\"validSPO2\":";
json += (g_validSPO2 ? "true" : "false");


  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("Content-Length: " + String(json.length()));
  client.println();
  client.print(json);
}

// ================================================================
void handleClient(WiFiClient& client) {
  String req = "";
  unsigned long t = millis();

  while (client.connected() && millis() - t < 1000) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (c == '\n') break;
    }
  }

  // تفريغ باقي الـ headers
  while (client.connected() && millis() - t < 1500) {
    if (client.available()) client.read();
    else break;
  }

  if (req.indexOf("GET /data") >= 0) {
    sendJSON(client);
  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("Health Monitor - Nano 33 IoT");
    client.println("Use /data for JSON");
    client.print("IP: ");
    client.println(WiFi.localIP());
  }

  delay(1);
  client.stop();
}

// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin();

  // ===== AD8232 =====
  pinMode(LO_PLUS,  INPUT);
  pinMode(LO_MINUS, INPUT);
  Serial.println("AD8232 ready");

  // ===== MAX30102 =====
  if (!particleSensor.begin(Wire)) {
    Serial.println("ERROR: MAX30102 not found!");
    while (true) delay(1000);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  Serial.println("MAX30102 ready");

  // ===== MAX30205 =====
  int tries = 0;
  while (!tempSensor.scanAvailableSensors()) {
    Serial.println("Waiting for MAX30205...");
    delay(2000);
    tries++;
    if (tries > 5) {
      Serial.println("WARNING: MAX30205 not found");
      break;
    }
  }
  tempSensor.begin();
  Serial.println("MAX30205 ready");

  // ===== WiFi =====
  setupWiFi();
  server.begin();
  Serial.println("HTTP server started");
}

// ================================================================
void loop() {
  unsigned long now = millis();

  // ── 1. قراءة ECG في كل دورة ──────────────────────────────────
  g_leadsOff = (digitalRead(LO_PLUS) == HIGH) || (digitalRead(LO_MINUS) == HIGH);
  if (!g_leadsOff) {
    g_ecgRaw  = analogRead(ECG_PIN);
    g_ecgVolt = (g_ecgRaw * 3.3f) / 1023.0f;
  } else {
    g_ecgRaw  = 0;
    g_ecgVolt = 0.0f;
  }

  // ── 2. قراءة الحرارة كل ثانية ───────────────────────────────
  if (now - lastTempRead >= 1000) {
    g_temp       = tempSensor.getTemperature();
    lastTempRead = now;
  }

  // ── 3. قراءة HR/SpO2 كل 5 ثوانٍ ────────────────────────────
  if (now - lastHRRead >= 5000) {
    for (byte i = 0; i < 100; i++) {
      while (!particleSensor.available()) particleSensor.check();
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i]  = particleSensor.getIR();
      particleSensor.nextSample();

      // خدمة HTTP أثناء جمع العينات
      WiFiClient c = server.available();
      if (c) handleClient(c);

      delay(10);
    }

    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, 100, redBuffer,
      &spo2, &validSPO2,
      &heartRate, &validHeartRate
    );

    g_validHR  = (validHeartRate == 1) && (heartRate > 20)  && (heartRate < 300);
    g_validSPO2= (validSPO2      == 1) && (spo2      > 50)  && (spo2      <= 100);
    g_hr       = g_validHR   ? (int)heartRate : 0;
    g_spo2     = g_validSPO2 ? (int)spo2      : 0;

    lastHRRead = millis();

    Serial.print("HR:");    Serial.print(g_hr);
    Serial.print(" SpO2:"); Serial.print(g_spo2);
    Serial.print(" T:");    Serial.print(g_temp, 1);
    Serial.print(" ECG:");  Serial.print(g_ecgVolt, 2);
    Serial.print("V Leads:"); Serial.println(g_leadsOff ? "OFF" : "ON");
  }

  // ── 4. خدمة طلبات HTTP ───────────────────────────────────────
  WiFiClient client = server.available();
  if (client) handleClient(client);
}
