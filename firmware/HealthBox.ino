// ====================================================================
//  SMART HEALTH BOX — Arduino Nano 33 IoT Firmware
//  Bachelor of Science (Honours) in Electrical Engineering
//  International University of Africa — Faculty of Engineering
//  Department of Electrical and Electronic Engineering
//
//  Authors   : Ahmed Sami  |  Abdalmounem
//  Team Name : SIDEQUEST
//  Supervisor: Dr. Nisreen
//  Year      : 2026
//
//  Project Summary:
//  A portable, low-cost IoT device for real-time monitoring of vital
//  signs: ECG, Heart Rate (HR), Blood Oxygen Saturation (SpO₂), and
//  Body Temperature. Data is transmitted wirelessly over Wi-Fi using
//  the HTTP protocol to a companion Android application developed by
//  the team.
//
//  Hardware Platform : Arduino Nano 33 IoT
//  Sensors Used      :
//    - AD8232   → ECG signal acquisition (analog, pin A0)
//    - MAX30102  → Heart Rate & SpO₂ (I²C, address 0x57)
//    - MAX30205  → Body Temperature   (I²C, address 0x4C)
//
//  Communication     : HTTP over shared local Wi-Fi
//                      The Arduino is assigned a STATIC IP so that
//                      the Android app always knows where to connect.
//                      (mDNS hostname resolution is unreliable on
//                       Android, so a fixed IP is the correct approach.)
//
//  NOTE on Respiratory Rate:
//  Respiratory rate is NOT implemented in this version. The hardware
//  and sensor suite are capable of PPG-derived respiration (PDR), but
//  the algorithm is left as future work due to the time constraints of
//  this academic project.
// ====================================================================
//
//  HTTP API Endpoints (called by the Android app over Wi-Fi):
//
//    GET /data              → Returns all current sensor values as JSON
//    GET /cmd?sensor=all    → Activate all sensors simultaneously
//    GET /cmd?sensor=hr     → Activate Heart Rate + SpO₂ only
//    GET /cmd?sensor=spo2   → Activate SpO₂ only
//    GET /cmd?sensor=temp   → Activate Temperature only
//    GET /cmd?sensor=ecg    → Activate ECG only
//
//  JSON Response Format (/data endpoint — 9 fields):
//    {
//      "heartRate"    : int,    // BPM,  0 if invalid or sensor idle
//      "spo2"         : int,    // %,    0 if invalid or sensor idle
//      "temperature"  : float,  // °C,   0.0 if sensor idle
//      "ecgVoltage"   : float,  // V,    converted from 10-bit ADC
//      "ecgRaw"       : int,    // 0–1023, raw ADC count
//      "leadsOff"     : bool,   // true  = electrode not attached
//      "validHR"      : bool,   // true  = HR reading passed sanity check
//      "validSPO2"    : bool,   // true  = SpO₂ reading passed sanity check
//      "activeSensor" : int     // 0=all 1=hr 2=spo2 3=temp 4=ecg
//    }
// ====================================================================

// ── Library Includes ─────────────────────────────────────────────────
// WiFiNINA  : Wi-Fi driver for the NINA-W102 module on the Nano 33 IoT
// Wire      : Arduino I²C library (used by MAX30102 and MAX30205)
// MAX30105  : SparkFun driver for the MAX30102 particle sensor
//             (SparkFun named their library "MAX30105" but it supports
//              the MAX30102 which is what this project uses)
// heartRate : Companion header from the SparkFun library — provides
//             the checkForBeat() helper (not used for final HR; kept
//             for potential future beat-by-beat debugging)
// spo2_algorithm : Provides maxim_heart_rate_and_oxygen_saturation(),
//             the official Maxim Integrated algorithm for computing
//             HR and SpO₂ from 100-sample IR/Red buffers.
//             *** This is NOT Pan-Tompkins. Pan-Tompkins is a QRS
//             detection algorithm for ECG signals. The MAX30102 uses
//             PPG (photoplethysmography), which requires a completely
//             different algorithm. ***
// Protocentral_MAX30205 : Driver for the MAX30205 digital thermometer.
#include <WiFiNINA.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include "Protocentral_MAX30205.h"


