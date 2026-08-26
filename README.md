# 🛡️ WatchPod v1.0 : Smart Home Energy & Security Assistant

WatchPod is a custom-built, modular, local-first IoT smart home node developed entirely from scratch. Its primary objective is to intelligently analyze room occupancy and lighting context to prevent energy waste, while acting as a hardcore 24/7 intrusion detection system when the house is empty.

**Developed by:** Bhupinderjot Singh Rai  
**Development Stack:** ESP32 (C++) | Mobile-first compilation via ArduinoDroid  

---

## 🌟 Why WatchPod?
Most commercial and DIY PIR sensors are naive: they rely on simple timeouts, causing false "empty room" triggers when you are reading or sleeping, and annoying security alerts when you get up for water at night. WatchPod fixes this by acting as an **Energy-Aware Occupancy Assistant** designed specifically for real-world household dynamics.

## ⚙️ Core Features

*   **🧠 Smart Activity Score (Sliding Window Algorithm):** 
    Instead of a binary motion timeout, WatchPod tracks triggers over a 5-minute sliding window to generate an `Activity Score (0-10)`. This acts as an advanced software debounce—low scores imply a resting user (sleeping/reading), actively suppressing false "lights off" alerts.
*   **🌿 Energy Saver Mode (Default):** 
    Continuously monitors context. Employs a graduated alert system if a room is empty and lights are left ON (2-min Soft Warning → 10-min Strong Reminder).
*   **🚨 Vacation Mode (Manual Security):** 
    24/7 hardcore intrusion monitoring. Triggered manually only when the house is empty, eliminating night-time false alarms.
*   **🔐 Bulletproof Telegram Security:** 
    Direct integration with the Telegram Bot API. Features **Chat ID Whitelisting** at the code level—if anyone else discovers the Bot Token, WatchPod actively rejects their commands with an "Unauthorized access" prompt.
*   **💾 Power-Cut Resilience:** 
    Utilizes ESP32's Non-Volatile Storage (NVS). If a power outage occurs, WatchPod remembers its last active mode and seamlessly resumes duty upon reboot.
*   **🌍 Multi-Language Interface:** 
    Full Telegram UI support in English, Hindi, and Punjabi.

---

## 🛠️ Bill of Materials (BOM)
All components are standard, low-cost DIY electronics (easily sourced from platforms like Robu.in or local electronics markets):
*   **Microcontroller:** ESP32 Development Board (WiFi enabled)
*   **Motion Sensor:** HC-SR501 PIR Sensor
*   **Light Sensor:** Analog LDR Module
*   **Visual Feedback:** 5mm LED with a 220Ω resistor
*   **Misc:** Breadboard, Jumper Wires, Micro-USB for power
*   **Current Enclosure:** Upcycled cardboard prototype (Alpha Build)

---

## 📲 Telegram Command Dashboard
*   `/status` - View live sensor states, light status, and current Activity Score.
*   `/energy` - Activate Smart Bedroom Energy Saver (Default).
*   `/vacation` - Activate Manual Security (24/7 Vacation Mode).
*   `/time` - View IST time sync and mode info.
*   `/lang` - Change UI language (EN/HI/PA).

---

## 🚀 Future Roadmap (v2.0)
While v1.0 maximizes the potential of basic sensors through software logic, future iterations will focus on hardware-level precision:
1.  **mmWave Radar Upgrade:** Replacing/Fusing the PIR with an HLK-LD2410 mmWave sensor to detect stationary micro-movements (breathing), fully solving the "sleeping user" limitation.
2.  **Optical LDR Baffling:** Adding a physical directional tube (light pipe) to the LDR to prevent sunlight interference and focus strictly on artificial ceiling light.
3.  **3D Printed Enclosure:** Moving from the cardboard prototype to a sleek, wall-mountable CAD-designed chassis modeled in Onshape.

---
*Built for smarter homes. Zero cloud subscriptions. 100% Privacy.*
