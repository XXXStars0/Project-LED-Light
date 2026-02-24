//------------------------
// Assignment: RGB LED
//------------------------
// Basic Function and API Test Version
// Header

#include "keys.h" // Wifi Passwords and Trello API keys
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

// Pin Setup
const int PIN_LED_R = 13;
const int PIN_LED_G = 14;
const int PIN_LED_B = 15;
const int PIN_POT = 26;

const float MAX_PRESSURE_THRESHOLD = 50.0;

// Maximum number of lists we can store
const int MAX_LISTS = 10;
String listIDs[MAX_LISTS];
String listNames[MAX_LISTS];
int listCount = 0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_POT, INPUT);

  // Set RP2040 analog read resolution (0-4095)
  analogReadResolution(12);

  // 1. Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  // 2. Synchronize Time using NTP (Needed for Due Date calculation)
  Serial.print("Syncing Time");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\n✅ Time synchronized!");

  // 3. Fetch Trello Lists once at startup
  fetchTrelloLists();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi disconnected!");
    delay(5000);
    return;
  }

  if (listCount == 0) {
    Serial.println("❌ No lists found or API failed.");
    delay(5000);
    return;
  }

  // 4. Reading Potentiometer and mapping
  int potValue = analogRead(PIN_POT);
  // Map analog value (0-4095) to list index (0 to listCount-1)
  int selectedIndex = map(potValue, 0, 4096, 0, listCount);
  selectedIndex = constrain(selectedIndex, 0, listCount - 1);

  Serial.println("\n========================================");
  Serial.print(
      "---> 🎛️ Simulating potentiometer input: Currently tracking list '");
  Serial.print(listNames[selectedIndex]);
  Serial.println("'");

  // 5. Calculate pressure
  int totalPressure = calculateListPressure(listIDs[selectedIndex]);

  Serial.print("🔥 Total pressure score: ");
  Serial.println(totalPressure);

  // 6. Map to RGB
  float pressureRatio = (float)totalPressure / MAX_PRESSURE_THRESHOLD;
  if (pressureRatio > 1.0)
    pressureRatio = 1.0;

  int red_pwm = 255 * pressureRatio;
  int green_pwm = 255 * (1.0 - pressureRatio);
  int blue_pwm = 0; // Default is not to mix blue

  analogWrite(PIN_LED_R, red_pwm);
  analogWrite(PIN_LED_G, green_pwm);
  analogWrite(PIN_LED_B, blue_pwm);

  Serial.println("🎨 Pico W Pin PWM Output Instructions:");
  Serial.print("   -> 🔴 GP13 (Red):   ");
  Serial.println(red_pwm);
  Serial.print("   -> 🟢 GP14 (Green): ");
  Serial.println(green_pwm);
  Serial.print("   -> 🔵 GP15 (Blue):  ");
  Serial.println(blue_pwm);

  // Delay before next check (Trello API limits rate, don't ping too fast!)
  delay(10000);
}

// ---------------------------------------------------------
// Helper Functions
// ---------------------------------------------------------

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