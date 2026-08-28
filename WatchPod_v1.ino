/*
====================================================
                 WATCHPOD v1.3
       SMART HOME ENERGY & SECURITY ASSISTANT

v1.3 FINAL SCOPE:
- Single-file Arduino sketch
- Telegram duplicate-update protection
- Unauthorized Telegram users silently ignored
- LDR calibration with NVS persistence
- Reboot notification with restored mode
- Rich /status
- Hardware watchdog recovery
- Energy Saver mode
- Vacation Security mode
- Language support

POWER MANAGEMENT:
- No Deep Sleep
- No Low Power mode
- No /lowpower command
- No /normalpower command

Hardware:
PIR OUT  -> GPIO 32
PIR VCC  -> 5V
PIR GND  -> GND

LDR AO   -> GPIO 34
LDR VCC  -> 3.3V
LDR GND  -> GND

LED +    -> GPIO 15 through resistor
LED -    -> GND

Fill only the four values below.
====================================================
*/

// ============================================================
// USER CONFIGURATION — REPLACE THESE 4 PLACEHOLDERS
// ============================================================

#define WIFI_SSID "PASTE_YOUR_WIFI_NAME_HERE"
#define WIFI_PASSWORD "PASTE_YOUR_WIFI_PASSWORD_HERE"

#define TELEGRAM_BOT_TOKEN "PASTE_YOUR_TELEGRAM_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID "PASTE_YOUR_TELEGRAM_CHAT_ID_HERE"

// ============================================================
// IMPORTANT:
// Keep the quotation marks. Example:
// #define WIFI_SSID "MyWiFi"
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include <time.h>
#include <esp_task_wdt.h>

// ==========================================
// HARDWARE
// ==========================================

const int PIR_PIN = 32;
const int LDR_PIN = 34;
const int LED_PIN = 15;

// ==========================================
// SETTINGS
// ==========================================

// Stored ambient LDR baseline.
// /calibrate updates this value.
int lightBaseline = 1000;

// Difference used to determine whether the
// environment is significantly darker than
// the calibrated ambient baseline.
const int LIGHT_DELTA = 150;

const unsigned long BOT_MTBS = 1500;

// 10 x 30 seconds = 5-minute sliding window.
const unsigned long WINDOW_INTERVAL = 30000;

const unsigned long SOFT_ALERT_TIME = 120000;
const unsigned long HARD_ALERT_TIME = 600000;
const unsigned long VACATION_ALERT_COOLDOWN = 30000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;

// ==========================================
// MODES
// ==========================================

enum SystemMode {
  ENERGY_SAVER,
  VACATION
};

SystemMode currentMode = ENERGY_SAVER;

// ==========================================
// LANGUAGE
// ==========================================

String currentLanguage = "EN";

// ==========================================
// ACTIVITY WINDOW
// ==========================================

int windowPIR[10] = {
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0
};

int windowIndex = 0;
int activityScore = 0;

// ==========================================
// TIMERS / STATES
// ==========================================

unsigned long lastBotScan = 0;
unsigned long lastVacationAlert = 0;
unsigned long lastWindowShift = 0;
unsigned long emptyRoomSince = 0;
unsigned long lastWifiRetry = 0;

bool softAlertSent = false;
bool hardAlertSent = false;
bool wasWifiConnected = false;

// ==========================================
// TELEGRAM DUPLICATE PROTECTION
// ==========================================

long lastProcessedUpdateId = -1;

// ==========================================
// WIFI / TELEGRAM
// ==========================================

WiFiClientSecure client;

UniversalTelegramBot bot(
  TELEGRAM_BOT_TOKEN,
  client
);

// ==========================================
// PREFERENCES
// ==========================================

Preferences preferences;

// ==========================================
// LANGUAGE MESSAGE SELECTOR
// ==========================================

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

// ==========================================
// TELEGRAM SEND
// ==========================================

bool sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TELEGRAM] Wi-Fi unavailable.");
    return false;
  }

  bool result = bot.sendMessage(
    TELEGRAM_CHAT_ID,
    message,
    ""
  );

  if (!result) {
    Serial.println("[TELEGRAM] Send failed.");
    return false;
  }

  return true;
}

// ==========================================
// TIME
// ==========================================

String getFormattedTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "Time Sync Error";
  }

  char buffer[50];

  strftime(
    buffer,
    sizeof(buffer),
    "%d-%m-%Y %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

// ==========================================
// WIFI MAINTENANCE
// ==========================================

void maintainWiFi() {
  bool wifiConnected =
    (WiFi.status() == WL_CONNECTED);

  if (wifiConnected && !wasWifiConnected) {
    Serial.println("[WIFI] Connection restored.");

    client.stop();

    configTime(
      19800,
      0,
      "pool.ntp.org"
    );

    lastBotScan = millis();
  }

  wasWifiConnected = wifiConnected;

  if (wifiConnected) {
    return;
  }

  if (
    millis() - lastWifiRetry >=
    WIFI_RETRY_INTERVAL
  ) {
    lastWifiRetry = millis();

    Serial.println("[WIFI] Retrying...");

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }
}

// ==========================================
// ACTIVITY SCORE
// ==========================================

void updateActivityScore() {
  int sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += windowPIR[i];
  }

  activityScore = sum;

  Serial.print("[SYSTEM] Activity Score: ");
  Serial.print(activityScore);
  Serial.println("/10");
}

// ==========================================
// RESET ACTIVITY WINDOW
// ==========================================

void resetActivityWindow() {
  for (int i = 0; i < 10; i++) {
    windowPIR[i] = 0;
  }

  windowIndex = 0;
  activityScore = 0;
  lastWindowShift = millis();
}

// ==========================================
// LDR CALIBRATION
// ==========================================

int readLDRAverage(int samples) {
  long total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(LDR_PIN);
    delay(20);
  }

  return (int)(total / samples);
}

void calibrateLDR() {
  Serial.println("[LDR] Calibration started.");

  int measuredBaseline =
    readLDRAverage(20);

  lightBaseline =
    measuredBaseline;

  preferences.putInt(
    "ldr_base",
    lightBaseline
  );

  Serial.print(
    "[LDR] New baseline: "
  );

  Serial.println(
    lightBaseline
  );
}

// ==========================================
// TELEGRAM MESSAGE HANDLER
// ==========================================

