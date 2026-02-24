// Processing_Connect.pde
// Fetches Trello data, calculates LED pressure value, sends R,G,B via Serial.
// API Key Config:
//   Keys are read automatically from the project root .env file (../.env).

import processing.serial.*;
import java.util.HashMap;
import java.io.BufferedReader;
import java.io.FileReader;

// Trello Config (loaded from .env)
String API_KEY  = "";
String TOKEN    = "";
String BOARD_ID = "";

// Serial
// Change COM_PORT to match your Pico W's Serial1 port (check Arduino IDE -> Tools -> Port)
final String COM_PORT   = "COM4";
final int    BAUD_RATE  = 9600;

// Pressure Config
final float MAX_PRESSURE = 50.0;
final int   CHECK_INTERVAL_MS = 10000; // Poll every 10 seconds

// Runtime State
Serial serialPort;
int    lastCheck = -CHECK_INTERVAL_MS; // Force check on first frame
int    latestPotValue = 0;             // Latest raw potentiometer reading from Pico (0-4095)
int    cachedListCount = 0;            // Number of lists fetched from Trello

// ----------------------------------------
void setup() {
  size(400, 200);
  textSize(14);
  background(30);

  // Load API keys
  loadEnv("../.env");
  if (API_KEY.isEmpty() || TOKEN.isEmpty() || BOARD_ID.isEmpty()) {
    println("ERROR: Missing Trello credentials in .env file.");
    println("  Expected keys: TRELLO_API_KEY, TRELLO_TOKEN, TRELLO_BOARD_ID");
    fill(255, 80, 80);
    text("Missing .env credentials!\nCheck console for details.", 10, 80);
    noLoop();
    return;
  }
  println(".env loaded. Board ID: " + BOARD_ID.substring(0, min(6, BOARD_ID.length())) + "...");

  // Init serial port
  try {
    serialPort = new Serial(this, COM_PORT, BAUD_RATE);
    serialPort.bufferUntil('\n'); // Trigger serialEvent() on each complete line
    println("Serial port opened: " + COM_PORT);
  } catch (Exception e) {
    println("WARNING: Could not open serial port. Running in console-only mode.");
    serialPort = null;
  }
}

// ----------------------------------------
void draw() {
  // Poll Trello on interval
  if (millis() - lastCheck >= CHECK_INTERVAL_MS) {
    lastCheck = millis();
    fetchAndSend();
  }
}

// ----------------------------------------
// Receive lines from Pico W via Serial 
void serialEvent(Serial port) {
  String line = port.readStringUntil('\n');
  if (line == null) return;
  line = line.trim();

  if (line.startsWith("POT:")) {
    try {
      latestPotValue = Integer.parseInt(line.substring(4));
    } catch (Exception e) {}
  }
}

