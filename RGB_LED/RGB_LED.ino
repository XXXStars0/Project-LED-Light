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

void setup() {
  Serial.begin(115200); // USB debug port

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_POT, INPUT);
  analogReadResolution(12);

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
  fetchTrelloLists();

#else
  // USB Mode: Serial (USB CDC) is shared with Processing.
  // Re-init at the baud rate Processing uses.
  Serial.begin(SERIAL1_BAUD);
  // Note: no debug print here — would collide with incoming data from
  // Processing
#endif
}

void loop() {
#ifdef USE_WIFI_MODE
  // ===== WiFi Mode Loop =====
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected!");
    delay(5000);
    return;
  }
  if (listCount == 0) {
    Serial.println("No lists found or API failed.");
    delay(5000);
    return;
  }

  int potValue = analogRead(PIN_POT);
  int selectedIndex = map(potValue, 0, 4096, 0, listCount);
  selectedIndex = constrain(selectedIndex, 0, listCount - 1);

  Serial.println("\n========================================");
  Serial.print("---> Tracking list: '");
  Serial.print(listNames[selectedIndex]);
  Serial.println("'");

  int totalPressure = calculateListPressure(listIDs[selectedIndex]);
  Serial.print("Total pressure: ");
  Serial.println(totalPressure);

  // Color mapping: Blue(idle) → Green(low) → Yellow(mid) → Red(high)
  int r, g, b;
  if (totalPressure == 0) {
    // No tasks / empty list → Blue
    r = 0;
    g = 0;
    b = 255;
  } else {
    float ratio = (float)totalPressure / MAX_PRESSURE_THRESHOLD;
    if (ratio > 1.0)
      ratio = 1.0;
    if (ratio <= 0.5) {
      // Green → Yellow: R ramps up, G stays at 255
      float t = ratio / 0.5;
      r = (int)(255 * t);
      g = 255;
    } else {
      // Yellow → Red: R stays at 255, G ramps down
      float t = (ratio - 0.5) / 0.5;
      r = 255;
      g = (int)(255 * (1.0 - t));
    }
    b = 0;
  }
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);

  delay(10000);

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

  // 2. Receive R,G,B commands from Processing
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    // Only parse lines matching "R,G,B" format (ignore POT echo or empty lines)
    int c1 = line.indexOf(',');
    int c2 = line.lastIndexOf(',');
    if (c1 > 0 && c2 > c1) {
      int r = line.substring(0, c1).toInt();
      int g = line.substring(c1 + 1, c2).toInt();
      int b = line.substring(c2 + 1).toInt();

      analogWrite(PIN_LED_R, r);
      analogWrite(PIN_LED_G, g);
      analogWrite(PIN_LED_B, b);
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