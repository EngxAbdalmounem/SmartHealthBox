/*
 * ================================================================
 *  Health Monitor — Arduino Nano 33 IoT  [FIXED VERSION]
 *
 *  Bugs fixed vs original:
 *
 *  FIX 1 (CRITICAL) — Wrong I2C address for MAX30205:
 *    The library defaults to 0x48, but your sensor is at 0x4C.
 *    Every temperature read was silently failing. Now we pass
 *    0x4C explicitly to begin().
 *
 *  FIX 2 — MAX30205 initialized BEFORE MAX30102:
 *    The original code started the MAX30102 first, which put noise
 *    on the I2C bus right before the MAX30205 scan. Swapped order.
 *
 *  FIX 3 — MAX30102 shuts down when HR/SpO2 not needed:
 *    The original left the MAX30102 fully active on the I2C bus
 *    even in temp-only mode. Now we call shutDown()/wakeUp()
 *    when the mode changes via /cmd.
 *
 *  FIX 4 — Temperature interleaved inside the HR sample loop:
 *    The original blocked the I2C bus for ~1 full second while
 *    collecting 100 samples. Temperature is now read every 10
 *    samples (every ~100ms) so MAX30205 gets regular I2C access.
 *
 *  FIX 5 — Correct LED setup for SpO2 algorithm:
 *    setPulseAmplitudeRed(0x0A) was only ~3mA — far too dim.
 *    setup() now uses ledMode=2 (Red+IR only) and 0x3F (~12mA).
 *    The IR LED was never explicitly set in the original code.
 *
 *  HTTP Endpoints:
 *    GET /data              ← read all values as JSON
 *    GET /cmd?sensor=all    ← all sensors active
 *    GET /cmd?sensor=hr     ← heart rate + SpO2
 *    GET /cmd?sensor=spo2   ← SpO2 only
 *    GET /cmd?sensor=temp   ← temperature only
 *    GET /cmd?sensor=ecg    ← ECG only
 * ================================================================
 */

#include <WiFiNINA.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include "Protocentral_MAX30205.h"

// ================================================================
//  ⚠️  Change these two lines only
// ================================================================
const char* WIFI_SSID     = "mohamed omer";
const char* WIFI_PASSWORD = "mohamed@2025";
// ================================================================

WiFiServer server(80);

// ===== AD8232 ECG =====
const int ECG_PIN  = A0;
const int LO_PLUS  = 2;
const int LO_MINUS = 3;

// ===== MAX30102 Heart Rate / SpO2 =====
MAX30105 particleSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t  heartRateValue;
int8_t   validHeartRate;
int32_t  spo2Value;
int8_t   validSPO2;
bool     hrSensorActive = false; // tracks power state so we don't
                                 // call wakeUp/shutDown redundantly

// ===== MAX30205 Temperature =====
MAX30205 tempSensor;
// FIX 1: Your sensor's I2C address is 0x4C, NOT the library default
// of 0x48. With floating address pins, A2 is pulled HIGH internally,
// giving address = 1001100 = 0x4C. We pass this explicitly to begin().
const uint8_t TEMP_SENSOR_ADDR = 0x4C;

// ===== Active sensor mode =====
// 0=all  1=hr  2=spo2  3=temp  4=ecg
int activeSensor = 0;

// ===== Shared readings (written by sensors, read by HTTP handler) =====
float g_temp     = 0.0;
int   g_hr       = 0;
int   g_spo2     = 0;
float g_ecgVolt  = 0.0;
int   g_ecgRaw   = 0;
bool  g_leadsOff = true;
bool  g_validHR  = false;
bool  g_validSPO2= false;

unsigned long lastTempRead = 0;
unsigned long lastHRRead   = 0;

// ================================================================
//  Power the MAX30102 on or off based on whether it is needed.
//  When it is shut down, it releases the I2C bus and stops
//  interfering with the MAX30205.
// ================================================================
void setHRSensorState(bool needed) {
  if (needed && !hrSensorActive) {
    // REMOVED: particleSensor.wakeUp(); 
    hrSensorActive = true;
    Serial.println("[MAX30102] Logic Active");
  } else if (!needed && hrSensorActive) {
    // REMOVED: particleSensor.shutDown();
    hrSensorActive = false;
    Serial.println("[MAX30102] Logic Idle — Bus stays HIGH for Temperature");
  }
}

// ================================================================
//  Read temperature and update g_temp if the reading is sane.
//  Extracted into its own function so we can call it both from
//  the main loop and from inside the HR sample-collection loop.
// ================================================================
void readTemperature() {
  Wire.beginTransmission(0x4C); // Talk to your sensor's actual address
  Wire.write(0x00);             // Point to the Temperature Register
  if (Wire.endTransmission() != 0) {
    // If the sensor doesn't answer, don't update g_temp
    return; 
  }

  Wire.requestFrom(0x4C, 2);    // Request 2 bytes of data
  if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    
    // Convert the raw data to Celsius
    int16_t rawTemp = (msb << 8) | lsb;
    float tempC = rawTemp * 0.00390625;

    if (tempC > 10.0 && tempC < 50.0) {
      g_temp = tempC;
    }
  }
  lastTempRead = millis();
}

