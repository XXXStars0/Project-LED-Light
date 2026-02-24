//------------------------
// Assignment: RGB LED
//------------------------

// Mode switch: edit wifi_config.h
#include "wifi_config.h"

// Pin Setup - Output
const int PIN_LED_R = 13;
const int PIN_LED_G = 14;
const int PIN_LED_B = 15;

// Pin Setup - Input
const int PIN_POT = 26;
const int PIN_BUTTON = 16;
const int PIN_LDR = 27;
const int PIN_IR = 28;

// Serial1 baud must match Processing sketch
const int SERIAL1_BAUD = 9600;

// --- State Machine Definitions ---
enum SystemState {
  STATE_BOOTING = 0,
  STATE_LOADING = 1,
  STATE_ERROR = 2,
  STATE_TRACKING = 3
};

// Volatile variables shared between Core 0 and Core 1
volatile SystemState currentState = STATE_BOOTING;
volatile int targetR = 0;
volatile int targetG = 0;
volatile int targetB = 0;

// WiFi-mode-only includes and globals
#ifdef USE_WIFI_MODE
#include "keys.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

const float MAX_PRESSURE_THRESHOLD = 50.0;
const int MAX_LISTS = 10;
String listIDs[MAX_LISTS];
String listNames[MAX_LISTS];
int listCount = 0;
#endif

// ---------------------------------------------------------
// Core 1 (LED Animation Engine) - Runs on the second thread
// ---------------------------------------------------------
void setup1() {
  // Give Core 0 time to init pins
  delay(100);
}

void loop1() {
  unsigned long ms = millis();

  switch (currentState) {
  case STATE_BOOTING:
  case STATE_LOADING:
    // White breathing for both booting and loading new data
    {
      float fade = (sin(ms / 300.0) + 1.0) / 2.0; // 0.0 to 1.0
      int val = (int)(255 * fade);
      analogWrite(PIN_LED_R, val);
      analogWrite(PIN_LED_G, val);
      analogWrite(PIN_LED_B, val);
    }
    break;

  case STATE_ERROR:
    // Red rapid double-blink
    {
      int cycle = ms % 1000;
      if ((cycle > 0 && cycle < 100) || (cycle > 200 && cycle < 300)) {
        analogWrite(PIN_LED_R, 255);
      } else {
        analogWrite(PIN_LED_R, 0);
      }
      analogWrite(PIN_LED_G, 0);
      analogWrite(PIN_LED_B, 0);
    }
    break;

  case STATE_TRACKING:
    // Solid tracking color
    analogWrite(PIN_LED_R, targetR);
    analogWrite(PIN_LED_G, targetG);
    analogWrite(PIN_LED_B, targetB);
    break;
  }
  delay(10); // Prevent hard looping
}

// ---------------------------------------------------------
// Core 0 (Logic Engine) - Main Thread
// ---------------------------------------------------------

void setup() {
  Serial.begin(115200); // USB debug port

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_POT, INPUT);
  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_IR, INPUT);
  analogReadResolution(12);

  currentState = STATE_BOOTING;

#ifdef USE_WIFI_MODE
  // WiFi Mode: connect to network, sync time, pre-fetch lists
  Serial.print("Connecting to WiFi");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  Serial.print("Syncing Time");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synchronized!");

  currentState = STATE_LOADING;
  fetchTrelloLists();

  if (listCount == 0) {
    currentState = STATE_ERROR;
  } else {
    currentState =
        STATE_TRACKING; // Requires initial loop logic to calculate color
  }

#else
  // USB Mode: Serial (USB CDC) is shared with Processing.
  Serial.begin(SERIAL1_BAUD);
  // Do not change state here, Processing will send STATE commands
#endif
}

void loop() {
#ifdef USE_WIFI_MODE
  // ===== WiFi Mode Loop =====
  if (WiFi.status() != WL_CONNECTED) {
    if (currentState != STATE_ERROR) {
      Serial.println("WiFi disconnected!");
      currentState = STATE_ERROR;
    }
    delay(1000);
    return;
  }
  if (listCount == 0) {
    if (currentState != STATE_ERROR) {
      Serial.println("No lists found or API failed.");
      currentState = STATE_ERROR;
    }
    delay(1000);
    return;
  }

  // Edge Detection State
  static int lastSelectedIndex = -1;
  static unsigned long lastApiRequestTime = 0;
  const unsigned long apiRequestInterval = 10000; // 10s default

  int potValue = analogRead(PIN_POT);
  int currentSelectedIndex = map(potValue, 0, 4096, 0, listCount);
  currentSelectedIndex = constrain(currentSelectedIndex, 0, listCount - 1);

  bool timeToUpdate = (millis() - lastApiRequestTime >= apiRequestInterval);
  bool listChanged = (currentSelectedIndex != lastSelectedIndex);

  // Trigger API update if list index changed or timer expired
  if (listChanged || timeToUpdate) {
    currentState = STATE_LOADING; // Trigger rainbow animation

    Serial.println("\n========================================");
    Serial.print("---> Tracking list: '");
    Serial.print(listNames[currentSelectedIndex]);
    Serial.println("'");

    int totalPressure = calculateListPressure(listIDs[currentSelectedIndex]);
    Serial.print("Total pressure: ");
    Serial.println(totalPressure);

    // Color mapping: Blue(idle) → Green(low) → Yellow(mid) → Red(high)
    int r, g, b;
    if (totalPressure == 0) {
      r = 0;
      g = 0;
      b = 255;
    } else {
      float ratio = (float)totalPressure / MAX_PRESSURE_THRESHOLD;
      if (ratio > 1.0)
        ratio = 1.0;
      if (ratio <= 0.5) {
        float t = ratio / 0.5;
        r = (int)(255 * t);
        g = 255;
      } else {
        float t = (ratio - 0.5) / 0.5;
        r = 255;
        g = (int)(255 * (1.0 - t));
      }
      b = 0;
    }

    // Update target colors that Core 1 will use in TRACKING state
    targetR = r;
    targetG = g;
    targetB = b;

    // Update detection states
    lastSelectedIndex = currentSelectedIndex;
    lastApiRequestTime = millis();

    // Let animation catch up, then track
    delay(200);
    currentState = STATE_TRACKING;
  }

  delay(50); // Small loop delay

#else
  // ===== USB Mode Loop =====

  // 1. Report potentiometer value to Processing every 200ms ("POT:xxxx\n")
  static unsigned long lastPotSend = 0;
  if (millis() - lastPotSend >= 200) {
    lastPotSend = millis();
    int potValue = analogRead(PIN_POT);
    Serial.print("POT:");
    Serial.println(potValue);
  }

  // 2. Receive commands from Processing
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("STATE:")) {
      int s = line.substring(6).toInt();
      if (s >= 0 && s <= 3) {
        currentState = (SystemState)s;
      }
    } else if (line.startsWith("RGB:")) {
      int c1 = line.indexOf(':', 4); // Find where R starts
      int c2 = line.indexOf(',');
      int c3 = line.lastIndexOf(',');
      if (c2 > 0 && c3 > c2) {
        // Line format: RGB:123,45,0
        int r = line.substring(4, c2).toInt();
        int g = line.substring(c2 + 1, c3).toInt();
        int b = line.substring(c3 + 1).toInt();
        targetR = r;
        targetG = g;
        targetB = b;
        currentState = STATE_TRACKING; // Force tracking if colors provided
      }
    }
  }