// ====================================================================
//  NETWORK CONFIGURATION
//  !! IMPORTANT — READ BEFORE UPLOADING !!
//
//  1. Set WIFI_SSID and WIFI_PASSWORD to match your router.
//
//  2. Set STATIC_IP to an address that:
//     a) Is on the same subnet as your router (e.g. 192.168.1.x)
//     b) Is OUTSIDE your router's DHCP pool (to avoid IP conflicts)
//     c) Matches the IP hardcoded in Ahmed's Android application
//
//  3. Set GATEWAY to your router's IP (usually 192.168.1.1)
//
//  WHY STATIC IP?
//  Android does not reliably support mDNS (.local hostnames), so the
//  app connects to a numeric IP address directly. If the Arduino gets
//  a different DHCP-assigned address after each reboot, the app will
//  fail to connect. A static IP solves this permanently.
// ====================================================================
const char* WIFI_SSID     = "mohamed omer";
const char* WIFI_PASSWORD = "mohamed@2025";

IPAddress STATIC_IP(192, 168, 1, 120);  // ← Set to the IP the Android app uses
IPAddress GATEWAY  (192, 168, 1,   1);  // ← Your router's IP address
IPAddress SUBNET   (255, 255, 255, 0);  // ← Standard home network subnet mask
// ====================================================================

// HTTP server listening on the standard port 80
WiFiServer server(80);


// ====================================================================
//  AD8232 — ECG SENSOR PIN ASSIGNMENTS
//
//  ECG_PIN  : Analog pin A0 receives the amplified ECG signal from
//             the AD8232's OUTPUT pin.
//
//  LO_PLUS  : "Leads-Off" detection — positive electrode.
//  LO_MINUS : "Leads-Off" detection — negative electrode.
//             When either LO pin reads HIGH, an electrode has
//             detached from the patient's skin. The firmware sets
//             g_leadsOff = true and zeroes the ECG output to prevent
//             transmitting meaningless noise to the app.
// ====================================================================
const int ECG_PIN  = A0;
const int LO_PLUS  = 2;
const int LO_MINUS = 3;


// ====================================================================
//  MAX30102 — HEART RATE & SpO₂ SENSOR
//
//  The SparkFun MAX30105 library object is used to communicate with
//  the MAX30102 over I²C.
//
//  irBuffer[]  : 100-sample buffer for the Infrared (IR) photodiode.
//                Infrared light (880 nm) is more strongly absorbed by
//                oxygenated haemoglobin (HbO₂).
//
//  redBuffer[] : 100-sample buffer for the Red photodiode.
//                Red light (660 nm) is absorbed differently depending
//                on blood oxygenation level.
//
//  Both buffers are passed together to maxim_heart_rate_and_oxygen_saturation()
//  which computes HR and SpO₂ using Maxim's proprietary algorithm
//  (from spo2_algorithm.h).
//
//  heartRateValue / spo2Value : Computed results (int32_t as required
//                               by the Maxim algorithm signature).
//  validHeartRate / validSPO2 : Quality flags — 1 means the algorithm
//                               is confident in the result, 0 means
//                               the signal was too noisy or too short.
// ====================================================================
MAX30105 particleSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t  heartRateValue;
int8_t   validHeartRate;
int32_t  spo2Value;
int8_t   validSPO2;

// Software flag: tracks whether the MAX30102 sampling logic is active.
// This is used to avoid redundant state-change calls in the main loop.
bool hrSensorActive = false;