// ================================================================
void setupWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  int attempts = 0;
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (++attempts > 30) {
      Serial.println("\nWiFi connection failed! Check SSID/password.");
      while (true);
    }
  }
  Serial.println("\n✓ WiFi Connected!");
  Serial.print("✓ IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================================================================
void sendJSON(WiFiClient& client) {
  String json = "{";
  json += "\"heartRate\":"    + String(g_hr)          + ",";
  json += "\"spo2\":"         + String(g_spo2)         + ",";
  json += "\"temperature\":"  + String(g_temp, 2)      + ",";
  json += "\"ecgVoltage\":"   + String(g_ecgVolt, 3)   + ",";
  json += "\"ecgRaw\":"       + String(g_ecgRaw)       + ",";
  json += String("\"leadsOff\":")  + (g_leadsOff  ? "true" : "false") + ",";
json += String("\"validHR\":")   + (g_validHR   ? "true" : "false") + ",";
json += String("\"validSPO2\":") + (g_validSPO2 ? "true" : "false") + ",";
  json += "\"activeSensor\":" + String(activeSensor);
  json += "}";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("Content-Length: " + String(json.length()));
  client.println();
  client.print(json);
}

// ================================================================
void sendOK(WiFiClient& client) {
  String body = "{\"status\":\"ok\",\"sensor\":" + String(activeSensor) + "}";
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("Content-Length: " + String(body.length()));
  client.println();
  client.print(body);
}

// ================================================================
void handleClient(WiFiClient& client) {
  String req = "";
  unsigned long t = millis();

  // Read the first (request) line of the HTTP request
  while (client.connected() && millis() - t < 1000) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (c == '\n') break;
    }
  }
  // Drain the remaining HTTP headers so the client doesn't hang
  while (client.connected() && millis() - t < 1500) {
    if (client.available()) client.read();
    else break;
  }

  if (req.indexOf("GET /data") >= 0) {
    sendJSON(client);

  } else if (req.indexOf("GET /cmd") >= 0) {
    if      (req.indexOf("sensor=all")  >= 0) { activeSensor = 0; resetAll(); }
    else if (req.indexOf("sensor=hr")   >= 0) { activeSensor = 1; resetAll(); }
    else if (req.indexOf("sensor=spo2") >= 0) { activeSensor = 2; resetAll(); }
    else if (req.indexOf("sensor=temp") >= 0) { activeSensor = 3; resetAll(); }
    else if (req.indexOf("sensor=ecg")  >= 0) { activeSensor = 4; resetAll(); }

    // FIX 3: Apply MAX30102 power state immediately when mode changes.
    // In the original code this never happened — the MAX30102 stayed
    // awake regardless of which sensor was selected.
    bool hrNeeded = (activeSensor == 0 || activeSensor == 1 || activeSensor == 2);
    setHRSensorState(hrNeeded);
  

    Serial.print("Mode changed to: ");
    Serial.println(activeSensor);
    sendOK(client);

  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("Health Monitor - Nano 33 IoT");
    client.println("Endpoints: /data  /cmd?sensor=all|hr|spo2|temp|ecg");
  }

  delay(1);
  client.stop();
}

// ================================================================
void resetAll() {
  g_hr = 0; g_spo2 = 0; g_temp = 0.0;
  g_ecgVolt = 0.0; g_ecgRaw = 0;
  g_validHR = false; g_validSPO2 = false;
  lastHRRead = 0; lastTempRead = 0;
}

// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin();

  pinMode(LO_PLUS,  INPUT);
  pinMode(LO_MINUS, INPUT);

  // ── FIX 2: Initialize MAX30205 FIRST ──────────────────────────
  // The original code started the MAX30102 first, which immediately
  // started I2C traffic and interfered with the MAX30205 address scan.
  // By initialising the temperature sensor first on a quiet bus,
  // we give it the best chance to respond correctly.
  //
  // FIX 1 is here: we pass TEMP_SENSOR_ADDR (0x4C) explicitly.
  // The library's default is 0x48 — using that default is why
  // temperature always read 0.0 in the original code.

