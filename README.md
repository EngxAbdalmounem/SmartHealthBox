# 🏥 Smart Health Box

> A portable, low-cost IoT-based physiological monitoring system  
> **BSc Thesis — International University of Africa, Faculty of Engineering, 2026**

---

## 📌 Overview

The **Smart Health Box** is a compact, affordable device designed to monitor essential vital signs in real time. It was developed to address the lack of continuous health monitoring tools in rural and remote areas of Sudan, where advanced medical equipment is scarce and costly.

The system captures ECG signals, heart rate, blood oxygen saturation (SpO₂), and body temperature — wirelessly transmitting all data via Wi-Fi over HTTP to a dedicated Android application for real-time visualization and data logging.

---

## 👥 Authors

| Name | Role |
|---|---|
| Abdalmounem Abdalhameed |Firmware, Thesis , Hardware , prototype |
| Ahmed Sami Ahmed |Firmware, Thesis, Mobile Application (React Native / Expo) |

**Supervisor:** Dr. Nisreen  
**Institution:** International University of Africa — Department of Electrical and Electronic Engineering

---

## 🔧 Hardware Components

| Component | Function |
|---|---|
| Arduino Nano 33 IoT | Main microcontroller + Wi-Fi |
| AD8232 | ECG signal acquisition |
| MAX30102 | Heart rate & SpO₂ (PPG) |
| MAX30205 | Body temperature (I²C) |
| USB Power Bank | Portable power supply |

---

## 📁 Repository Structure

```
SmartHealthBox/
│
├── firmware/
│   └── health_box.ino          # Arduino firmware (C++)
│
├── mobile-app/
│   ├── src/                    # Screen and component source files
│   ├── assets/                 # Images and icons
│   ├── App.js                  # Main application entry point
│   ├── app.json                # Expo configuration
│   ├── babel.config.js         # Babel configuration
│   ├── eas.json                # Expo EAS build config
│   └── package.json            # Dependencies
│
├── docs/
│   └── circuit_diagram.png     # EasyEDA circuit schematic
│
└── README.md
```

---

## 📡 System Architecture

```
[Sensors] → [Arduino Nano 33 IoT] → [Wi-Fi / HTTP] → [Android App]
  AD8232        Firmware: C++           Static IP        React Native
  MAX30102       Signal Processing      Port: 80         Real-time Display
  MAX30205       JSON Payload                            Data Logging
```

**Communication:** The Arduino hosts an HTTP server at a static IP address. The Android app sends requests to the `/cmd` endpoint and receives a nine-field JSON payload containing all sensor readings.

---

## 📊 Monitored Parameters

| Parameter | Sensor | Method |
|---|---|---|
| ECG Signal | AD8232 | Analog acquisition, 500 Hz sampling |
| Heart Rate (BPM) | MAX30102 | Photoplethysmography (PPG) |
| SpO₂ (%) | MAX30102 | Red/IR light ratio (Beer-Lambert) |
| Body Temperature (°C) | MAX30205 | Digital I²C, ±0.1°C accuracy |

---

## 📱 Mobile Application

- Built with **React Native** and **Expo**
- Android platform
- Connects to the Arduino via **static IP over local Wi-Fi**
- Features: real-time visualization, data logging, multi-mode sensor display

---

## ⚙️ Firmware Notes

- ADC resolution: **10-bit** (0–1023)
- ECG sampling rate: **500 Hz**
- SpO₂/HR algorithm: Maxim's onboard `maxim_heart_rate_and_oxygen_saturation()` function
- Noise filtering: analog and digital filters applied
- Communication: HTTP server on Arduino, static IP via `WiFi.config()`

---

## 🚀 How to Use

### Firmware
1. Open `firmware/health_box.ino` in Arduino IDE
2. Install required libraries: `WiFiNINA`, `MAX30105`, `Protocentral_MAX30205`
3. Set your Wi-Fi credentials and static IP in the code
4. Upload to Arduino Nano 33 IoT

### Mobile App
1. Navigate to `mobile-app/`
2. Run `npm install`
3. Run `npx expo start`
4. Connect your Android device on the same Wi-Fi network
5. Enter the Arduino's static IP address in the app settings

---

## 📄 License

This project was developed for academic purposes as part of a BSc thesis at the International University of Africa. All rights reserved by the authors.

---

*Smart Health Box — Bridging the diagnostic gap in resource-limited environments.*