// ====================================================================
//  MAX30205 — BODY TEMPERATURE SENSOR
//
//  The MAX30205 is a high-precision digital thermometer (±0.1°C)
//  that communicates over I²C.
//
//  Hardware I²C address: 0x4C
//  (Configured by the address pins A0, A1, A2 on the CJMCU module.)
//
//  IMPORTANT NOTE ON I²C BUS SHARING:
//  Both the MAX30102 and MAX30205 share the same I²C bus (SDA/SCL).
//  In an earlier version of this firmware, the MAX30102's hardware
//  shutDown() / wakeUp() commands were used to release the bus.
//  This caused the MAX30205 to fail silently — it would stop
//  responding mid-session without any error flag.
//
//  The solution implemented here: the MAX30102 is NEVER hardware-shut
//  down. Instead, the firmware controls which sensor's DATA is used
//  via the activeSensor mode flag. Temperature is read by making
//  direct I²C register calls (readTemperature()) which are shorter
//  and do not interfere with ongoing MAX30102 sampling.
// ====================================================================
MAX30205 tempSensor;
const uint8_t TEMP_SENSOR_ADDR = 0x4C;


// ====================================================================
//  SENSOR MODE FLAG
//
//  activeSensor controls which sensors are polled in the main loop.
//  The Android app sets this by sending a GET /cmd?sensor=X request.
//
//    0 = all    → ECG + HR + SpO₂ + Temperature active simultaneously
//    1 = hr     → HR + SpO₂ only (MAX30102)
//    2 = spo2   → SpO₂ only  (MAX30102)
//    3 = temp   → Temperature only (MAX30205)
//    4 = ecg    → ECG only (AD8232)
//
//  Default is 0 (all sensors) on power-up.
// ====================================================================
int activeSensor = 0;


// ====================================================================
//  SHARED GLOBAL READINGS
//
//  These variables are written by sensor-reading code and read by the
//  HTTP response handler (sendJSON). In a single-threaded Arduino
//  environment this is safe — the loop() function is not interrupted
//  mid-execution by the HTTP handler.
// ====================================================================
float g_temp     = 0.0;   // Body temperature in degrees Celsius
int   g_hr       = 0;     // Heart rate in beats per minute (BPM)
int   g_spo2     = 0;     // Blood oxygen saturation (SpO₂) in percent
float g_ecgVolt  = 0.0;   // ECG signal converted to volts
int   g_ecgRaw   = 0;     // ECG raw ADC count (0–1023, 10-bit)
bool  g_leadsOff = true;  // true = electrode detached / no skin contact
bool  g_validHR  = false; // true = HR value passed the sanity check
bool  g_validSPO2= false; // true = SpO₂ value passed the sanity check

// Timestamps for non-blocking polling intervals
unsigned long lastTempRead = 0;   // Last temperature read time (ms)
unsigned long lastHRRead   = 0;   // Last HR/SpO₂ computation time (ms)


// ====================================================================
//  FUNCTION: setHRSensorState(bool needed)
//
//  Controls whether the MAX30102 sampling logic is considered active.
//
//  DESIGN DECISION — Why hardware shutDown() was removed:
//  The MAX30102 library's shutDown() command pulls the I²C bus into
//  a state that the MAX30205 cannot recover from without a full power
//  cycle. This caused the temperature sensor to fail silently (no
//  error, just returning 0 or garbage) whenever the HR mode was
//  toggled off. Removing shutDown()/wakeUp() and keeping the MAX30102
//  always powered eliminates this I²C bus conflict entirely.
//
//  The function now manages only the SOFTWARE state (hrSensorActive),
//  which controls whether the HR sampling block in loop() executes.
// ====================================================================
void setHRSensorState(bool needed) {
  if (needed && !hrSensorActive) {
    // Sampling logic is now active — the MAX30102 was already powered
    hrSensorActive = true;
    Serial.println("[MAX30102] Sampling logic: ACTIVE");
  } else if (!needed && hrSensorActive) {
    // Sampling logic suspended — hardware stays powered to protect
    // the MAX30205 from I²C bus disruption
    hrSensorActive = false;
    Serial.println("[MAX30102] Sampling logic: IDLE (hardware stays ON to protect I2C bus)");
  }
}