#endif
}

// ---------------------------------------------------------
// WiFi Mode Helper Functions
// ---------------------------------------------------------
#ifdef USE_WIFI_MODE

void fetchTrelloLists() {
  HTTPClient http;
  String url = "https://api.trello.com/1/boards/" + String(TRELLO_BOARD_ID) +
               "/lists?key=" + String(TRELLO_API_KEY) +
               "&token=" + String(TRELLO_TOKEN);

  Serial.println("Fetching board Lists...");
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    // Use ArduinoJson v7 to parse
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonArray array = doc.as<JsonArray>();
      listCount = 0;
      for (JsonObject v : array) {
        if (listCount >= MAX_LISTS)
          break;
        listIDs[listCount] = v["id"].as<String>();
        listNames[listCount] = v["name"].as<String>();
        Serial.print("  [");
        Serial.print(listCount);
        Serial.print("] ");
        Serial.println(listNames[listCount]);
        listCount++;
      }
      Serial.print("✅ Found ");
      Serial.print(listCount);
      Serial.println(" lists.");
    } else {
      Serial.println("❌ JSON Parsing failed!");
    }
  } else {
    Serial.print("❌ Request failed, code: ");
    Serial.println(httpCode);
  }
  http.end();
}

int calculateListPressure(String listId) {
  HTTPClient http;
  String url = "https://api.trello.com/1/lists/" + listId +
               "/cards?key=" + String(TRELLO_API_KEY) +
               "&token=" + String(TRELLO_TOKEN);

  http.begin(url);
  int httpCode = http.GET();
  int pressure = 0;

  if (httpCode == 200) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());

    if (!error) {
      JsonArray cards = doc.as<JsonArray>();
      Serial.print(" There are ");
      Serial.print(cards.size());
      Serial.println(" cards in this list.");

      time_t currentTime = time(nullptr);

      for (JsonObject card : cards) {
        String cardName = card["name"].as<String>();
        Serial.print("   Card: ");
        Serial.println(cardName);

        if (!card["due"].isNull()) {
          String dueStr = card["due"].as<String>();
          time_t dueDate = parseISO8601(dueStr.c_str());

          double hoursLeft = difftime(dueDate, currentTime) / 3600.0;

          if (hoursLeft < 0) {
            Serial.print("    Status: Overdue! (");
            Serial.print(-hoursLeft);
            Serial.println(" h) -> +20");
            pressure += 20;
          } else if (hoursLeft <= 24.0) {
            Serial.print("    Status: Due within 24h! (");
            Serial.print(hoursLeft);
            Serial.println(" h) -> +10");
            pressure += 10;
          } else if (hoursLeft <= 24.0 * 7.0) {
            Serial.print("    Status: Due within 7 days. (");
            Serial.print(hoursLeft / 24.0);
            Serial.println(" d) -> +5");
            pressure += 5;
          } else {
            Serial.println("    Status: Due in more than 7 days. -> +2");
            pressure += 2;
          }
        } else {
          Serial.println("    Status: No due date -> +1");
          pressure += 1;
        }
      }
    } else {
      Serial.println("❌ JSON Parsing failed for cards!");
    }
  } else {
    Serial.println("❌ Failed to fetch cards.");
  }

  http.end();
  return pressure;
}

// Simple ISO8601 date string parser to UNIX time
time_t parseISO8601(const char *dateStr) {
  struct tm t = {0};
  int y, M, d, h, m;
  float s;

  // Format: 2024-05-20T10:00:00.000Z
  sscanf(dateStr, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s);

  t.tm_year = y - 1900;
  t.tm_mon = M - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = (int)s;

  // Default timezone mktime conversion
  return mktime(&t);
}
#endif // USE_WIFI_MODE