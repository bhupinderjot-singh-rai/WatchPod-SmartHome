/*
====================================================
                 WATCHPOD v1.0
          SMART BEDROOM/HOME FINAL VERSION

            * Default: Energy Saver
            * Optional: Vacation Security

Hardware:
PIR OUT  -> GPIO 32
PIR VCC  -> 5V
PIR GND  -> GND

LDR AO   -> GPIO 34
LDR VCC  -> 3.3V
LDR GND  -> GND

LED +    -> GPIO 15 (with resistor)
LED -    -> GND

Commands:
 /start
 /status
 /energy
 /vacation
 /time
 /lang
====================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"


// ==========================================
// 1. HARDWARE PIN DEFINITIONS
// ==========================================

const int PIR_PIN = 32;
const int LDR_PIN = 34;
const int LED_PIN = 15;


// ==========================================
// 2. SYSTEM CONSTANTS & THRESHOLDS
// ==========================================

const int LIGHT_THRESHOLD = 1000;

const unsigned long BOT_MTBS = 1000;

const unsigned long WINDOW_INTERVAL = 30000;

const unsigned long SOFT_ALERT_TIME = 120000;

const unsigned long HARD_ALERT_TIME = 600000;

// Vacation Mode alert cooldown
const unsigned long VACATION_ALERT_COOLDOWN = 30000;


// ==========================================
// 3. STATE VARIABLES
// ==========================================

enum SystemMode {
  ENERGY_SAVER,
  VACATION
};

SystemMode currentMode = ENERGY_SAVER;

String currentLanguage = "EN";


int windowPIR[10] = {
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0
};

int windowIndex = 0;

int activityScore = 0;


// Timers
unsigned long lastBotScan = 0;

unsigned long lastVacationAlert = 0;

unsigned long lastWindowShift = 0;

unsigned long emptyRoomSince = 0;

unsigned long lastWifiRetry = 0;
const unsigned long WIFI_RETRY_INTERVAL = 30000;
// Energy Saver alert states
bool softAlertSent = false;

bool hardAlertSent = false;


// ==========================================
// 4. WIFI / TELEGRAM
// ==========================================

WiFiClientSecure client;

UniversalTelegramBot bot(
  TELEGRAM_BOT_TOKEN,
  client
);


// ==========================================
// WIFI RECONNECT
// ==========================================

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {

    lastWifiRetry = millis();

    Serial.println(
      "[WIFI] Connection lost. Retrying..."
    );

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }
}


// ==========================================
// 5. PREFERENCES
// ==========================================

Preferences preferences;


// ==========================================
// 6. HELPER FUNCTIONS
// ==========================================


// ------------------------------------------
// Multi-language message selector
// ------------------------------------------

String getMsg(
  String en,
  String hi,
  String pa
) {

  if (currentLanguage == "HI") {
    return hi;
  }

  if (currentLanguage == "PA") {
    return pa;
  }

  return en;
}


// ------------------------------------------
// Send Telegram message
// ------------------------------------------

void sendTelegram(String message) {

  bot.sendMessage(
    TELEGRAM_CHAT_ID,
    message,
    ""
  );
}


// ------------------------------------------
// Get formatted IST time
// ------------------------------------------

String getFormattedTime() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "Time Sync Error";
  }

  char timeStringBuff[50];

  strftime(
    timeStringBuff,
    sizeof(timeStringBuff),
    "%d-%m-%Y %H:%M:%S",
    &timeinfo
  );

  return String(timeStringBuff);
}


// ==========================================
// 7. ACTIVITY SCORE
// ==========================================

void updateActivityScore() {

  int sum = 0;

  for (int i = 0; i < 10; i++) {

    sum += windowPIR[i];

  }

  activityScore = sum;


  Serial.print(
    "[SYSTEM] Activity Score updated: "
  );

  Serial.print(activityScore);

  Serial.println("/10");
}


// ==========================================
// 8. TELEGRAM MESSAGE HANDLER
// ==========================================

void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {


    // --------------------------------------
    // SECURITY CHECK
    // --------------------------------------

    String senderChatId =
      String(bot.messages[i].chat_id);


    if (senderChatId != TELEGRAM_CHAT_ID) {

      Serial.print(
        "[SECURITY] Unauthorized access attempt from Chat ID: "
      );

      Serial.println(senderChatId);


      bot.sendMessage(
        senderChatId,
        "Unauthorized Access Denied.",
        ""
      );

      continue;
    }


    String text =
      bot.messages[i].text;


    Serial.print(
      "[TELEGRAM] Received command: "
    );

    Serial.println(text);


    // ======================================
    // /start
    // ======================================

    if (text == "/start") {

      String welcome = getMsg(

        "Welcome to WatchPod v1.0!\n\n"
        "Commands:\n"
        "/status - Check live system status & Activity Score\n"
        "/energy - Smart Bedroom Energy Saver\n"
        "/vacation - Manual Security Mode\n"
        "/time - View current time & active mode\n"
        "/lang - Change language",

        "WatchPod v1.0 mein aapka swagat hai!\n\n"
        "Commands:\n"
        "/status - Live status aur Activity Score\n"
        "/energy - Smart Bedroom Energy Saver\n"
        "/vacation - Manual Security Mode\n"
        "/time - Samay aur active mode\n"
        "/lang - Bhasha badlein",

        "WatchPod v1.0 vich tuhada swagat hai!\n\n"
        "Commands:\n"
        "/status - Live status te Activity Score\n"
        "/energy - Smart Bedroom Energy Saver\n"
        "/vacation - Manual Security Mode\n"
        "/time - Samaa te active mode\n"
        "/lang - Bhasha badlo"
      );


      sendTelegram(welcome);
    }


    // ======================================
    // /status
    // ======================================

    else if (text == "/status") {

      int currentPir =
        digitalRead(PIR_PIN);

      int currentLdr =
        analogRead(LDR_PIN);


      String ldrState =
        (currentLdr < LIGHT_THRESHOLD)
        ? "LIGHT ON"
        : "LIGHT OFF";


      String pirState =
        (currentPir == HIGH)
        ? "MOTION"
        : "CLEAR";


      String modeStr =
        (currentMode == VACATION)
        ? "VACATION"
        : "ENERGY SAVER";


      String msg =
        "WATCHPOD STATUS\n===\n";


      msg +=
        "SYSTEM : ONLINE\n";


      msg +=
        "MODE   : " +
        modeStr +
        "\n\n";


      msg +=
        "TIME   : " +
        getFormattedTime() +
        "\n\n";


      msg +=
        "PIR    : " +
        pirState +
        "\n";


      msg +=
        "LDR    : " +
        String(currentLdr) +
        " (" +
        ldrState +
        ")\n";


      msg +=
        "ACTIVITY SCORE: " +
        String(activityScore) +
        "/10 (5m window)";


      sendTelegram(msg);
    }


    // ======================================
    // /time
    // ======================================

    else if (text == "/time") {

      String modeStr =
        (currentMode == VACATION)
        ? "VACATION"
        : "ENERGY SAVER";


      String msg = getMsg(

        "System Time: " +
        getFormattedTime() +
        "\nActive Mode: " +
        modeStr,

        "System Samay: " +
        getFormattedTime() +
        "\nActive Mode: " +
        modeStr,

        "System Samaa: " +
        getFormattedTime() +
        "\nActive Mode: " +
        modeStr
      );


      sendTelegram(msg);
    }


    // ======================================
    // /energy
    // ======================================

    else if (text == "/energy") {

      currentMode =
        ENERGY_SAVER;


      preferences.putUInt(
        "mode",
        ENERGY_SAVER
      );


      sendTelegram(getMsg(

        "Mode switched to Energy Saver.",

        "Mode badal kar Energy Saver kar diya gaya hai.",

        "Mode badal ke Energy Saver kar ditta gaya hai."
      ));
    }


    // ======================================
    // /vacation
    // ======================================

    else if (text == "/vacation") {

      currentMode =
        VACATION;


      preferences.putUInt(
        "mode",
        VACATION
      );


      sendTelegram(getMsg(

        "Mode switched to Vacation Security.",

        "Mode badal kar Vacation Security kar diya gaya hai.",

        "Mode badal ke Vacation Security kar ditta gaya hai."
      ));
    }


    // ======================================
    // /lang
    // ======================================

    else if (text == "/lang") {

      String keyboardJson =
        "[[\"English\", \"Hindi\", \"Punjabi\"]]";


      bot.sendMessageWithReplyKeyboard(
        TELEGRAM_CHAT_ID,
        "Choose Language / Bhasha chunein / Bhasha chuno:",
        "",
        keyboardJson,
        true
      );
    }


    // ======================================
    // ENGLISH
    // ======================================

    else if (text == "English") {

      currentLanguage =
        "EN";


      preferences.putString(
        "lang",
        "EN"
      );


      sendTelegram(
        "Language updated to English."
      );
    }


    // ======================================
    // HINDI
    // ======================================

    else if (text == "Hindi") {

      currentLanguage =
        "HI";


      preferences.putString(
        "lang",
        "HI"
      );


      sendTelegram(
        "Bhasha badal kar Hindi kar di gayi hai."
      );
    }


    // ======================================
    // PUNJABI
    // ======================================

    else if (text == "Punjabi") {

      currentLanguage =
        "PA";


      preferences.putString(
        "lang",
        "PA"
      );


      sendTelegram(
        "Bhasha badal ke Punjabi kar ditti gayi hai."
      );
    }
  }
}


// ==========================================
// 9. SETUP
// ==========================================

void setup() {

  Serial.begin(115200);


  Serial.println(
    "\n[BOOT] Initializing WatchPod Core..."
  );


  // ----------------------------------------
  // Hardware
  // ----------------------------------------

  pinMode(
    PIR_PIN,
    INPUT
  );


  pinMode(
    LDR_PIN,
    INPUT
  );


  pinMode(
    LED_PIN,
    OUTPUT
  );


  digitalWrite(
    LED_PIN,
    LOW
  );


  // ----------------------------------------
  // Preferences
  // ----------------------------------------

  preferences.begin(
    "watchpod",
    false
  );


  currentMode =
    (SystemMode)preferences.getUInt(
      "mode",
      ENERGY_SAVER
    );


  currentLanguage =
    preferences.getString(
      "lang",
      "EN"
    );


  Serial.println(
    "[NVS] Loaded saved configurations:"
  );


  Serial.print(
    " - Mode: "
  );


  Serial.println(
    (currentMode == VACATION)
    ? "VACATION"
    : "ENERGY_SAVER"
  );


  Serial.print(
    " - Language: "
  );


  Serial.println(
    currentLanguage
  );


  // ----------------------------------------
  // Wi-Fi
  // ----------------------------------------

  WiFi.mode(
    WIFI_STA
  );


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  client.setCACert(
    TELEGRAM_CERTIFICATE_ROOT
  );


  Serial.print(
    "[WIFI] Connecting to "
  );


  Serial.println(
    WIFI_SSID
  );


  while (
    WiFi.status() != WL_CONNECTED
  ) {

    digitalWrite(
      LED_PIN,
      !digitalRead(LED_PIN)
    );


    delay(500);


    Serial.print(".");
  }


  digitalWrite(
    LED_PIN,
    LOW
  );


  Serial.println(
    "\n[WIFI] Connected successfully!"
  );


  Serial.print(
    "[WIFI] IP Address: "
  );


  Serial.println(
    WiFi.localIP()
  );


  // ----------------------------------------
  // NTP / IST
  // ----------------------------------------

  configTime(
    19800,
    0,
    "pool.ntp.org"
  );


  Serial.println(
    "[NTP] Syncing network time..."
  );


  // ----------------------------------------
  // Telegram startup message
  // ----------------------------------------

  sendTelegram(getMsg(

    "WatchPod Online & Synced.",

    "WatchPod Online aur Synced hai.",

    "WatchPod Online te Synced hai."
  ));
}


// ==========================================
// 10. MAIN LOOP
// ==========================================

void loop() {
  maintainWiFi();


  int pirValue =
    digitalRead(PIR_PIN);


  int ldrValue =
    analogRead(LDR_PIN);


  bool isLightOn =
    (ldrValue < LIGHT_THRESHOLD);


  // ========================================
  // MOTION LED
  // ========================================

  if (pirValue == HIGH) {

    windowPIR[windowIndex] =
      1;


    digitalWrite(
      LED_PIN,
      HIGH
    );

  } else {

    digitalWrite(
      LED_PIN,
      LOW
    );
  }


  // ========================================
  // 5-MINUTE SLIDING WINDOW
  // 10 × 30 SECOND SLICES
  // ========================================

  if (
    millis() - lastWindowShift >
    WINDOW_INTERVAL
  ) {

    lastWindowShift =
      millis();


    windowIndex =
      (windowIndex + 1) % 10;


    windowPIR[windowIndex] =
      0;


    updateActivityScore();
  }


  // ========================================
  // VACATION SECURITY MODE
  // ========================================

  if (currentMode == VACATION) {


    if (
      pirValue == HIGH &&
      millis() - lastVacationAlert >=
      VACATION_ALERT_COOLDOWN
    ) {


      Serial.println(
        "[ALERT] Intrusion detected in Vacation Mode!"
      );


      sendTelegram(getMsg(

        "[SECURITY ALERT] Motion detected in Vacation Mode!",

        "[SECURITY ALERT] Vacation Mode mein halchal detect hui!",

        "[SECURITY ALERT] Vacation Mode vich halchal detect hoyi!"
      ));


      lastVacationAlert =
        millis();
    }
  }


  // ========================================
  // ENERGY SAVER MODE
  // ========================================

  else {


    if (
      activityScore == 0 &&
      isLightOn
    ) {


      // ------------------------------------
      // Start empty-room timer
      // ------------------------------------

      if (
        emptyRoomSince == 0
      ) {

        emptyRoomSince =
          millis();


        Serial.println(
          "[TIMER] Room empty with lights ON. Tracking timer started."
        );
      }


      unsigned long emptyDuration =
        millis() - emptyRoomSince;


      // ------------------------------------
      // 2-MINUTE SOFT REMINDER
      // ------------------------------------

      if (
        emptyDuration >
        SOFT_ALERT_TIME &&
        !softAlertSent
      ) {


        Serial.println(
          "[ALERT] Sending 2-min Soft Reminder."
        );


        sendTelegram(getMsg(

          "[REMINDER] Room appears unoccupied, but lights are ON.",

          "[REMINDER] Kamre mein koi nahi hai, lekin light ON hai.",

          "[REMINDER] Kamre vich koi nahi hai, par light ON hai."
        ));


        softAlertSent =
          true;
      }


      // ------------------------------------
      // 10-MINUTE HARD ALERT
      // ------------------------------------

      if (
        emptyDuration >
        HARD_ALERT_TIME &&
        !hardAlertSent
      ) {


        Serial.println(
          "[ALERT] Sending 10-min Strong Alert."
        );


        sendTelegram(getMsg(

          "[ALERT] Lights have been ON in an empty room for over 10 minutes!",

          "[ALERT] Khali kamre mein pichle 10 minute se light ON hai!",

          "[ALERT] Khali kamre vich pichle 10 minute ton light ON hai."
        ));


        hardAlertSent =
          true;
      }
    }


    // --------------------------------------
    // Reset empty-room tracking
    // --------------------------------------

    else {


      if (
        emptyRoomSince != 0
      ) {

        Serial.println(
          "[STATE] Resetting empty room timer."
        );
      }


      emptyRoomSince =
        0;


      softAlertSent =
        false;


      hardAlertSent =
        false;
    }
  }


  // ========================================
  // TELEGRAM POLLING
  // ========================================

  if (
    millis() - lastBotScan >
    BOT_MTBS
  ) {


    int numNewMessages =
      bot.getUpdates(
        bot.last_message_received + 1
      );


    while (numNewMessages) {


      handleNewMessages(
        numNewMessages
      );


      numNewMessages =
        bot.getUpdates(
          bot.last_message_received + 1
        );
    }


    lastBotScan =
      millis();
  }
}