// ----------------------------------------
// Core: Fetch Trello -> Calculate pressure -> Send to Pico W
void fetchAndSend() {
  println("\n========================================");

  // 1. Get all lists on the board
  String listsJson = httpGet("https://api.trello.com/1/boards/" + BOARD_ID
    + "/lists?key=" + API_KEY + "&token=" + TOKEN);
  if (listsJson == null) { println("ERROR: Failed to fetch lists."); return; }

  JSONArray lists = parseJSONArray(listsJson);
  if (lists == null || lists.size() == 0) { println("ERROR: No lists found."); return; }

  println("Found " + lists.size() + " lists:");
  cachedListCount = lists.size(); // Update global for pot mapping
  for (int i = 0; i < lists.size(); i++) {
    println("  [" + i + "] " + lists.getJSONObject(i).getString("name"));
  }

  // 2. Map potentiometer reading to list index
  //    Pot range: 0-4095 (12-bit ADC). Lists: 0 to cachedListCount-1.
  int selectedIndex;
  if (cachedListCount <= 1) {
    selectedIndex = 0;
  } else {
    selectedIndex = (int)map(latestPotValue, 0, 4095, 0, cachedListCount - 0.001);
    selectedIndex = constrain(selectedIndex, 0, cachedListCount - 1);
  }
  JSONObject selectedList = lists.getJSONObject(selectedIndex);
  String listId   = selectedList.getString("id");
  String listName = selectedList.getString("name");
  println("---> Pot:" + latestPotValue + " -> List[" + selectedIndex + "]: '" + listName + "'");

  // 3. Get cards in the selected list
  String cardsJson = httpGet("https://api.trello.com/1/lists/" + listId
    + "/cards?key=" + API_KEY + "&token=" + TOKEN);
  if (cardsJson == null) { println("ERROR: Failed to fetch cards."); return; }

  JSONArray cards = parseJSONArray(cardsJson);
  println("There are " + cards.size() + " cards in this list.");

  // 4. Pressure Analysis (mirrors trello_api_test.py)
  int totalPressure = 0;
  long nowMs = System.currentTimeMillis();

  for (int i = 0; i < cards.size(); i++) {
    JSONObject card = cards.getJSONObject(i);
    String cardName = card.getString("name");
    println("   Card: " + cardName);

    if (!card.isNull("due")) {
      String dueStr = card.getString("due"); // "2024-05-20T10:00:00.000Z"
      long dueMs = parseISO8601toMs(dueStr);
      double hoursLeft = (dueMs - nowMs) / 3600000.0;

      if (hoursLeft < 0) {
        println("    Status: Overdue (" + nf((float)-hoursLeft, 0, 1) + " h) -> +20");
        totalPressure += 20;
      } else if (hoursLeft <= 24) {
        println("    Status: Due within 24h (" + nf((float)hoursLeft, 0, 1) + " h) -> +10");
        totalPressure += 10;
      } else if (hoursLeft <= 24 * 7) {
        println("    Status: Due within 7 days (" + nf((float)(hoursLeft/24), 0, 1) + " d) -> +5");
        totalPressure += 5;
      } else {
        println("    Status: Due in more than 7 days -> +2");
        totalPressure += 2;
      }
    } else {
      println("    Status: No due date -> +1");
      totalPressure += 1;
    }
  }

  println("Total pressure score: " + totalPressure);

  // 5. Color mapping: Blue(idle) → Green(low) → Yellow(mid) → Red(high)
  int redPwm, greenPwm, bluePwm;
  if (totalPressure == 0) {
    // No tasks / empty list → Blue
    redPwm = 0; greenPwm = 0; bluePwm = 255;
  } else {
    float ratio = constrain(totalPressure / MAX_PRESSURE, 0, 1);
    if (ratio <= 0.5) {
      // Green → Yellow: R ramps up, G stays at 255
      float t = ratio / 0.5;
      redPwm = (int)(255 * t);
      greenPwm = 255;
    } else {
      // Yellow → Red: R stays 255, G ramps down
      float t = (ratio - 0.5) / 0.5;
      redPwm = 255;
      greenPwm = (int)(255 * (1.0 - t));
    }
    bluePwm = 0;
  }

  println("PWM -> R:" + redPwm + " G:" + greenPwm + " B:" + bluePwm);

  // 6. Update Processing window 
  background(redPwm, greenPwm, bluePwm);
  fill(255);
  text("Pot: " + latestPotValue + " / 4095", 10, 25);
  text("List [" + selectedIndex + "]: " + listName, 10, 50);
  text("Pressure: " + totalPressure, 10, 75);
  text("R:" + redPwm + "  G:" + greenPwm + "  B:" + bluePwm, 10, 100);
  textSize(11);
  fill(255, 200);
  text("Rotate potentiometer to switch list", 10, 130);
  textSize(14);

  // 7. Send to Pico W via Serial (format: "R,G,B\n")
  if (serialPort != null) {
    serialPort.write(redPwm + "," + greenPwm + "," + bluePwm + "\n");
    println("Sent to Pico W: " + redPwm + "," + greenPwm + "," + bluePwm);
  } else {
    println("[No hardware] Serial send skipped.");
  }
}

// ----------------------------------------
// Load key=value pairs from a .env file into API_KEY / TOKEN / BOARD_ID
void loadEnv(String relativePath) {
  // Resolve path relative to sketch folder
  String fullPath = sketchPath(relativePath);
  try {
    BufferedReader reader = new BufferedReader(new FileReader(fullPath));
    String line;
    while ((line = reader.readLine()) != null) {
      line = line.trim();
      // Skip comments and blank lines
      if (line.isEmpty() || line.startsWith("#")) continue;
      int eq = line.indexOf('=');
      if (eq < 1) continue;
      String key   = line.substring(0, eq).trim();
      String value = line.substring(eq + 1).trim();
      // Strip surrounding quotes if present
      if (value.startsWith("\"") && value.endsWith("\"")) {
        value = value.substring(1, value.length() - 1);
      }
      switch (key) {
        case "TRELLO_API_KEY":  API_KEY  = value; break;
        case "TRELLO_TOKEN":    TOKEN    = value; break;
        case "TRELLO_BOARD_ID": BOARD_ID = value; break;
      }
    }
    reader.close();
  } catch (Exception e) {
    println("ERROR: Could not read .env file at: " + fullPath);
    println(e.getMessage());
  }
}

// ----------------------------------------
// HTTP GET helper — returns response body or null on error
String httpGet(String url) {
  try {
    String[] lines = loadStrings(url);
    if (lines == null) return null;
    return join(lines, "");
  } catch (Exception e) {
    println("HTTP error: " + e.getMessage());
    return null;
  }
}

// ----------------------------------------
// Parse ISO 8601 date string to Unix milliseconds
// Input format: "2024-05-20T10:00:00.000Z"
long parseISO8601toMs(String s) {
  try {
    // Strip fractional seconds and Z, reformat using Java's parse
    s = s.replace("Z", "+0000");
    // Processing uses Java underneath — use SimpleDateFormat
    java.text.SimpleDateFormat sdf = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSSZ");
    sdf.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
    return sdf.parse(s).getTime();
  } catch (Exception e) {
    // Fallback: try without milliseconds
    try {
      java.text.SimpleDateFormat sdf2 = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ");
      sdf2.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
      return sdf2.parse(s.replace("Z", "+0000")).getTime();
    } catch (Exception e2) {
      println("Date parse error: " + s);
      return System.currentTimeMillis(); // Treat as now -> no pressure
    }
  }
}