// ====================================================================
//  FUNCTION: readTemperature()
//
//  Reads body temperature from the MAX30205 using direct I²C register
//  access (bypassing the library's higher-level call).
//
//  WHY DIRECT I²C?
//  The Protocentral library's getTemperature() function performs a
//  full I²C transaction that is slow enough to cause a visible delay
//  inside the MAX30102's 100-sample collection loop. Direct register
//  access is faster and gives us fine-grained control over timing.
//
//  HOW THE CONVERSION WORKS:
//  The MAX30205 stores temperature in a 16-bit register (0x00).
//  The value is a 2's-complement fixed-point number where each LSB
//  represents 0.00390625°C (i.e., 1/256 of a degree).
//  Formula: Temperature (°C) = rawTemp × 0.00390625
//
//  A sanity check (10°C < result < 50°C) rejects obviously bad reads.
//  If the sensor does not acknowledge on I²C, the function returns
//  without updating g_temp, preserving the last known good value.
// ====================================================================
void readTemperature() {
  // Step 1: Point the sensor's internal register pointer to 0x00 (Temp Register)
  Wire.beginTransmission(0x4C);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    // Sensor did not acknowledge — do not update g_temp
    return;
  }

  // Step 2: Request 2 bytes (MSB and LSB of the temperature register)
  Wire.requestFrom(0x4C, 2);
  if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    // Step 3: Reconstruct the 16-bit signed integer
    int16_t rawTemp = (msb << 8) | lsb;

    // Step 4: Apply the MAX30205 datasheet conversion factor
    float tempC = rawTemp * 0.00390625;

    // Step 5: Sanity check — valid human body temperature range
    if (tempC > 10.0 && tempC < 50.0) {
      g_temp = tempC;
    }
  }
  lastTempRead = millis();
}


// ====================================================================
//  FUNCTION: setupWiFi()
//
//  Connects the Arduino to the specified Wi-Fi network and assigns
//  the static IP address defined in the NETWORK CONFIGURATION block.
//
//  Static IP is configured BEFORE WiFi.begin() using WiFi.config().
//  This is the correct order for the WiFiNINA library.
//
//  The function retries up to 30 times (30 seconds) before halting
//  with an error message on the Serial Monitor.
// ====================================================================
void setupWiFi() {
  // Apply static IP configuration BEFORE connecting.
  // This prevents the DHCP client from overwriting our chosen address.
  WiFi.config(STATIC_IP, GATEWAY, SUBNET);

  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(WIFI_SSID);

  int attempts = 0;
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (++attempts > 30) {
      Serial.println("\n[ERROR] Wi-Fi connection failed after 30 attempts.");
      Serial.println("        Check SSID, password, and that the router is reachable.");
      while (true); // Halt — reboot the device to retry
    }
  }

  Serial.println("\n[OK] Wi-Fi connected.");
  Serial.print("[OK] Static IP assigned: ");
  Serial.println(WiFi.localIP());
  Serial.print("[OK] Android app endpoint: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/data");
}


// ====================================================================
//  FUNCTION: sendJSON(WiFiClient& client)
//
//  Builds and sends the 9-field JSON payload over HTTP in response
//  to a GET /data request from the Android application.
//
//  The response includes proper HTTP/1.1 headers:
//    - Content-Type: application/json
//    - Access-Control-Allow-Origin: *  (permits cross-origin requests
//      if the app uses a WebView component)
//    - Connection: close  (stateless — each request opens and closes
//      a new TCP connection, which is correct for HTTP/1.1 on Arduino)
//    - Content-Length: computed at runtime so the client knows exactly
//      how many bytes to read before closing the connection
// ====================================================================
void sendJSON(WiFiClient& client) {
  // Build the JSON string in memory before sending
  String json = "{";
  json += "\"heartRate\":"    + String(g_hr)          + ",";
  json += "\"spo2\":"         + String(g_spo2)         + ",";
  json += "\"temperature\":"  + String(g_temp, 2)      + ",";  // 2 decimal places
  json += "\"ecgVoltage\":"   + String(g_ecgVolt, 3)   + ",";  // 3 decimal places (millivolt precision)
  json += "\"ecgRaw\":"       + String(g_ecgRaw)       + ",";
  json += String("\"leadsOff\":")  + (g_leadsOff  ? "true" : "false") + ",";
  json += String("\"validHR\":")   + (g_validHR   ? "true" : "false") + ",";
  json += String("\"validSPO2\":") + (g_validSPO2 ? "true" : "false") + ",";
  json += "\"activeSensor\":" + String(activeSensor);
  json += "}";

  // Send HTTP response headers followed by the JSON body
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println("Content-Length: " + String(json.length()));
  client.println(); // Blank line separates headers from body (HTTP spec)
  client.print(json);
}


