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
#include <time.h>
#include <Preferences.h> 

// =================================================
// WIFI SETTINGS (Apni details yahan dalein)
// =================================================
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// =================================================
// TELEGRAM SETTINGS (Apni details yahan dalein)
// =================================================
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

// =================================================
// PIN SETTINGS
// =================================================
#define PIR_PIN 32
#define LDR_PIN 34
#define LED_PIN 15

// =================================================
// LIGHT SETTINGS
// =================================================
const int LIGHT_THRESHOLD = 1000; 

// =================================================
// TIME SETTINGS (IST UTC +5:30)
// =================================================
const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;

// =================================================
// ACTIVITY SCORE & TIMERS
// =================================================
const int MAX_PIR_EVENTS = 10; 
unsigned long pir_events[MAX_PIR_EVENTS];
int pir_event_index = 0;
const unsigned long ACTIVITY_WINDOW = 300000; // 5 minutes window

const unsigned long EMPTY_SOFT_WARNING_TIME = 120000; // 2 mins (Light ON, No Motion)
const unsigned long EMPTY_REMINDER_TIME = 600000;     // 10 mins (Light ON, No Motion)

const unsigned long TELEGRAM_CHECK_INTERVAL = 1000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long TIME_SYNC_INTERVAL = 3600000;
const unsigned long SERIAL_INTERVAL = 2000;

// =================================================
// SYSTEM OBJECTS
// =================================================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Preferences preferences; 

enum SystemMode {
  MODE_VACATION, // 24/7 Manual Security
  MODE_ENERGY    // Smart Bedroom Energy Saver (Default)
};
SystemMode currentMode = MODE_ENERGY;

// =================================================
// VARIABLES
// =================================================
unsigned long lastMotionTime = 0;
unsigned long lastBotCheck = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastTimeSync = 0;
unsigned long lastSerial = 0;

bool motionAlertSent = false;
bool softWarningSent = false;
bool reminderAlertSent = false;
int previousPirState = LOW;

String currentLanguage = "EN"; 

// =================================================
// NVS MEMORY FUNCTIONS
// =================================================
void saveModeToNVS(SystemMode mode) {
  preferences.begin("watchpod", false);
  preferences.putInt("mode", (int)mode);
  preferences.end();
  Serial.print("Saved Mode to NVS: ");
  Serial.println((int)mode);
}

void loadModeFromNVS() {
  preferences.begin("watchpod", true);
  int storedMode = preferences.getInt("mode", (int)MODE_ENERGY);
  currentMode = (SystemMode)storedMode;
  preferences.end();
  Serial.print("Loaded Mode from NVS: ");
  Serial.println(storedMode);
}

// =================================================
// ACTIVITY SCORE LOGIC
// =================================================
void updateActivityLog() {
  unsigned long now = millis();
  pir_events[pir_event_index] = now;
  pir_event_index = (pir_event_index + 1) % MAX_PIR_EVENTS;
}

int getActivityScore() {
  int count = 0;
  unsigned long now = millis();
  for (int i = 0; i < MAX_PIR_EVENTS; i++) {
    if (pir_events[i] > 0 && (now - pir_events[i] < ACTIVITY_WINDOW)) {
      count++;
    }
  }
  return count;
}

// =================================================
// WIFI & TIME SETUP
// =================================================
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); 
    Serial.print("."); 
    attempts++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi connection failed. Will retry.");
  }
}

void setupTime() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Syncing time IST...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
      Serial.println("Time synchronized.");
      lastTimeSync = millis();
    } else {
      Serial.println("Time sync failed.");
    }
  }
}

String getCurrentTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "TIME NOT SYNCED YET";
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

void sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, message, "");
  }
}

void blinkLED(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(duration);
    digitalWrite(LED_PIN, LOW); delay(duration);
  }
}