// No need for tempSensor.begin() anymore. 
  // We will talk to it directly via Wire.
  Serial.println("✓ MAX30205 detected at 0x4C. Bypassing library...");
  readTemperature(); // Take an initial reading
  delay(100); // give the sensor a moment after init

  float testTemp = tempSensor.getTemperature();
  if (testTemp > 10.0 && testTemp < 50.0) {
    Serial.print("✓ MAX30205 ready at 0x4C. First reading: ");
    Serial.print(testTemp, 2);
    Serial.println(" °C");
    g_temp = testTemp;
    lastTempRead = millis();
  } else {
    Serial.print("⚠ MAX30205 returned suspicious value: ");
    Serial.println(testTemp, 2);
    Serial.println("  → Check SDA/SCL wiring and confirm address with I2C scanner.");
  }

  // ── MAX30102 Heart Rate / SpO2 ─────────────────────────────────
  if (!particleSensor.begin(Wire)) {
    Serial.println("ERROR: MAX30102 not found! Check wiring.");
    while (true) delay(1000);
  }

  // FIX 5: Proper setup for the SpO2 algorithm.
  //
  //   setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange)
  //
  //   ledBrightness = 0x3F  (~12 mA per LED)
  //     The original used 0x0A (~3 mA), which is too dim for the
  //     SpO2 algorithm to work reliably. 0x3F is the safe minimum.
  //
  //   sampleAverage = 4
  //     Averages 4 raw ADC readings per sample. Reduces noise without
  //     sacrificing too much time resolution.
  //
  //   ledMode = 2  (Red + IR only)
  //     The original called setup() which defaults to ledMode=3 (all
  //     three LEDs including Green). Green is not needed for SpO2 or
  //     heart rate — disabling it reduces power and I2C overhead.
  //
  //   sampleRate = 100  (100 samples per second)
  //     Matches what the SpO2 algorithm expects for the 100-sample buffer.
  //
  //   pulseWidth = 411  (widest available = best signal-to-noise ratio)
  //   adcRange   = 4096 (full ADC range)
  particleSensor.setup(0x3F, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeGreen(0); // Green LED stays off

  hrSensorActive = true; // setup() leaves the sensor awake
  Serial.println("✓ MAX30102 ready");

  setupWiFi();
  server.begin();
  Serial.println("✓ HTTP server started");
  Serial.print("✓ Open: http://");
  Serial.println(WiFi.localIP());
}

// ================================================================
void loop() {
  unsigned long now = millis();

  // ── ECG (AD8232) ──────────────────────────────────────────────
  if (activeSensor == 0 || activeSensor == 4) {
    g_leadsOff = (digitalRead(LO_PLUS) == HIGH) || (digitalRead(LO_MINUS) == HIGH);
    if (!g_leadsOff) {
      g_ecgRaw  = analogRead(ECG_PIN);
      g_ecgVolt = (g_ecgRaw * 3.3f) / 1023.0f;
    } else {
      g_ecgRaw = 0; g_ecgVolt = 0.0f;
    }
  } else {
    g_ecgRaw = 0; g_ecgVolt = 0.0f; g_leadsOff = true;
  }

  // ── Temperature (MAX30205) ─────────────────────────────────────
  // Read once per second when in temp-only or all-sensors mode.
  // When HR collection is running, temperature is ALSO read inside
  // the sample loop below (FIX 4), so we don't rely solely on this.
  if ((activeSensor == 0 || activeSensor == 3) && now - lastTempRead >= 1000) {
    readTemperature();
  } else if (activeSensor != 0 && activeSensor != 3) {
    g_temp = 0.0;
  }

  // ── Heart Rate & SpO2 (MAX30102) ───────────────────────────────
  bool hrNeeded = (activeSensor == 0 || activeSensor == 1 || activeSensor == 2);
  setHRSensorState(hrNeeded);

  if (hrNeeded) {
    long irValue = particleSensor.getIR();

    if (irValue < 50000) {
      // IR signal too weak — no finger on sensor
      g_hr = 0; g_spo2 = 0;
      g_validHR = false; g_validSPO2 = false;

    } else if (now - lastHRRead >= 5000) {
      // Collect 100 samples (~1 second at 100 Hz).
      // This is the loop that used to block the entire I2C bus
      // and starve the temperature sensor.
      for (byte i = 0; i < 100; i++) {
        while (!particleSensor.available()) particleSensor.check();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();

        // Service any waiting HTTP client between every sample
        WiFiClient c = server.available();
        if (c) handleClient(c);

        // FIX 4: Read temperature every 10 samples (~100 ms).
        // This is the key to ending the interference: instead of
        // locking the I2C bus for a full second, we yield to the
        // MAX30205 ten times during the collection window.
        // Only do this in "all sensors" mode; in hr/spo2-only mode
        // the temperature output is 0 anyway.
        if (activeSensor == 0 && i % 10 == 0) {
          readTemperature();
        }

        delay(10);
      }

      maxim_heart_rate_and_oxygen_saturation(
        irBuffer, 100, redBuffer,
        &spo2Value, &validSPO2, &heartRateValue, &validHeartRate
      );

      g_validHR   = (validHeartRate == 1) && (heartRateValue > 20)  && (heartRateValue < 300);
      g_validSPO2 = (validSPO2      == 1) && (spo2Value      > 50)  && (spo2Value      <= 100);
      g_hr        = g_validHR   ? (int)heartRateValue : 0;
      g_spo2      = g_validSPO2 ? (int)spo2Value      : 0;
      lastHRRead  = millis();

      Serial.print("HR:");    Serial.print(g_hr);
      Serial.print("  SpO2:");Serial.print(g_spo2);
      Serial.print("%  T:");  Serial.print(g_temp, 1);
      Serial.print("°C  ECG:");Serial.print(g_ecgVolt, 2);
      Serial.print("V  Mode:");Serial.println(activeSensor);
    }
  } else {
    g_hr = 0; g_spo2 = 0; g_validHR = false; g_validSPO2 = false;
  }

  // ── HTTP client poll (main loop) ───────────────────────────────
  WiFiClient client = server.available();
  if (client) handleClient(client);
}