// ====================================================================
//  FUNCTION: sendOK(WiFiClient& client)
//
//  Sends a simple JSON acknowledgement in response to a /cmd request.
//  Confirms to the Android app that the mode was changed successfully
//  and echoes back the new activeSensor value.
// ====================================================================
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


// ====================================================================
//  FUNCTION: handleClient(WiFiClient& client)
//
//  Parses an incoming HTTP request and routes it to the appropriate
//  handler function (sendJSON or sendOK).
//
//  PARSING APPROACH:
//  Only the first line of the HTTP request (the "request line") is
//  needed to determine the endpoint and query parameters. The
//  remaining header lines are drained (read and discarded) so the
//  client-side TCP stack does not stall waiting for a response.
//
//  Supported routes:
//    GET /data        → sendJSON()
//    GET /cmd?sensor= → update activeSensor, sendOK()
//    (anything else)  → plain-text usage message
// ====================================================================
void handleClient(WiFiClient& client) {
  String req = "";
  unsigned long t = millis();

  // Read the HTTP request line (ends at the first '\n' character)
  while (client.connected() && millis() - t < 1000) {
    if (client.available()) {
      char c = client.read();
      req += c;
      if (c == '\n') break;
    }
  }

  // Drain remaining HTTP headers (Authorization, User-Agent, etc.)
  // so the client does not wait for us to consume them
  while (client.connected() && millis() - t < 1500) {
    if (client.available()) client.read();
    else break;
  }

  // ── Route: GET /data ─────────────────────────────────────────────
  if (req.indexOf("GET /data") >= 0) {
    sendJSON(client);

  // ── Route: GET /cmd ──────────────────────────────────────────────
  } else if (req.indexOf("GET /cmd") >= 0) {
    // Parse the sensor= query parameter and update the active mode
    if      (req.indexOf("sensor=all")  >= 0) { activeSensor = 0; resetAll(); }
    else if (req.indexOf("sensor=hr")   >= 0) { activeSensor = 1; resetAll(); }
    else if (req.indexOf("sensor=spo2") >= 0) { activeSensor = 2; resetAll(); }
    else if (req.indexOf("sensor=temp") >= 0) { activeSensor = 3; resetAll(); }
    else if (req.indexOf("sensor=ecg")  >= 0) { activeSensor = 4; resetAll(); }

    // Immediately update the HR sensor's software state to reflect
    // the new mode. Modes 0, 1, and 2 need the MAX30102; modes 3
    // and 4 do not.
    bool hrNeeded = (activeSensor == 0 || activeSensor == 1 || activeSensor == 2);
    setHRSensorState(hrNeeded);

    Serial.print("[MODE] Changed to activeSensor = ");
    Serial.println(activeSensor);
    sendOK(client);

  // ── Route: Unknown / Default ─────────────────────────────────────
  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("Smart Health Box — Arduino Nano 33 IoT");
    client.println("Team: SIDEQUEST");
    client.println("Available endpoints:");
    client.println("  GET /data");
    client.println("  GET /cmd?sensor=all|hr|spo2|temp|ecg");
  }

  delay(1);
  client.stop(); // Close the TCP connection (stateless HTTP)
}


