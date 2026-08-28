# 🛡️ WatchPod v1.3 : Smart Home Energy & Security Assistant

WatchPod is a custom-built, modular, local-first IoT smart home node developed entirely from scratch. Its primary objective is to intelligently analyze room occupancy and lighting context to prevent energy waste, while acting as a 24/7 intrusion detection system when the house is empty.

**Developed by:** Bhupinderjot Singh Rai  
**Current Release:** WatchPod v1.3  
**Development Stack:** ESP32 (C++) | Mobile-first development via ArduinoDroid

---

## 🌟 Why WatchPod?

Most basic PIR-based systems rely on simple motion timeouts. This can create false "empty room" situations when a person is resting, reading, or temporarily inactive.

WatchPod approaches the problem differently by combining motion activity with lighting context.

It acts as an **Energy-Aware Occupancy Assistant** for everyday household use, while providing a dedicated **Vacation Security Mode** when the house is empty.

---

## ⚙️ Core Features

### 🧠 Smart Activity Score

Instead of relying on a single motion event, WatchPod tracks PIR activity through a **5-minute sliding window** divided into 10 × 30-second intervals.

This produces an **Activity Score (0–10)** that represents recent room activity.

Low activity can indicate a resting or temporarily inactive user, helping reduce unnecessary energy alerts.

---

### 🌿 Energy Saver Mode

**Default operating mode.**

WatchPod continuously evaluates:

- Recent room activity
- PIR motion state
- LDR light state
- How long the room has remained inactive

When the room appears empty while the light remains ON, WatchPod uses a graduated alert system:

**2-minute Soft Warning → 10-minute Strong Alert**

---

### 🚨 Vacation Security Mode

Manual security mode designed for situations when the house is empty.

When Vacation Mode is active, PIR motion can trigger a Telegram security alert.

A cooldown mechanism helps prevent repeated alerts from continuously firing during the same event.

---

### 📡 Wi-Fi Recovery

WatchPod includes automatic Wi-Fi reconnection handling.

If the Wi-Fi connection is lost, WatchPod continues running its local sensing logic and periodically attempts to reconnect.

---

### 📱 Telegram Control

WatchPod integrates with the Telegram Bot API for remote monitoring and control.

The Telegram interface provides:

- Live system status
- Energy Saver control
- Vacation Security control
- Time information
- Language selection
- Security alerts
- Energy-saving alerts

---

### 🔐 Telegram Access Control

WatchPod uses **Chat ID Whitelisting**.

Only the configured Telegram Chat ID is authorized to control the device.

Unauthorized Chat IDs are silently ignored.

> Never publish your Telegram Bot Token, Wi-Fi password, or private `config.h` file.

---

### 💾 Power-Cut Resilience

WatchPod uses the ESP32's **Non-Volatile Storage (NVS)** to retain important settings.

The device can remember:

- Active operating mode
- Selected interface language

This allows the system to restore its saved configuration after a restart or power interruption.

---

### 🌍 Multi-Language Interface

Telegram interface supports:

- 🇬🇧 English
- 🇮🇳 Hindi
- ਪੰਜਾਬੀ Punjabi

Language selection is available through `/lang`.

---

## 🆕 What's New in v1.2

WatchPod v1.2 focuses on improving reliability and making the Telegram monitoring experience more useful while preserving the core v1.1 functionality.

### v1.2 improvements

- 📡 Automatic Wi-Fi reconnection
- 🔄 Improved network recovery handling
- 📱 Improved Telegram status information
- 🚨 Improved Vacation Security alerts
- 💡 Improved Energy Saver alerts
- 🧠 Existing 5-minute Activity Score preserved
- 💾 NVS configuration persistence preserved
- 🌍 English / Hindi / Punjabi support preserved
- 🔐 Telegram Chat ID authorization preserved
- 💡 PIR activity LED indication
- 🧪 Tested on real ESP32 hardware

**Current status: 🟢 Stable / Working Prototype**

---

## 🆕 What's New in v1.3

WatchPod v1.3 focuses on reliability, observability, and practical sensor calibration while keeping the project bounded and hardware-focused.

### v1.3 improvements

- 🧪 `/calibrate` command for LDR ambient-light calibration
- 💾 Calibrated LDR baseline stored in ESP32 NVS
- 📡 Wi-Fi RSSI added to `/status`
- 🧠 Live Activity Score shown in `/status`
- 💾 Free heap memory shown in `/status`
- 🔄 Telegram duplicate-update protection
- 🔔 Telegram reboot notification with the NVS-restored operating mode
- 🛡️ Unauthorized Telegram users are silently ignored
- 🐕 Hardware watchdog for automatic recovery from a stalled main loop
- 📡 Existing automatic Wi-Fi reconnection preserved
- 🌍 Existing English / Hindi / Punjabi support preserved
- 💡 Existing PIR activity LED indication preserved

### v1.3 power-management scope

WatchPod v1.3 does **not** include Deep Sleep, Low Power mode, `/lowpower`, or `/normalpower`.

The device remains remotely controllable through Telegram during normal operation.

---

## 🔌 Hardware & Pinout