void resetAlertStates() {
  motionAlertSent = false;
  softWarningSent = false;
  reminderAlertSent = false;
  lastMotionTime = millis();
  previousPirState = digitalRead(PIR_PIN);
  for (int i = 0; i < MAX_PIR_EVENTS; i++) pir_events[i] = 0;
}

// =================================================
// MULTI-LANGUAGE SYSTEM TEXTS
// =================================================
String getText(String key) {
  if (currentLanguage == "EN") {
    if (key == "ONLINE") return "WATCHPOD ONLINE\n===\nPrimary: Energy Saver (Active)\nVacation: Optional/Manual\nUse /help";
    if (key == "VACATION") return "VACATION MODE (Security ARMED)\n===\n24/7 Intrusion alerts active. House is empty.";
    if (key == "ENERGY") return "ENERGY SAVER MODE (Default Active)\n===\nMonitoring Lights & Motion context.\nSuppresses alerts during sleep.";
    if (key == "MOTION_ALERT") return "WATCHPOD SECURITY ALERT\n===\nMOTION DETECTED!\nCheck for intrusions immediately.";
    if (key == "SOFT_WARNING") return "WATCHPOD ENERGY WARN\n===\nLIGHT IS ON.\nNO MOTION FOR 2 MINUTES.\nRoom activity extremely low.";
    if (key == "REMINDER_ALERT") return "WATCHPOD ENERGY REMIND\n===\nLIGHT STILL ON.\nNO MOTION FOR 10 MINUTES.\nPlease check the light.";
    if (key == "STATUS_VACATION") return "VACATION SECURITY";
    if (key == "STATUS_ENERGY") return "ENERGY SAVER";
  }

  if (currentLanguage == "HI") {
    if (key == "ONLINE") return "WATCHPOD ONLINE\n===\nDefault: Energy Saver Active\nUse /help";
    if (key == "VACATION") return "VACATION MODE (Security Active)\n===\n24/7 Intrusion alerts ARMED hain.";
    if (key == "ENERGY") return "ENERGY SAVER MODE (Default Active)\n===\nLight aur Motion context monitor ho raha hai.";
    if (key == "MOTION_ALERT") return "WATCHPOD SECURITY ALERT\n===\nMOTION DETECTED!\nArea check karein!";
    if (key == "SOFT_WARNING") return "WATCHPOD ENERGY WARN\n===\nLIGHT ON HAI aur 2 min se motion nahi hai.";
    if (key == "REMINDER_ALERT") return "WATCHPOD ENERGY REMIND\n===\nLIGHT ABHI BHI ON HAI.\n10 min se koi motion nahi.\nPlease light check karein.";
    if (key == "STATUS_VACATION") return "VACATION SECURITY";
    if (key == "STATUS_ENERGY") return "ENERGY SAVER";
  }

  if (currentLanguage == "PA") {
    if (key == "ONLINE") return "WATCHPOD ONLINE\n===\nDefault: Energy Saver Active\nUse /help";
    if (key == "VACATION") return "VACATION MODE (Security Active)\n===\n24/7 Intrusion alerts lag gaye han.";
    if (key == "ENERGY") return "ENERGY SAVER MODE (Default Active)\n===\nRoom Context di monitoring ho rahi hai.";
    if (key == "MOTION_ALERT") return "WATCHPOD SECURITY ALERT\n===\nMOTION DETECTED!\nArea check karo!";
    if (key == "SOFT_WARNING") return "WATCHPOD ENERGY WARN\n===\nLIGHT ON HAI te 2 min ton motion nahi hai.";
    if (key == "REMINDER_ALERT") return "WATCHPOD ENERGY REMIND\n===\nLIGHT HALLE VI ON HAI.\n10 min ton koi motion nahi.\nPlease light check karo.";
    if (key == "STATUS_VACATION") return "VACATION SECURITY";
    if (key == "STATUS_ENERGY") return "ENERGY SAVER";
  }
  return "";
}