// ====================================================================
//  FUNCTION: resetAll()
//
//  Resets all shared readings and timing counters to their default
//  (zero / false) state. Called whenever the active sensor mode
//  changes so the Android app receives fresh data immediately rather
//  than stale values from the previous mode.
// ====================================================================
void resetAll() {
  g_hr = 0;  g_spo2 = 0;  g_temp = 0.0;
  g_ecgVolt = 0.0;  g_ecgRaw = 0;
  g_validHR = false;  g_validSPO2 = false;
  lastHRRead = 0;  lastTempRead = 0;
}


// ====================================================================
//  FUNCTION: setup()
//
//  Runs once on power-up or after a hardware reset. Initialises all
//  peripherals in the following order:
//    1. Serial port (for debugging via USB)
//    2. I²C bus
//    3. AD8232 leads-off detection pins
//    4. MAX30205 temperature sensor (verified with an initial read)
//    5. MAX30102 heart rate / SpO₂ sensor
//    6. Wi-Fi connection and HTTP server
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Allow the Serial Monitor to open before printing

  // ── 1. I²C Bus Initialisation ───────────────────────────────────
  // Wire.begin() configures the Nano 33 IoT's SDA (A4) and SCL (A5)
  // pins as the I²C master. Both MAX30102 and MAX30205 share this bus.
  Wire.begin();

  // ── 2. AD8232 Leads-Off Pin Configuration ───────────────────────
  // LO+ and LO- are digital outputs from the AD8232 module that go
  // HIGH when an electrode is not making proper skin contact.
  pinMode(LO_PLUS,  INPUT);
  pinMode(LO_MINUS, INPUT);
  Serial.println("[AD8232] Leads-off detection pins configured.");

  // ── 3. MAX30205 Temperature Sensor Initialisation ───────────────
  // We verify the sensor is reachable over I²C by calling
  // readTemperature() directly. This uses raw register access
  // (address 0x4C, register 0x00) and does not rely on the library's
  // begin() call, which avoids a known initialisation timing issue.
  Serial.println("[MAX30205] Checking sensor at I2C address 0x4C...");
  readTemperature();

  if (g_temp > 10.0 && g_temp < 50.0) {
    Serial.print("[MAX30205] Ready. Initial temperature reading: ");
    Serial.print(g_temp, 2);
    Serial.println(" °C");
  } else {
    Serial.println("[WARNING] MAX30205 did not return a valid temperature.");
    Serial.println("          → Verify SDA/SCL wiring and I2C address.");
    Serial.println("          → Use an I2C scanner sketch to confirm 0x4C is detected.");
    // We continue setup — the sensor may warm up and respond on the
    // next poll cycle in loop().
  }

  // ── 4. MAX30102 Heart Rate / SpO₂ Sensor Initialisation ─────────
  // particleSensor.begin() performs an I²C device scan for address
  // 0x57. If the sensor is not found, the firmware halts here with
  // an error. Check wiring if this occurs.
  if (!particleSensor.begin(Wire)) {
    Serial.println("[ERROR] MAX30102 not found on I2C bus!");
    Serial.println("        → Check SDA/SCL connections.");
    Serial.println("        → Verify 3.3V power to the sensor module.");
    while (true) delay(1000); // Halt — cannot continue without HR sensor
  }

  // Sensor configuration parameters for particleSensor.setup():
  //   ledBrightness = 0x3F (63/255) — moderate IR/Red LED power
  //   sampleAverage = 4            — average 4 readings per sample point
  //   ledMode       = 2            — enable both Red and IR LEDs (required for SpO₂)
  //   sampleRate    = 100          — 100 samples per second
  //   pulseWidth    = 411 µs       — ADC integration time (affects resolution)
  //   adcRange      = 4096         — full-scale ADC range
  particleSensor.setup(0x3F, 4, 2, 100, 411, 4096);

  // Disable the Green LED entirely — it is only used for wrist-based
  // heart rate detection and is not needed for finger-based SpO₂.
  particleSensor.setPulseAmplitudeGreen(0);

  hrSensorActive = true; // setup() leaves the MAX30102 in its active state
  Serial.println("[MAX30102] Ready.");

  // ── 5. Wi-Fi and HTTP Server ─────────────────────────────────────
  setupWiFi();
  server.begin();
  Serial.println("[HTTP] Server started.");
  Serial.print("[HTTP] Android app should connect to: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/data");
  Serial.println("----------------------------------------------------");
  Serial.println("  Smart Health Box is running. SIDEQUEST — 2026");
  Serial.println("----------------------------------------------------");
}