| Component | Connection |
|---|---|
| ESP32 | Main controller |
| HC-SR501 PIR OUT | GPIO 32 |
| HC-SR501 PIR VCC | 5V |
| HC-SR501 PIR GND | GND |
| LDR Analog Output | GPIO 34 (ADC) |
| LDR VCC | 3.3V |
| LDR GND | GND |
| LED (+) | GPIO 15 through 220Ω resistor |
| LED (−) | GND |

### Hardware Notes

- PIR sensor is used for motion/activity detection.
- LDR is used for light-state monitoring.
- LED provides local activity indication.
- All components share a common ground.
- Use an appropriate resistor with the LED.

---

## 🛠️ Bill of Materials (BOM)

All components are standard, low-cost DIY electronics.

- **Microcontroller:** ESP32 Development Board (Wi-Fi enabled)
- **Motion Sensor:** HC-SR501 PIR Sensor
- **Light Sensor:** Analog LDR Module
- **Visual Feedback:** 5mm LED with a 220Ω resistor
- **Misc:** Breadboard, Jumper Wires, Micro-USB cable
- **Current Enclosure:** Upcycled cardboard prototype (Alpha Build)

---

## 📐 Circuit Diagram

The circuit diagram documents the current WatchPod hardware connections.

> Circuit diagram will be added to the repository documentation.

---

## 🧠 System Logic

### Energy Saver Mode

```text
PIR + LDR
   ↓
ESP32
   ↓
Calculate Activity Score
   ↓
Is room inactive?
   ↓
Is light ON?
   ↓
Start empty-room timer
   ↓
2 minutes → Soft Warning
   ↓
10 minutes → Strong Alert
```

### Vacation Security Mode

```text
PIR Motion
    ↓
ESP32
    ↓
Vacation Mode active?
    ↓
YES
    ↓
Security Alert
    ↓
Telegram
```

---

## 📲 Telegram Command Dashboard

| Command | Function |
|---|---|
| `/start` | Open WatchPod command interface |
| `/status` | View live system status, Wi-Fi RSSI, free heap, sensor states and Activity Score |
| `/energy` | Activate Smart Bedroom Energy Saver |
| `/vacation` | Activate Manual Vacation Security Mode |
| `/time` | View system time and active mode |
| `/lang` | Change interface language |
| `/calibrate` | Calibrate the LDR using the current ambient light level |

---

## 🔐 Configuration

The public `WatchPod_v1.3.ino` source uses placeholders for private credentials.

Before uploading the firmware, replace these four values locally:

```cpp
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define TELEGRAM_BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define TELEGRAM_CHAT_ID "YOUR_TELEGRAM_CHAT_ID"
```

### ⚠️ Security

Never commit your real:

- Wi-Fi password
- Telegram Bot Token
- Telegram Chat ID

to a public repository.

Keep your personal credentials only in your local working copy.

---

## 📱 Development

WatchPod is developed using:

**ESP32 + Arduino/C++ + ArduinoDroid**

The project follows a mobile-first development workflow, allowing firmware development and compilation without requiring a traditional desktop computer.

---

## 🧪 Testing Status

WatchPod v1.3 has been:

- ✅ Compiled successfully
- ✅ Uploaded to a real ESP32
- ✅ Connected to Wi-Fi
- ✅ Tested with Telegram
- ✅ Tested with PIR motion detection
- ✅ Tested with LDR light monitoring
- ✅ Tested with Energy Saver Mode
- ✅ Tested with Vacation Security Mode
- ✅ Tested with Telegram commands
- ✅ Tested with LDR calibration
- ✅ Tested with persistent NVS settings
- ✅ Tested with v1.3 Telegram control

**Current status: Working real-hardware prototype.**

---

## 📂 Repository Structure

```text
WatchPod-SmartHome/
│
├── WatchPod_v1.3.ino
├── .gitignore
├── README.md
│
└── docs/
    ├── circuit-diagram.png
    ├── system-architecture.png
    └── flowchart.png
```

> Private credentials are intentionally excluded from the public repository.

---

## 🚀 Future Roadmap

### v1.4+ — Future Development

Future improvements will be evaluated only after the v1.3 stable build.

Possible areas include:

- Smarter occupancy detection
- Better environmental awareness
- Hardware refinements
- Additional sensing capabilities

### v2.0 — Long-Term Hardware Roadmap

1. **mmWave Radar Upgrade**  
   Replacing or fusing PIR with an HLK-LD2410 mmWave sensor to detect stationary micro-movements and improve occupancy detection.

2. **Optical LDR Baffling**  
   Adding a physical directional tube/light pipe to reduce sunlight interference and focus the LDR on artificial lighting.

3. **3D Printed Enclosure**  
   Moving from the current cardboard prototype to a sleek, wall-mountable CAD-designed enclosure.

---

## 🔒 Privacy Philosophy

WatchPod is designed around a **local-first approach**.

The device uses:

- ESP32 local processing
- Direct Wi-Fi connectivity
- Telegram for remote notifications
- No mandatory cloud subscription
- No camera
- No continuous audio recording

---

## 🏆 Project Status

**WatchPod v1.3 — Stable Working Prototype**

The system has progressed from a software concept to a functional ESP32-based real-hardware prototype with persistent configuration, calibration, remote monitoring, and automatic recovery.

Future development will focus on improving sensing accuracy, reliability, hardware design, and usability without compromising the project's privacy-first approach.

---

*Built for smarter homes. Zero cloud subscriptions. 100% Privacy.*

