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
final String COM_PORT   = "COM4";
final int    BAUD_RATE  = 9600;

// Pressure Config
final float MAX_PRESSURE = 50.0;
final int   CHECK_INTERVAL_MS = 10000; // Poll every 10 seconds

// --- State Machine Definitions ---
final int STATE_BOOTING = 0;
final int STATE_LOADING = 1;
final int STATE_ERROR   = 2;
final int STATE_TRACKING= 3;

// Runtime State
Serial serialPort;
long   lastApiRequestTime = -CHECK_INTERVAL_MS; 
int    latestPotValue = 0;             
int    cachedListCount = 0;          
int    lastSelectedIndex = -1;        
int    currentState = STATE_BOOTING;

// Target colors for TRACKING state
int    targetRed = 0;
int    targetGreen = 0;
int    targetBlue = 0;
boolean isFetching = false;       

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
  long ms = millis();

  // 1. Edge Detection & Timer Check
  boolean timeToUpdate = (ms - lastApiRequestTime >= CHECK_INTERVAL_MS);
  
  // Predict which list we're pointing at based on latestPotValue
  int currentSelectedIndex = 0;
  if (cachedListCount > 1) {
    currentSelectedIndex = (int)map(latestPotValue, 0, 4095, 0, cachedListCount - 0.001);
    currentSelectedIndex = constrain(currentSelectedIndex, 0, cachedListCount - 1);
  }
  
  boolean listChanged = (currentSelectedIndex != lastSelectedIndex) && (cachedListCount > 0);

  // 2. Trigger Background Thread
  if ((listChanged || timeToUpdate) && !isFetching) {
    lastSelectedIndex = currentSelectedIndex;
    lastApiRequestTime = ms;

    setState(STATE_LOADING);
    
    thread("fetchTrelloDataBackground"); 
  }

  // 3. Render Window UI based on current State
  switch (currentState) {
    case STATE_BOOTING:
      background(30);
      drawText("BOOTING... Waiting for API");
      break;

    case STATE_LOADING:
      // White/Gray breathing, matching BOOTING style but active
      float pulse = (sin(ms / 300.0) + 1.0) / 2.0; // 0.0 to 1.0
      int val = (int)(30 + (70 * pulse)); 
      background(val);
      drawText("LOADING Data from Trello...");
      break;

    case STATE_ERROR:
      // Red flash
      if ((ms % 1000) < 500) background(150, 0, 0); else background(30, 0, 0);
      drawText("ERROR: API Request Failed!");
      break;

    case STATE_TRACKING:
      background(targetRed, targetGreen, targetBlue);
      drawText("TRACKING | List [" + lastSelectedIndex + "]\nPot: " + latestPotValue + " / 4095\nR:" + targetRed + " G:" + targetGreen + " B:" + targetBlue);
      break;
  }
}

// UI Helper
void drawText(String msg) {
  fill(255);
  text(msg, 10, 30);
}

// State Helper: Updates local state and notifies Pico W
void setState(int s) {
  currentState = s;
  if (serialPort != null) {
    serialPort.write("STATE:" + s + "\n");
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
// Fetch Trello -> Calculate pressure -> Send to Pico W (Runs on separate thread)
void fetchTrelloDataBackground() {
  isFetching = true; // Lock
  println("\n========================================");

  // 1. Get all lists on the board
  String listsJson = httpGet("https://api.trello.com/1/boards/" + BOARD_ID + "/lists?key=" + API_KEY + "&token=" + TOKEN);
  if (listsJson == null) { 
    println("ERROR: Failed to fetch lists."); 
    setState(STATE_ERROR);
    isFetching = false; 
    return; 
  }

  JSONArray lists = parseJSONArray(listsJson);
  if (lists == null || lists.size() == 0) { 
    println("ERROR: No lists found."); 
    setState(STATE_ERROR);
    isFetching = false; 
    return; 
  }

  cachedListCount = lists.size();
  
  // Use the index saved right before the thread triggered
  int selectedIndex = lastSelectedIndex; 
  if (selectedIndex < 0 || selectedIndex >= cachedListCount) selectedIndex = 0;

  JSONObject selectedList = lists.getJSONObject(selectedIndex);
  String listId   = selectedList.getString("id");
  String listName = selectedList.getString("name");
  println("---> Tracking list[" + selectedIndex + "]: '" + listName + "'");

  // 2. Get cards in the selected list
  String cardsJson = httpGet("https://api.trello.com/1/lists/" + listId + "/cards?key=" + API_KEY + "&token=" + TOKEN);
  if (cardsJson == null) { 
    println("ERROR: Failed to fetch cards."); 
    setState(STATE_ERROR);
    isFetching = false; 
    return; 
  }

  JSONArray cards = parseJSONArray(cardsJson);
  println("There are " + cards.size() + " cards in this list.");

  // 3. Pressure Analysis
  int totalPressure = 0;
  long nowMs = System.currentTimeMillis();

  for (int i = 0; i < cards.size(); i++) {
    JSONObject card = cards.getJSONObject(i);
    if (!card.isNull("due")) {
      String dueStr = card.getString("due"); 
      long dueMs = parseISO8601toMs(dueStr);
      double hoursLeft = (dueMs - nowMs) / 3600000.0;

      if (hoursLeft < 0) totalPressure += 20;
      else if (hoursLeft <= 24) totalPressure += 10;
      else if (hoursLeft <= 24 * 7) totalPressure += 5;
      else totalPressure += 2;
    } else {
      totalPressure += 1;
    }
  }

  println("Total pressure score: " + totalPressure);

  // 4. Color mapping: Blue(idle) → Green(low) → Yellow(mid) → Red(high)
  if (totalPressure == 0) {
    targetRed = 0; targetGreen = 0; targetBlue = 255;
  } else {
    float ratio = constrain(totalPressure / MAX_PRESSURE, 0, 1);
    if (ratio <= 0.5) {
      float t = ratio / 0.5;
      targetRed = (int)(255 * t);
      targetGreen = 255;
    } else {
      float t = (ratio - 0.5) / 0.5;
      targetRed = 255;
      targetGreen = (int)(255 * (1.0 - t));
    }
    targetBlue = 0;
  }

  println("PWM -> R:" + targetRed + " G:" + targetGreen + " B:" + targetBlue);

  // 5. Send to Pico W via Serial (format: "RGB:r,g,b\n")
  // The RGB string implicitly tells Pico to enter TRACKING state, but we also set local tracking
  setState(STATE_TRACKING);
  if (serialPort != null) {
    serialPort.write("RGB:" + targetRed + "," + targetGreen + "," + targetBlue + "\n");
    println("Sent RGB to Pico W.");
  }
  
  isFetching = false; // Unlock
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

      if (line.isEmpty() || line.startsWith("#")) continue;
      int eq = line.indexOf('=');
      if (eq < 1) continue;
      String key   = line.substring(0, eq).trim();
      String value = line.substring(eq + 1).trim();

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
    s = s.replace("Z", "+0000");
    // Processing uses Java underneath — use SimpleDateFormat
    java.text.SimpleDateFormat sdf = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSSZ");
    sdf.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
    return sdf.parse(s).getTime();
  } catch (Exception e) {
    try {
      java.text.SimpleDateFormat sdf2 = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ");
      sdf2.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
      return sdf2.parse(s.replace("Z", "+0000")).getTime();
    } catch (Exception e2) {
      println("Date parse error: " + s);
      return System.currentTimeMillis(); 
    }
  }
}