// ====================================================================
//  FUNCTION: loop()
//
//  The main execution loop runs continuously after setup() completes.
//  It performs four tasks on every iteration:
//
//    A. ECG sampling (AD8232) — fast, runs every iteration
//    B. Temperature polling (MAX30205) — once per second
//    C. HR & SpO₂ computation (MAX30102) — once every 5 seconds
//    D. HTTP client polling — checks for incoming Android app requests
//
//  TIMING STRATEGY:
//  Tasks A and D are non-blocking and execute on every loop pass.
//  Tasks B and C use millis()-based timers so they do not block the
//  loop. Task C (HR collection) does block for ~1 second while
//  gathering 100 samples, but it services HTTP requests and reads
//  temperature every 10 samples to avoid neglecting other tasks.
// ====================================================================
void loop() {
  unsigned long now = millis();

  // ── A. ECG SAMPLING (AD8232) ─────────────────────────────────────
  // Relevant modes: 0 (all) and 4 (ecg only)
  if (activeSensor == 0 || activeSensor == 4) {

    // Check leads-off status: if either LO pin is HIGH, the electrode
    // has lost skin contact and the ECG signal is noise — discard it.
    g_leadsOff = (digitalRead(LO_PLUS) == HIGH) || (digitalRead(LO_MINUS) == HIGH);

    if (!g_leadsOff) {
      // Read the 10-bit ADC value (0–1023) from pin A0.
      // The Arduino Nano 33 IoT operates at 3.3 V, so the conversion
      // from ADC count to voltage is:
      //   V = (ADC_count / 1023) × 3.3 V
      // NOTE: The divisor is 1023 (not 1024) because a 10-bit ADC
      // produces values from 0 to 1023 — dividing by 1023 maps the
      // maximum count exactly to 3.3 V (the reference voltage).
      g_ecgRaw  = analogRead(ECG_PIN);
      g_ecgVolt = (g_ecgRaw * 3.3f) / 1023.0f;
    } else {
      // Leads are off — zero the output so the app shows a flat line
      // rather than electrode noise
      g_ecgRaw = 0;
      g_ecgVolt = 0.0f;
    }
  } else {
    // ECG is not active in this mode — zero the values
    g_ecgRaw = 0;
    g_ecgVolt = 0.0f;
    g_leadsOff = true;
  }


  // ── B. TEMPERATURE POLLING (MAX30205) ────────────────────────────
  // Relevant modes: 0 (all) and 3 (temp only)
  // Poll interval: once per second (1000 ms)
  if ((activeSensor == 0 || activeSensor == 3) && now - lastTempRead >= 1000) {
    readTemperature();
  } else if (activeSensor != 0 && activeSensor != 3) {
    // Temperature is not relevant in the current mode — clear the value
    g_temp = 0.0;
  }


  // ── C. HEART RATE & SpO₂ (MAX30102) ─────────────────────────────
  // Relevant modes: 0 (all), 1 (hr only), 2 (spo2 only)
  bool hrNeeded = (activeSensor == 0 || activeSensor == 1 || activeSensor == 2);
  setHRSensorState(hrNeeded);

  if (hrNeeded) {
    // Quick IR signal check — if the reading is below 50,000 counts,
    // no finger is placed on the sensor. Skip computation.
    long irValue = particleSensor.getIR();

    if (irValue < 50000) {
      // No finger detected — return zeros to the app
      g_hr = 0;  g_spo2 = 0;
      g_validHR = false;  g_validSPO2 = false;

    } else if (now - lastHRRead >= 5000) {
      // ── Sample Collection Phase ────────────────────────────────
      // Collect 100 samples from the MAX30102's Red and IR channels.
      // At 100 Hz (set in setup), this takes approximately 1 second.
      //
      // During this loop:
      //   - HTTP requests are serviced every sample (every ~10 ms)
      //   - Temperature is read every 10 samples (~every 100 ms) when
      //     in "all sensors" mode (activeSensor == 0)
      for (byte i = 0; i < 100; i++) {
        // Wait until the sensor's FIFO has a new sample available
        while (!particleSensor.available()) particleSensor.check();

        // Store the Red and IR counts in their respective buffers
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample(); // Advance the FIFO read pointer

        // Service any Android app HTTP requests that arrived during
        // this sample-collection period
        WiFiClient c = server.available();
        if (c) handleClient(c);

        // Interleave temperature reads during HR sample collection.
        // Every 10th sample (~100 ms), the firmware yields briefly to
        // the MAX30205. This prevents a full 1-second I²C blackout
        // that would otherwise cause the temperature sensor to fail.
        // Only applies when all sensors are active (mode 0).
        if (activeSensor == 0 && i % 10 == 0) {
          readTemperature();
        }

        delay(10); // 10 ms between samples → ~100 Hz effective sample rate
      }

      // ── HR and SpO₂ Computation ────────────────────────────────
      // maxim_heart_rate_and_oxygen_saturation() is Maxim Integrated's
      // official algorithm (from spo2_algorithm.h). It analyses the
      // 100-sample IR and Red buffers using the Beer-Lambert Law
      // (ratio-of-ratios method) to compute SpO₂, and detects
      // heartbeat peaks in the IR signal to compute HR.
      //
      // THIS IS NOT PAN-TOMPKINS.
      // Pan-Tompkins is a QRS detection algorithm for ECG signals.
      // The MAX30102 uses PPG (photoplethysmography), which requires
      // a different approach. Maxim's algorithm is purpose-built for
      // this sensor and is validated against clinical-grade oximeters.
      maxim_heart_rate_and_oxygen_saturation(
        irBuffer, 100, redBuffer,
        &spo2Value, &validSPO2, &heartRateValue, &validHeartRate
      );

      // ── Sanity Checks ─────────────────────────────────────────
      // Even when the algorithm reports a result as valid (flag == 1),
      // physiologically impossible values are rejected:
      //   HR    : valid range 20–300 BPM (covers bradycardia to extreme tachycardia)
      //   SpO₂  : valid range 50–100%    (below 50% is not physiologically survivable)
      g_validHR   = (validHeartRate == 1) && (heartRateValue > 20)  && (heartRateValue < 300);
      g_validSPO2 = (validSPO2      == 1) && (spo2Value      > 50)  && (spo2Value      <= 100);

      // Only publish values that passed the sanity check
      g_hr   = g_validHR   ? (int)heartRateValue : 0;
      g_spo2 = g_validSPO2 ? (int)spo2Value      : 0;
      lastHRRead = millis();

      // Print a summary line to the Serial Monitor for live debugging
      Serial.print("[DATA] HR: ");   Serial.print(g_hr);
      Serial.print(" BPM  |  SpO2: "); Serial.print(g_spo2);
      Serial.print("%  |  Temp: ");   Serial.print(g_temp, 1);
      Serial.print(" C  |  ECG: ");   Serial.print(g_ecgVolt, 2);
      Serial.print(" V  |  Mode: ");  Serial.println(activeSensor);
    }
  } else {
    // HR/SpO₂ not active in this mode — clear values
    g_hr = 0;  g_spo2 = 0;
    g_validHR = false;  g_validSPO2 = false;
  }


  // ── D. HTTP CLIENT POLL ───────────────────────────────────────────
  // Check once per main loop iteration whether the Android app has
  // sent a new HTTP request. This covers requests that arrive between
  // HR sample collection windows.
  WiFiClient client = server.available();
  if (client) handleClient(client);
}

// ====================================================================
//  END OF FILE
//  Smart Health Box Firmware — SIDEQUEST — IUA 2026
// ====================================================================
