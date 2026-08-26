/*
====================================================
                 WATCHPOD v1.0
          SMART BEDROOM/HOME FINAL VERSION

            * Default: Energy Saver (24/7 Smart Context)
            * Optional: Vacation Security (24/7 Override)

Hardware:
PIR OUT  -> GPIO 32
PIR VCC  -> 5V
PIR GND  -> GND

LDR AO   -> GPIO 34
LDR VCC  -> 3.3V
LDR GND  -> GND

LED +    -> GPIO 15 (with resistor)
LED -    -> GND

Commands for BotFather:
 status - Check live system status & Activity Score
 energy - Smart Bedroom Energy Saver (Default)
 vacation - Manual Security Mode (Use when house is empty)
 time - View current time & active mode status
 lang - Change language (EN/HI/PA)
====================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// ==========================================
// 1. NETWORK & TELEGRAM CREDENTIALS
// ==========================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#define BOTtoken "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID "YOUR_TELEGRAM_CHAT_ID"

// ==========================================
// 2. HARDWARE PIN DEFINITIONS
// ==========================================
const int PIR_PIN = 32;   // Digital input from HC-SR501
const int LDR_PIN = 34;   // Analog input from LDR module
const int LED_PIN = 15;   // Visual status / motion feedback LED

// ==========================================
// 3. SYSTEM CONSTANTS & THRESHOLDS
// ==========================================
const int LIGHT_THRESHOLD = 1000;             // Below 1000 = Light ON
const unsigned long BOT_MTBS = 1000;           // Mean time between Telegram scans (1s)
const unsigned long WINDOW_INTERVAL = 30000;   // 30 seconds per slice (Total 5 mins = 10 slices)
const unsigned long SOFT_ALERT_TIME = 120000;  // 2 minutes empty room timer
const unsigned long HARD_ALERT_TIME = 600000;  // 10 minutes empty room timer

// ==========================================
// 4. STATE VARIABLES
// ==========================================
enum SystemMode { 
  ENERGY_SAVER, 
  VACATION 
};

SystemMode currentMode = ENERGY_SAVER;
String currentLanguage = "EN"; // Supported: EN, HI, PA

int windowPIR[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int windowIndex = 0;
int activityScore = 0;

unsigned long lastBotScan = 0;
unsigned long lastWindowShift = 0;
unsigned long emptyRoomSince = 0;

bool softAlertSent = false;
bool hardAlertSent = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
Preferences preferences;

// ==========================================
// 5. HELPER FUNCTIONS
// ==========================================

// Multi-language string selector
String getMsg(String en, String hi, String pa) {
  if (currentLanguage == "HI") return hi;
  if (currentLanguage == "PA") return pa;
  return en;
}

// Telegram message dispatcher
void sendTelegram(String message) {
  bot.sendMessage(CHAT_ID, message, "");
}

// Formatted IST time generator
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time Sync Error";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d-%m-%Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// Sliding window sum calculator (0 to 10)
void updateActivityScore() {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += windowPIR[i];
  }
  activityScore = sum;
  Serial.print("[SYSTEM] Activity Score updated: ");
  Serial.print(activityScore);
  Serial.println("/10");
}

// ==========================================
// 6. TELEGRAM MESSAGE HANDLER
// ==========================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String senderChatId = String(bot.messages[i].chat_id);
    
    // Security check: Whitelist sender
    if (senderChatId != CHAT_ID) {
      Serial.print("[SECURITY] Unauthorized access attempt from Chat ID: ");
      Serial.println(senderChatId);
      bot.sendMessage(senderChatId, "Unauthorized Access Denied.", "");
      continue;
    }

    String text = bot.messages[i].text;
    Serial.print("[TELEGRAM] Received command: ");
    Serial.println(text);

    int currentPir = digitalRead(PIR_PIN);
    int currentLdr = analogRead(LDR_PIN);

    if (text == "/start") {
      String welcome = getMsg(
        "Welcome to WatchPod v1.0!\n\nCommands:\n/status - Check live system status & Activity Score\n/energy - Smart Bedroom Energy Saver (Default)\n/vacation - Manual Security Mode\n/time - View current time & active mode status\n/lang - Change language",
        "WatchPod v1.0 mein aapka swagat hai!\n\nCommands:\n/status - Live sthiti aur Activity Score dekhein\n/energy - Smart Bedroom Energy Saver (Default)\n/vacation - Manual Security Mode\n/time - Samay aur active mode dekhein\n/lang - Bhasha badlein",
        "WatchPod v1.0 vich tuhada swagat hai!\n\nCommands:\n/status - Live sthiti te Activity Score dekho\n/energy - Smart Bedroom Energy Saver (Default)\n/vacation - Manual Security Mode\n/time - Samaa te active mode dekho\n/lang - Bhasha badlo"
      );
      sendTelegram(welcome);
    } 
    else if (text == "/status") {
      String ldrState = (currentLdr < LIGHT_THRESHOLD) ? "LIGHT ON" : "LIGHT OFF";
      String pirState = (currentPir == HIGH) ? "MOTION" : "CLEAR";
      String modeStr = (currentMode == VACATION) ? "VACATION" : "ENERGY SAVER";

      String msg = "WATCHPOD STATUS\n===\n";
      msg += "SYSTEM : ONLINE\n";
      msg += "MODE   : " + modeStr + "\n\n";
      msg += "TIME   : " + getFormattedTime() + "\n\n";
      msg += "PIR    : " + pirState + "\n";
      msg += "LDR    : " + String(currentLdr) + " (" + ldrState + ")\n";
      msg += "ACTIVITY SCORE: " + String(activityScore) + "/10 (5m window)";

      sendTelegram(msg);
    } 
    else if (text == "/time") {
      String modeStr = (currentMode == VACATION) ? "VACATION" : "ENERGY SAVER";
      String msg = getMsg(
        "System Time: " + getFormattedTime() + "\nActive Mode: " + modeStr,
        "System Samay: " + getFormattedTime() + "\nActive Mode: " + modeStr,
        "System Samaa: " + getFormattedTime() + "\nActive Mode: " + modeStr
      );
      sendTelegram(msg);
    }
    else if (text == "/energy") {
      currentMode = ENERGY_SAVER;
      preferences.putUInt("mode", ENERGY_SAVER);
      sendTelegram(getMsg(
        "Mode switched to Energy Saver.", 
        "Mode badal kar Energy Saver kar diya gaya hai.", 
        "Mode badal ke Energy Saver kar ditta gaya hai."
      ));
    } 
    else if (text == "/vacation") {
      currentMode = VACATION;
      preferences.putUInt("mode", VACATION);
      sendTelegram(getMsg(
        "Mode switched to Vacation Security.", 
        "Mode badal kar Vacation Security kar diya gaya hai.", 
        "Mode badal ke Vacation Security kar ditta gaya hai."
      ));
    } 
    else if (text == "/lang") {
      String keyboardJson = "[[\"English\", \"Hindi\", \"Punjabi\"]]";
      bot.sendMessageWithReplyKeyboard(
        CHAT_ID, 
        "Choose Language / Bhasha chunein / Bhasha chuno:", 
        "", 
        keyboardJson, 
        true
      );
    } 
    else if (text == "English") {
      currentLanguage = "EN";
      preferences.putString("lang", "EN");
      sendTelegram("Language updated to English.");
    } 
    else if (text == "Hindi") {
      currentLanguage = "HI";
      preferences.putString("lang", "HI");
      sendTelegram("Bhasha badal kar Hindi kar di gayi hai.");
    } 
    else if (text == "Punjabi") {
      currentLanguage = "PA";
      preferences.putString("lang", "PA");
      sendTelegram("Bhasha badal ke Punjabi kar ditti gayi hai.");
    }
  }
}

// ==========================================
// 7. SETUP ROUTINE
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Initializing WatchPod Core...");

  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Load persistent configurations from NVS
  preferences.begin("watchpod", false);
  currentMode = (SystemMode)preferences.getUInt("mode", ENERGY_SAVER);
  currentLanguage = preferences.getString("lang", "EN");

  Serial.println("[NVS] Loaded saved configurations:");
  Serial.print(" - Mode: "); Serial.println((currentMode == VACATION) ? "VACATION" : "ENERGY_SAVER");
  Serial.print(" - Language: "); Serial.println(currentLanguage);

  // Connect to local WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  Serial.print("[WIFI] Connecting to ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Blink indicator while connecting
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n[WIFI] Connected successfully!");
  Serial.print("[WIFI] IP Address: ");
  Serial.println(WiFi.localIP());

  // Synchronize system time with IST (UTC +5:30 = 19800 seconds offset)
  configTime(19800, 0, "pool.ntp.org");
  Serial.println("[NTP] Syncing network time...");

  sendTelegram(getMsg(
    "WatchPod Online & Synced.", 
    "WatchPod Online aur Synced hai.", 
    "WatchPod Online te Synced hai."
  ));
}

// ==========================================
// 8. MAIN EXECUTION LOOP
// ==========================================
void loop() {
  int pirValue = digitalRead(PIR_PIN);
  int ldrValue = analogRead(LDR_PIN);
  bool isLightOn = (ldrValue < LIGHT_THRESHOLD);

  // Hardware motion visualization
  if (pirValue == HIGH) {
    windowPIR[windowIndex] = 1;
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  // Sliding Window: Shift index every 30 seconds
  if (millis() - lastWindowShift > WINDOW_INTERVAL) {
    lastWindowShift = millis();
    windowIndex = (windowIndex + 1) % 10;
    windowPIR[windowIndex] = 0; // Reset slice for next monitoring interval
    updateActivityScore();
  }

  // Operational Mode Logic
  if (currentMode == VACATION) {
    if (pirValue == HIGH) {
      Serial.println("[ALERT] Intrusion detected in Vacation Mode!");
      sendTelegram(getMsg(
        "[SECURITY ALERT] Motion detected in Vacation Mode!",
        "[SECURITY ALERT] Vacation Mode mein halchal detect hui!",
        "[SECURITY ALERT] Vacation Mode vich halchal detect hoyi!"
      ));
      delay(5000); // Debounce trigger to prevent message flood
    }
  } 
  else { // ENERGY_SAVER Mode
    if (activityScore == 0 && isLightOn) {
      if (emptyRoomSince == 0) {
        emptyRoomSince = millis();
        Serial.println("[TIMER] Room empty with lights ON. Tracking timer started.");
      }

      unsigned long emptyDuration = millis() - emptyRoomSince;

      // 2-Minute Soft Reminder
      if (emptyDuration > SOFT_ALERT_TIME && !softAlertSent) {
        Serial.println("[ALERT] Sending 2-min Soft Reminder.");
        sendTelegram(getMsg(
          "[REMINDER] Room appears unoccupied, but lights are ON.",
          "[REMINDER] Kamre mein koi nahi hai, lekin light ON hai.",
          "[REMINDER] Kamre vich koi nahi hai, par light ON hai."
        ));
        softAlertSent = true;
      }

      // 10-Minute Hard Alert
      if (emptyDuration > HARD_ALERT_TIME && !hardAlertSent) {
        Serial.println("[ALERT] Sending 10-min Strong Alert.");
        sendTelegram(getMsg(
          "[ALERT] Lights have been ON in an empty room for over 10 minutes!",
          "[ALERT] Khali kamre mein pichle 10 minute se light ON hai!",
          "[ALERT] Khali kamre vich pichle 10 minute ton light ON hai!"
        ));
        hardAlertSent = true;
      }
    } 
    else {
      // Reset alert tracking when room is occupied or lights are OFF
      if (emptyRoomSince != 0) {
        Serial.println("[STATE] Resetting empty room timer.");
      }
      emptyRoomSince = 0;
      softAlertSent = false;
      hardAlertSent = false;
    }
  }

  // Poll for incoming Telegram updates
  if (millis() - lastBotScan > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotScan = millis();
  }
}