void handleNewMessages(
  int numNewMessages
) {
  for (
    int i = 0;
    i < numNewMessages;
    i++
  ) {
    long updateId =
      bot.messages[i].update_id;

    // Ignore already processed updates.
    if (
      updateId <=
      lastProcessedUpdateId
    ) {
      Serial.print(
        "[TELEGRAM] Duplicate ignored: "
      );

      Serial.println(updateId);

      continue;
    }

    lastProcessedUpdateId =
      updateId;

    String senderChatId =
      String(
        bot.messages[i].chat_id
      );

    // Unauthorized users are silently ignored.
    if (
      senderChatId !=
      TELEGRAM_CHAT_ID
    ) {
      Serial.println(
        "[SECURITY] Unauthorized update ignored."
      );

      continue;
    }

    String text =
      bot.messages[i].text;

    Serial.print(
      "[TELEGRAM] Received: "
    );

    Serial.println(text);

    // ======================================
    // /start
    // ======================================

    if (text == "/start") {
      String welcome =
        getMsg(

          "Welcome to WatchPod v1.3!\n\n"
          "Commands:\n"
          "/status - Live system status\n"
          "/energy - Energy Saver Mode\n"
          "/vacation - Vacation Security\n"
          "/time - Current time and mode\n"
          "/lang - Change language\n"
          "/calibrate - Calibrate LDR",

          "WatchPod v1.3 mein aapka swagat hai!\n\n"
          "Commands:\n"
          "/status - Live system status\n"
          "/energy - Energy Saver Mode\n"
          "/vacation - Vacation Security\n"
          "/time - Samay aur mode\n"
          "/lang - Bhasha badlein\n"
          "/calibrate - LDR calibration",

          "WatchPod v1.3 vich tuhada swagat hai!\n\n"
          "Commands:\n"
          "/status - Live system status\n"
          "/energy - Energy Saver Mode\n"
          "/vacation - Vacation Security\n"
          "/time - Samaa te mode\n"
          "/lang - Bhasha badlo\n"
          "/calibrate - LDR calibration"
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

      String pirState =
        (currentPir == HIGH)
        ? "MOTION"
        : "CLEAR";

      int detectionThreshold =
        lightBaseline - LIGHT_DELTA;

      String ldrState =
        (currentLdr < detectionThreshold)
        ? "LIGHT ON"
        : "LIGHT OFF";

      String modeStr =
        (currentMode == VACATION)
        ? "VACATION"
        : "ENERGY SAVER";

      String wifiStr =
        (WiFi.status() == WL_CONNECTED)
        ? "CONNECTED"
        : "DISCONNECTED";

      String msg =
        "WATCHPOD STATUS\n"
        "================\n";

      msg +=
        "SYSTEM : ONLINE\n";

      msg +=
        "WIFI   : " +
        wifiStr +
        "\n";

      if (
        WiFi.status() ==
        WL_CONNECTED
      ) {
        msg +=
          "RSSI   : " +
          String(WiFi.RSSI()) +
          " dBm\n";
      }

      msg +=
        "FREE HEAP: " +
        String(ESP.getFreeHeap()) +
        " bytes\n";

      msg +=
        "UPTIME : " +
        String(millis() / 1000) +
        " sec\n";

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
        "LDR BASELINE: " +
        String(lightBaseline) +
        "\n";

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

      String msg =
        getMsg(

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

      emptyRoomSince = 0;
      softAlertSent = false;
      hardAlertSent = false;

      sendTelegram(
        getMsg(

          "Mode switched to Energy Saver.",

          "Mode badal kar Energy Saver kar diya gaya hai.",

          "Mode badal ke Energy Saver kar ditta gaya hai."
        )
      );
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

      lastVacationAlert =
        millis() -
        VACATION_ALERT_COOLDOWN;

      sendTelegram(
        getMsg(

          "Mode switched to Vacation Security.",

          "Mode badal kar Vacation Security kar diya gaya hai.",

          "Mode badal ke Vacation Security kar ditta gaya hai."
        )
      );
    }

    // ======================================
    // /calibrate
    // ======================================

    else if (text == "/calibrate") {
      calibrateLDR();

      int detectionThreshold =
        lightBaseline - LIGHT_DELTA;

      String msg =
        "LDR calibration complete.\n\n"
        "Ambient baseline: " +
        String(lightBaseline) +
        "\n"
        "Detection threshold: " +
        String(detectionThreshold);

      sendTelegram(msg);
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
    // English
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
    // Hindi
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
    // Punjabi
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
// SETUP
// ==========================================

void setup() {
  Serial.begin(115200);

  // ========================================
  // Hardware watchdog
  // ========================================

  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms = 10000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_task_wdt_init(
    &wdtConfig
  );

  esp_task_wdt_add(NULL);

  Serial.println(
    "\n================================"
  );

  Serial.println(
    "       WATCHPOD v1.3"
  );

  Serial.println(
    "================================"
  );

  // ========================================
  // Hardware
  // ========================================

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

  // ========================================
  // Preferences
  // ========================================

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

  lightBaseline =
    preferences.getInt(
      "ldr_base",
      1000
    );

  Serial.println(
    "[NVS] Configuration loaded."
  );

  // ========================================
  // Wi-Fi
  // ========================================

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  /*
    Single-file version:
    certificate verification is disabled.
    Telegram traffic remains encrypted,
    but the server certificate is not verified.
  */

  client.setInsecure();

  Serial.print(
    "[WIFI] Connecting to "
  );

  Serial.println(
    WIFI_SSID
  );

  unsigned long wifiStart =
    millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - wifiStart < 20000
  ) {
    digitalWrite(
      LED_PIN,
      !digitalRead(LED_PIN)
    );

    delay(300);

    Serial.print(".");
  }

  digitalWrite(
    LED_PIN,
    LOW
  );

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    Serial.println(
      "\n[WIFI] Connected."
    );

    Serial.print(
      "[WIFI] IP: "
    );

    Serial.println(
      WiFi.localIP()
    );

    wasWifiConnected =
      true;

    configTime(
      19800,
      0,
      "pool.ntp.org"
    );

    String restoredMode =
      (currentMode == VACATION)
      ? "Vacation Security"
      : "Energy Saver";

    String rebootMessage =
      "WatchPod restarted.\n"
      "Mode restored: " +
      restoredMode +
      "\n"
      "LDR baseline: " +
      String(lightBaseline) +
      "\n"
      "Time: " +
      getFormattedTime();

    sendTelegram(
      rebootMessage
    );
  }

  else {
    Serial.println(
      "\n[WIFI] Initial connection failed."
    );
  }

  // ========================================
  // Activity window
  // ========================================

  resetActivityWindow();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  // Feed watchdog every loop iteration.
  esp_task_wdt_reset();

  maintainWiFi();

  // ========================================
  // SENSOR READ
  // ========================================

  int pirValue =
    digitalRead(PIR_PIN);

  int ldrValue =
    analogRead(LDR_PIN);

  int detectionThreshold =
    lightBaseline - LIGHT_DELTA;

  bool isLightOn =
    (ldrValue < detectionThreshold);

  // ========================================
  // MOTION LED
  // ========================================

  if (pirValue == HIGH) {
    windowPIR[windowIndex] = 1;

    digitalWrite(
      LED_PIN,
      HIGH
    );
  }

  else {
    digitalWrite(
      LED_PIN,
      LOW
    );
  }

  // ========================================
  // 5-MINUTE ACTIVITY WINDOW
  // ========================================

  if (
    millis() - lastWindowShift >=
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
  // VACATION SECURITY
  // ========================================

  if (
    currentMode ==
    VACATION
  ) {
    if (
      pirValue == HIGH &&
      millis() - lastVacationAlert >=
      VACATION_ALERT_COOLDOWN
    ) {
      Serial.println(
        "[ALERT] Vacation motion detected."
      );

      String securityAlert =
        getMsg(

          "WATCHPOD SECURITY ALERT\n\n"
          "Motion detected!\n"
          "Mode: VACATION\n"
          "Time: " +
          getFormattedTime() +
          "\n"
          "Activity Score: " +
          String(activityScore) +
          "/10",

          "WATCHPOD SECURITY ALERT\n\n"
          "Halchal detect hui!\n"
          "Mode: VACATION\n"
          "Time: " +
          getFormattedTime() +
          "\n"
          "Activity Score: " +
          String(activityScore) +
          "/10",

          "WATCHPOD SECURITY ALERT\n\n"
          "Halchal detect hoyi!\n"
          "Mode: VACATION\n"
          "Samaa: " +
          getFormattedTime() +
          "\n"
          "Activity Score: " +
          String(activityScore) +
          "/10"
        );

      sendTelegram(
        securityAlert
      );

      lastVacationAlert =
        millis();
    }
  }

  // ========================================
  // ENERGY SAVER
  // ========================================

  else {
    if (
      activityScore == 0 &&
      isLightOn
    ) {
      if (
        emptyRoomSince ==
        0
      ) {
        emptyRoomSince =
          millis();

        Serial.println(
          "[TIMER] Empty room + light ON."
        );
      }

      unsigned long emptyDuration =
        millis() -
        emptyRoomSince;

      // ------------------------------------
      // 2-MINUTE REMINDER
      // ------------------------------------

      if (
        emptyDuration >
        SOFT_ALERT_TIME &&
        !softAlertSent
      ) {
        String energyReminder =
          getMsg(

            "WATCHPOD ENERGY REMINDER\n\n"
            "Room appears unoccupied.\n"
            "Light is ON.\n"
            "Mode: ENERGY SAVER\n"
            "Time: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10",

            "WATCHPOD ENERGY REMINDER\n\n"
            "Kamra khali lag raha hai.\n"
            "Light ON hai.\n"
            "Mode: ENERGY SAVER\n"
            "Time: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10",

            "WATCHPOD ENERGY REMINDER\n\n"
            "Kamra khali lagda hai.\n"
            "Light ON hai.\n"
            "Mode: ENERGY SAVER\n"
            "Samaa: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10"
          );

        sendTelegram(
          energyReminder
        );

        softAlertSent =
          true;
      }

      // ------------------------------------
      // 10-MINUTE ALERT
      // ------------------------------------

      if (
        emptyDuration >
        HARD_ALERT_TIME &&
        !hardAlertSent
      ) {
        String energyAlert =
          getMsg(

            "WATCHPOD ENERGY ALERT\n\n"
            "Lights have been ON in an empty room for over 10 minutes.\n"
            "Mode: ENERGY SAVER\n"
            "Time: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10",

            "WATCHPOD ENERGY ALERT\n\n"
            "Khali kamre mein 10 minute se light ON hai.\n"
            "Mode: ENERGY SAVER\n"
            "Time: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10",

            "WATCHPOD ENERGY ALERT\n\n"
            "Khali kamre vich 10 minute ton light ON hai.\n"
            "Mode: ENERGY SAVER\n"
            "Samaa: " +
            getFormattedTime() +
            "\n"
            "Activity Score: " +
            String(activityScore) +
            "/10"
          );

        sendTelegram(
          energyAlert
        );

        hardAlertSent =
          true;
      }
    }

    else {
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
    WiFi.status() == WL_CONNECTED &&
    millis() - lastBotScan >= BOT_MTBS
  ) {
    int numNewMessages =
      bot.getUpdates(
        bot.last_message_received + 1
      );

    if (numNewMessages > 0) {
      handleNewMessages(
        numNewMessages
      );
    }

    lastBotScan =
      millis();
  }
}