String getModeName() {
  if (currentMode == MODE_VACATION) return getText("STATUS_VACATION");
  return getText("STATUS_ENERGY");
}

// =================================================
// STATUS & COMMAND CENTER
// =================================================
void sendStatus() {
  int pirValue = digitalRead(PIR_PIN);
  int ldrValue = analogRead(LDR_PIN);
  bool lightOn = (ldrValue < LIGHT_THRESHOLD);
  int activity = getActivityScore();

  String message = "WATCHPOD STATUS\n===\n";
  message += "SYSTEM : ONLINE\n";
  message += "MODE   : " + getModeName() + "\n\n";
  message += "TIME   : " + getCurrentTimeString() + "\n\n";
  
  message += "PIR    : " + String(pirValue == HIGH ? "MOTION\n" : "CLEAR\n");
  message += "LDR    : " + String(ldrValue) + String(lightOn ? " (LIGHT ON)\n" : " (LIGHT OFF)\n");
  message += "ACTIVITY SCORE: " + String(activity) + "/10 (5m window)\n";
  if (activity <= 1) message += " (Low Activity / Bed Context)\n";

  sendTelegram(message);
}

void sendHelp() {
  String message = "WATCHPOD CONTROL CENTER\n===\n"
    "/status - Check live status & Activity Score\n\n"
    "/energy - Smart Bedroom Energy Saver (Default)\n\n"
    "/vacation - Manual Security (24/7 Vacation Mode)\n\n"
    "/time - View IST time & mode info\n\n"
    "/lang - Change Language (EN/HI/PA)";
  sendTelegram(message);
}

void sendLanguageMenu() {
  String message = "WATCHPOD LANGUAGE SETTINGS\n===\nSelect language:\nEnglish\nHindi\nPunjabi";
  String keyboard = "[[\"English\",\"Hindi\"],[\"Punjabi\"]]";
  bot.sendMessageWithReplyKeyboard(CHAT_ID, message, "", keyboard, true);
}

// =================================================
// TELEGRAM COMMAND HANDLER
// =================================================
void handleTelegram() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      if (chat_id != CHAT_ID) {
        bot.sendMessage(chat_id, "Unauthorized access.", "");
        continue;
      }

      if (text == "/start") {
        sendTelegram(getText("ONLINE"));
      }
      else if (text == "/help") {
        sendHelp();
      }
      else if (text == "/status") {
        sendStatus();
      }
      else if (text == "/vacation" || text == "/on") {
        currentMode = MODE_VACATION;
        resetAlertStates();
        saveModeToNVS(MODE_VACATION);
        sendTelegram(getText("VACATION"));
      }
      else if (text == "/energy" || text == "/auto" || text == "/off") {
        // Mapped old auto/off commands to the new smart default
        currentMode = MODE_ENERGY;
        resetAlertStates();
        saveModeToNVS(MODE_ENERGY);
        sendTelegram(getText("ENERGY"));
      }
      else if (text == "/time") {
        String message = "WATCHPOD TIME SYSTEM\n===\n";
        message += "CURRENT TIME\n" + getCurrentTimeString();
        message += "\n\nMODE INFO:\n- Energy Saver: Active (Default)\n- Vacation Security: Manual Override";
        sendTelegram(message);
      }
      else if (text == "/lang") {
        sendLanguageMenu();
      }
      else if (text == "English") {
        currentLanguage = "EN";
        sendTelegram("Language updated to English.");
      }
      else if (text == "Hindi") {
        currentLanguage = "HI";
        sendTelegram("Language updated to Hindi.");
      }
      else if (text == "Punjabi") {
        currentLanguage = "PA";
        sendTelegram("Language updated to Punjabi.");
      }
      else {
        sendTelegram("Command not recognized. Use /help");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// =================================================
// SETUP
// =================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  analogReadResolution(12);
  secured_client.setInsecure();

  loadModeFromNVS(); 

  connectWiFi();
  setupTime();

  lastMotionTime = millis();
  previousPirState = digitalRead(PIR_PIN);
  for (int i = 0; i < MAX_PIR_EVENTS; i++) pir_events[i] = 0;

  blinkLED(2, 200);

  if (WiFi.status() == WL_CONNECTED) {
    sendTelegram(getText("ONLINE"));
  }
  Serial.println("WATCHPOD READY");
}

// =================================================
// MAIN LOOP
// =================================================
void loop() {
  unsigned long currentTime = millis();

  // WiFi Check & Reconnect
  if (currentTime - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = currentTime;
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
      if (WiFi.status() == WL_CONNECTED) setupTime();
    }
  }

  // Periodic Time Sync
  if (WiFi.status() == WL_CONNECTED && (currentTime - lastTimeSync >= TIME_SYNC_INTERVAL)) {
    setupTime();
  }

  // Telegram Check
  if (WiFi.status() == WL_CONNECTED && (currentTime - lastBotCheck >= TELEGRAM_CHECK_INTERVAL)) {
    handleTelegram();
    lastBotCheck = currentTime;
  }

  // SENSOR READINGS
  int pirValue = digitalRead(PIR_PIN);
  int ldrValue = analogRead(LDR_PIN);
  bool lightOn = (ldrValue < LIGHT_THRESHOLD);

  // =================================================
  // MOTION HANDLING
  // =================================================
  if (pirValue == HIGH) {
    lastMotionTime = currentTime;

    // Track rising edge for Activity Score
    if (previousPirState == LOW) {
      updateActivityLog();
    }

    reminderAlertSent = false;

    // Vacation Security Alert 
    if (currentMode == MODE_VACATION && !motionAlertSent) {
      sendTelegram(getText("MOTION_ALERT"));
      motionAlertSent = true;
      blinkLED(2, 100);
    }
  }

  // PIR Return to LOW (Rearm Security)
  if (pirValue == LOW && previousPirState == HIGH) {
    motionAlertSent = false;
  }
  previousPirState = pirValue;

  // =================================================
  // ENERGY SAVER LOGIC (Context-Aware)
  // =================================================
  if (currentMode == MODE_ENERGY) {
    unsigned long noMotionDuration = currentTime - lastMotionTime;
    int activity = getActivityScore();

    // Empty room criteria: no motion AND activity score is extremely low (<=1)
    bool roomContextEmpty = (noMotionDuration >= EMPTY_SOFT_WARNING_TIME && activity <= 1);

    if (lightOn) {
      // 10 Min Strong Reminder
      if (noMotionDuration >= EMPTY_REMINDER_TIME && !reminderAlertSent && activity <= 1) {
        sendTelegram(getText("REMINDER_ALERT"));
        reminderAlertSent = true;
        blinkLED(3, 150);
      }
      // 2 Min Soft Warning
      else if (noMotionDuration >= EMPTY_SOFT_WARNING_TIME && !softWarningSent && roomContextEmpty) {
        sendTelegram(getText("SOFT_WARNING"));
        softWarningSent = true;
        blinkLED(1, 300);
      }
    }

    // Reset warnings if light turns off or activity resumes
    if (!lightOn || activity > 1) {
      softWarningSent = false;
    }
    if (!lightOn) {
      reminderAlertSent = false;
    }
  }

  // =================================================
  // SERIAL DEBUG MONITOR
  // =================================================
  if (currentTime - lastSerial >= SERIAL_INTERVAL) {
    Serial.print("PIR: "); Serial.print(pirValue);
    Serial.print(" | Activity: "); Serial.print(getActivityScore());
    Serial.print("/10 | LDR: "); Serial.print(ldrValue);
    Serial.print(" | Mode: ");
    if (currentMode == MODE_VACATION) Serial.println("VACATION");
    else Serial.println("ENERGY SAVER");
    lastSerial = currentTime;
  }

  delay(20);
}
