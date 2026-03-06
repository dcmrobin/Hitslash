/********************************************************************
  HITSLASH RADIO - WITH BATTERY MONITOR AND FIXED AP BEHAVIOR
********************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MAX1704X.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"

// Debug macro - set to 1 to enable verbose debugging
#define DEBUG 0

#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// =====================================================
// ================= DISPLAY ===========================
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

#define OLED_DC     9
#define OLED_CS     10
#define OLED_RESET  8

Adafruit_SH1107 display =
  Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI,
                  OLED_DC, OLED_RESET, OLED_CS);

// =====================================================
// ================= BATTERY MONITOR ===================
// =====================================================

Adafruit_MAX17048 max17048;
float batteryVoltage = 0;
float batteryPercent = 0;
unsigned long lastBatteryRead = 0;

// =====================================================
// ================= MODES =============================
// =====================================================

enum DeviceMode {
  MODE_SETUP,
  MODE_RADIO,
  MODE_CONNECTING,
  MODE_MANAGE_NETWORKS,
  MODE_HELLDIVERS
};

DeviceMode currentMode = MODE_SETUP;

// =====================================================
// ================= BUTTONS ===========================
// =====================================================

#define BTN_UP      6
#define BTN_DOWN    5
#define BTN_REFRESH 14
#define BTN_LEFT    4
#define BTN_RIGHT   3

bool refreshHeld = false;
unsigned long refreshPressTime = 0;

// =====================================================
// ================= LTE / MODEM =======================
// =====================================================

#define LTE_MOSFET_PIN 15
#define SPEAKER_MOSFET_PIN 16
#define MODEM_SSID "hitslash-router"
#define MODEM_PASSWORD "hitslashradio"
bool modemPoweredOn = false;

// =====================================================
// =============== HELLDIVERS 2 VARS ===================
// =====================================================

#define SEQ_LENGTH 5
#define MAX_OBJECTIVES 5

String majorOrderTitle = "";
String majorOrderBrief = "";
String rewardText = "";

String objectiveText[MAX_OBJECTIVES];
int objectiveProgress[MAX_OBJECTIVES];
int objectiveTarget[MAX_OBJECTIVES];

int objectiveCount = 0;

int contentHeight = 0;

int inputSequence[SEQ_LENGTH];
int seqIndex = 0;

int secretSequence[SEQ_LENGTH] = {
  BTN_UP,
  BTN_RIGHT,
  BTN_DOWN,
  BTN_DOWN,
  BTN_DOWN
};


// For secret sequence debounce
bool prevUpState = HIGH;
bool prevRightState = HIGH;
bool prevDownState = HIGH;
bool prevLeftState = HIGH;
unsigned long lastUpDebounce = 0;
unsigned long lastRightDebounce = 0;
unsigned long lastDownDebounce = 0;
unsigned long lastLeftDebounce = 0;
const unsigned long secretDebounceTime = 120; // ms

String hdTitle = "";
String hdBrief = "";
String hdTask = "";
int hdProgress = 0;
long hdExpires = 0;

unsigned long lastHDUpdate = 0;
const unsigned long hdUpdateInterval = 60000;

unsigned long refreshPressStart = 0;
unsigned long lastRefreshTrigger = 0;

const unsigned long longPressTime = 2000;     // 2s to exit Helldivers
const unsigned long refreshInterval = 1000;   // 1s between refreshes when held

int hdScrollOffset = 0;

// =====================================================
// ================= WIFI ==============================
// =====================================================

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

unsigned long lastStatusUpdate = 0;
const char* AP_SSID = "HITSLASH-RADIO-SETUP";
IPAddress apIP(192,168,4,1);
IPAddress netMask(255,255,255,0);

// Saved networks list
String savedSSIDs[10];
String savedPasswords[10];
int savedCount = 0;
int selectedNetworkIndex = 0;

// HTML page for captive portal
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <title>HITSLASH Radio Setup</title>
    <style>
        body { 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif; 
            text-align: center; 
            margin: 0; 
            padding: 20px; 
            background: #1a1a1a; 
            color: white; 
            min-height: 100vh;
        }
        .container { 
            max-width: 400px; 
            margin: 0 auto; 
            padding: 20px; 
        }
        h1 { color: #4CAF50; }
        .form-group {
            margin-bottom: 15px;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #ccc;
        }
        input, select { 
            width: 100%; 
            padding: 12px; 
            border-radius: 5px; 
            border: 1px solid #444;
            background: #333;
            color: white;
            box-sizing: border-box;
        }
        button { 
            background: #4CAF50; 
            color: white; 
            padding: 12px 20px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            width: 100%;
            margin: 5px 0;
            font-size: 16px;
        }
        button.danger { background: #f44336; }
        button.secondary { background: #555; }
        .info { color: #888; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>HITSLASH Radio</h1>
        <h2>WiFi Setup</h2>
        
        <form action="/save" method="POST" target="hidden-form">
            <div class="form-group">
                <label>Network Name (SSID)</label>
                <input type="text" name="ssid" required>
            </div>
            <div class="form-group">
                <label>Password</label>
                <input type="password" name="password">
            </div>
            <button type="submit">Save and Connect</button>
        </form>
        
        <button class="secondary" onclick="window.location.href='/manage'">Manage Saved Networks</button>
        
        <div class="info">
            <p>After saving, the radio will attempt to connect.</p>
        </div>
    </div>
    
    <iframe name="hidden-form" style="display:none"></iframe>
</body>
</html>
)rawliteral";

// =====================================================
// ================= AUDIO =============================
// =====================================================

Audio audio;

#define I2S_BCLK 12
#define I2S_LRC  11
#define I2S_DOUT 13

#define POT_PIN A0

const char* stations[] = {
  "https://hypr.website/hypr.mp3",
  "https://scenestream.io/necta64.mp3",
  "http://radio.chapter3-it.io/cvgm64",
  "http://stream.keygen-fm.ru:8082/listen.mp3"
};

const char* stationNames[] = {
  "HYPR", "NECTA", "CVGM", "KEYGEN"
};

const int stationCount = 4;
int currentStation = 0;

unsigned long lastReconnect = 0;

// =====================================================
// ================= SCREEN STATES =====================
// =====================================================

enum DisplayMode {
  DISPLAY_STATION,
  DISPLAY_WIFI_INFO,
  DISPLAY_SPEAKER_CTRL,
  DISPLAY_NETWORK_LIST
};

DisplayMode currentDisplay = DISPLAY_STATION;
bool speakerEnabled = true;
int maxVolumeSpeakerOn = 10;
int maxVolumeSpeakerOff = 21;
int lastMaxVolume = 21;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

// =====================================================
// ================= SCROLL SYSTEM =====================
// =====================================================

#define MAX_LINES 20
String lines[MAX_LINES];
int totalLines = 0;
int scrollOffset = 0;
int lineHeight = 10;
int maxVisibleLines = 12;

// =====================================================
// ================= BATTERY FUNCTIONS =================
// =====================================================

void initBattery() {
  DEBUG_PRINTLN("Initializing MAX17048 battery monitor...");
  if (!max17048.begin()) {
    DEBUG_PRINTLN("Could not find MAX17048 battery monitor!");
  } else {
    DEBUG_PRINTLN("MAX17048 found!");
  }
}

void updateBattery() {
  if (millis() - lastBatteryRead > 1000) { // Update every second
    batteryPercent = max17048.cellPercent();
    batteryVoltage = max17048.cellVoltage();
    lastBatteryRead = millis();
    
    DEBUG_PRINTF("Battery: %.2fV %.1f%%\n", batteryVoltage, batteryPercent);
  }
}

void drawBatteryIcon(int x, int y) {
  // Draw battery outline
  display.drawRect(x, y, 25, 10, SH110X_WHITE);
  display.drawRect(x + 25, y + 2, 3, 6, SH110X_WHITE);
  
  // Fill based on percentage
  float pct = constrain(batteryPercent, 0, 100);
  int fillWidth = map(pct, 0, 100, 0, 23);
  if (fillWidth > 0) {
    if (batteryPercent > 20) {
      display.fillRect(x + 1, y + 1, fillWidth, 8, SH110X_WHITE);
    } else {
      // Low battery - fill with blinking or just outline
      display.fillRect(x + 1, y + 1, fillWidth, 8, SH110X_WHITE);
    }
  }
  
  // Show percentage text
  display.setCursor(x + 30, y);
  display.setTextSize(1);
  display.print((int)batteryPercent);
  display.print("%");
}

// =====================================================
// ================= SAVED NETWORKS ====================
// =====================================================

void loadSavedNetworks() {
  DEBUG_PRINTLN("Loading saved networks...");
  preferences.begin("wifi", true);
  savedCount = preferences.getInt("count", 0);
  
  for (int i = 0; i < savedCount && i < 10; i++) {
    savedSSIDs[i] = preferences.getString(("ssid_" + String(i)).c_str(), "");
    savedPasswords[i] = preferences.getString(("pass_" + String(i)).c_str(), "");
    DEBUG_PRINTF("Loaded %d: %s\n", i, savedSSIDs[i].c_str());
  }
  
  preferences.end();
  DEBUG_PRINTF("Total saved networks: %d\n", savedCount);
}

void saveNetwork(String ssid, String password) {
  preferences.begin("wifi", false);

  int count = preferences.getInt("count", 0);

  // Prevent duplicates
  for (int i = 0; i < count && i < 10; i++) {
    String existing = preferences.getString(("ssid_" + String(i)).c_str(), "");
    if (existing == ssid) {
      DEBUG_PRINTLN("SSID already saved");
      preferences.end();
      return;
    }
  }

  if (count >= 10) {
    DEBUG_PRINTLN("Network storage full");
    preferences.end();
    return;
  }

  preferences.putString(("ssid_" + String(count)).c_str(), ssid);
  preferences.putString(("pass_" + String(count)).c_str(), password);
  preferences.putInt("count", count + 1);

  preferences.end();
  loadSavedNetworks();
}

void clearAllNetworks() {
  DEBUG_PRINTLN("Clearing all saved networks");
  
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  
  // Reload networks
  loadSavedNetworks();
  selectedNetworkIndex = 0;
}

void deleteNetwork(int index) {
  if (index < 0 || index >= savedCount) return;
  
  DEBUG_PRINTF("Deleting network %d: %s\n", index, savedSSIDs[index].c_str());
  
  preferences.begin("wifi", false);
  
  // Shift all networks after index
  for (int i = index + 1; i < savedCount; i++) {
    String ssid = preferences.getString(("ssid_" + String(i)).c_str(), "");
    String pass = preferences.getString(("pass_" + String(i)).c_str(), "");
    preferences.putString(("ssid_" + String(i-1)).c_str(), ssid);
    preferences.putString(("pass_" + String(i-1)).c_str(), pass);
  }
  
  // Decrease count
  int count = preferences.getInt("count", 0);
  preferences.putInt("count", count - 1);
  
  preferences.end();
  
  // Reload networks
  loadSavedNetworks();
  
  if (selectedNetworkIndex >= savedCount) {
    selectedNetworkIndex = savedCount - 1;
  }
  if (selectedNetworkIndex < 0) selectedNetworkIndex = 0;
}

// =====================================================
// ================= SETUP TEXT ========================
// =====================================================

void buildSetupText() {
  totalLines = 0;
  lines[totalLines++] = "HITSLASH RADIO";
  lines[totalLines++] = "Setup Mode";
  lines[totalLines++] = "";
  lines[totalLines++] = "Connect to:";
  lines[totalLines++] = AP_SSID;
  lines[totalLines++] = "";
  lines[totalLines++] = "Captive portal";
  lines[totalLines++] = "should appear.";
  lines[totalLines++] = "";
  lines[totalLines++] = "If not open:";
  lines[totalLines++] = "192.168.4.1";
}

void buildNetworkListText() {
  totalLines = 0;
  lines[totalLines++] = "Saved Networks";
  lines[totalLines++] = "-------------";
  
  if (savedCount == 0) {
    lines[totalLines++] = "No networks saved";
  } else {
    for (int i = 0; i < savedCount; i++) {
      String line = String(i+1) + ". " + savedSSIDs[i];
      if (i == selectedNetworkIndex) {
        line = "> " + line;
      }
      lines[totalLines++] = line;
    }
  }
  
  lines[totalLines++] = "";
  lines[totalLines++] = "UP/DOWN: Select";
  lines[totalLines++] = "LEFT: Delete";
  lines[totalLines++] = "RIGHT: Clear All";
  lines[totalLines++] = "Hold refresh restart";
}

void buildConnectingText(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Connecting...");
  display.setCursor(0, 35);
  display.println(message);
  display.display();
}

// =====================================================
// ================= DISPLAY FUNCTIONS =================
// =====================================================

void drawWrappedText(String text, int x, int y, int maxWidth, int lineHeight) {

  int cursorX = x;
  int cursorY = y;

  String word = "";

  for (int i = 0; i < text.length(); i++) {

    char c = text[i];

    if (c == ' ' || i == text.length() - 1) {

      if (i == text.length() - 1)
        word += c;

      int wordWidth = word.length() * 6;

      if (cursorX + wordWidth > maxWidth) {
        cursorX = x;
        cursorY += lineHeight;
      }

      display.setCursor(cursorX, cursorY);
      display.print(word + " ");

      cursorX += (word.length() + 1) * 6;

      word = "";
    } 
    else {
      word += c;
    }
  }
}

void drawSetupScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int i = 0; i < maxVisibleLines; i++) {
    int lineIndex = i + scrollOffset;
    if (lineIndex >= totalLines) break;
    display.setCursor(0, i * lineHeight);
    display.println(lines[lineIndex]);
  }
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawRadioScreen() {
  display.clearDisplay();
  
  // Station name (large)
  display.setTextSize(2);
  display.setCursor(0, 10);
  if (audio.isRunning()) {
    display.println(stationNames[currentStation]);
  } else if (!audio.isRunning() && millis() - lastReconnect > 5000){
    display.println("ERROR");
  }
  
  // Volume
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Vol: ");
  int vol = audio.getVolume();
  display.print(vol);
  
  // Volume bar
  int barWidth = map(vol, 0, 21, 0, 100);
  display.drawRect(0, 60, 100, 8, SH110X_WHITE);
  display.fillRect(0, 60, barWidth, 8, SH110X_WHITE);
  
  // Hint for button functions
  display.setCursor(0, 80);
  display.print("L/R: Station");
  display.setCursor(0, 92);
  display.print("U/D: Cycle screens");
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawWifiInfoScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("WiFi Info");
  display.println("---------");
  
  display.print("SSID: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.SSID());
  } else {
    display.println("Not Connected");
  }
  
  display.print("RSSI: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.RSSI());
    display.println(" dBm");
  } else {
    display.println("N/A");
  }
  
  display.print("IP: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP());
  } else {
    display.println("None");
  }
  
  display.println("---------");
  display.print("Station: ");
  display.println(stationNames[currentStation]);
  
  display.print("Volume: ");
  display.println(audio.getVolume());
  display.print("");
  display.setCursor(0, 80);
  display.print("Hold refresh for");
  display.setCursor(0, 92);
  display.print("network management.");
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawNetworkListScreen() {
  buildNetworkListText();
  
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int i = 0; i < maxVisibleLines; i++) {
    int lineIndex = i + scrollOffset;
    if (lineIndex >= totalLines) break;
    display.setCursor(0, i * lineHeight);
    display.println(lines[lineIndex]);
  }
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

// =====================================================
// ================= WIFI CONNECTION ===================
// =====================================================

bool tryConnect(const char* ssid, const char* password) {
  DEBUG_PRINTF("Attempting to connect to SSID: %s\n", ssid);
  
  buildConnectingText(ssid);
  
  WiFi.begin(ssid, password);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
    DEBUG_PRINT(".");
    // Update display with dots
    static int dotCount = 0;
    display.setCursor(0, 55);
    display.print("Connecting");
    for (int i = 0; i < dotCount; i++) display.print(".");
    display.print("   ");
    drawBatteryIcon(0, display.height() - 15); // Keep battery updated
    display.display();
    dotCount = (dotCount + 1) % 4;
  }
  
  bool connected = (WiFi.status() == WL_CONNECTED);
  DEBUG_PRINTLN("");
  
  if (connected) {
    DEBUG_PRINTLN("Connection SUCCESS!");
    // Turn off modem when connected to WiFi
    if (modemPoweredOn) {
      digitalWrite(LTE_MOSFET_PIN, LOW);
      modemPoweredOn = false;
      DEBUG_PRINTLN("Modem powered off");
    }
  } else {
    DEBUG_PRINTLN("Connection FAILED!");
  }
  
  return connected;
}

bool connectToSavedNetworks() {
  loadSavedNetworks();
  
  for (int i = 0; i < savedCount; i++) {
    DEBUG_PRINTF("Trying network %d: %s\n", i, savedSSIDs[i].c_str());
    
    if (tryConnect(savedSSIDs[i].c_str(), savedPasswords[i].c_str())) {
      DEBUG_PRINTLN("Connected to saved network!");
      return true;
    }
  }
  
  DEBUG_PRINTLN("No saved networks worked");
  return false;
}

bool connectToModem() {
  DEBUG_PRINTLN("Trying modem network...");
  
  // Power on the modem
  digitalWrite(LTE_MOSFET_PIN, HIGH);
  modemPoweredOn = true;
  DEBUG_PRINTLN("Modem powered on");
  
  // Wait for modem to boot
  for (int waitTime = 0; waitTime < 60000; waitTime += 100) {
      if (!digitalRead(BTN_REFRESH)) {
          DEBUG_PRINTLN("Modem boot bypassed - entering setup");
          digitalWrite(LTE_MOSFET_PIN, LOW);
          modemPoweredOn = false;
          return false;  // Will fall into setup
      }

      if (waitTime % 1000 == 0) {
          int secondsLeft = 60 - (waitTime/1000);
          buildConnectingText("Powering modem...");
          display.setCursor(0, 55);
          display.printf("Waiting %ds   ", secondsLeft);
          drawBatteryIcon(0, display.height() - 15);
          display.display();
      }

      delay(100);
  }
  
  return tryConnect(MODEM_SSID, MODEM_PASSWORD);
}

// =====================================================
// ================ HELLDIVERS 2 LOGIC =================
// =====================================================

void drawProgressBar(int x, int y, int width, int height, float percent) {
  if (percent < 0) percent = 0;
  if (percent > 1) percent = 1;
  display.drawRect(x, y, width, height, SH110X_WHITE);
  int fill = (width - 2) * percent;
  display.fillRect(x + 1, y + 1, fill, height - 2, SH110X_WHITE);
}

void drawHelldiversMajorOrder() {
  display.clearDisplay();
  int y = hdScrollOffset;
  display.setCursor(0, y);
  display.println("HELLDIVERS 2 DATA");
  y += 12;
  y = drawWrappedText(majorOrderTitle, 0, y, 128);
  y += 4;
  y = drawWrappedText(majorOrderBrief, 0, y, 128);
  y += 8;
  for (int i = 0; i < objectiveCount; i++) {
    y = drawWrappedText(objectiveText[i], 0, y, 128);
    float percent = 0;
    if (objectiveTarget[i] > 0)
      percent = (float)objectiveProgress[i] / objectiveTarget[i];
    drawProgressBar(0, y, 110, 8, percent);
    y += 10;
    display.setCursor(0, y);
    display.print(objectiveProgress[i]);
    display.print("/");
    display.println(objectiveTarget[i]);
    y += 12;
  }
  y = drawWrappedText(rewardText, 0, y, 128);
  contentHeight = y;
  // Constrain hdScrollOffset so content stays in view
  int minScroll = -(contentHeight + 200);
  if (minScroll > 0) minScroll = 0;
  hdScrollOffset = constrain(hdScrollOffset, minScroll, 0);
  display.display();
}

void fetchMajorOrder(bool force = false) {

  if (WiFi.status() != WL_CONNECTED) return;

  if (!force && millis() - lastHDUpdate < hdUpdateInterval)
    return;

  lastHDUpdate = millis();

  HTTPClient http;
  http.begin("https://api.helldivers2.dev/raw/api/v2/Assignment/War/801");
  http.addHeader("X-Super-Client", "HitslashRadio");
  http.addHeader("X-Super-Contact", "dcm.robin@gmail.com");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      parseMajorOrder(doc);
      JsonObject order = doc[0];
      hdTitle = order["setting"]["overrideTitle"].as<String>();
      hdBrief = order["setting"]["overrideBrief"].as<String>();
      hdTask = order["setting"]["taskDescription"].as<String>();
      hdProgress = order["progress"][0];
      hdExpires = order["expiresIn"];
    } else {
      majorOrderTitle = "API Parse Error";
      majorOrderBrief = String(err.c_str());
      rewardText = "";
      objectiveCount = 0;
    }
  } else {
    majorOrderTitle = "API Error";
    majorOrderBrief = "HTTP " + String(httpCode);
    rewardText = "";
    objectiveCount = 0;
  }
  http.end();
}

void registerSequence(int btn) {

  inputSequence[seqIndex++] = btn;

  if (seqIndex >= SEQ_LENGTH) {

    bool match = true;

    for (int i = 0; i < SEQ_LENGTH; i++) {
      if (inputSequence[i] != secretSequence[i]) {
        match = false;
        break;
      }
    }

    if (match) {

      currentMode = MODE_HELLDIVERS;
      hdScrollOffset = 0;

      fetchMajorOrder(true);
      drawHelldiversMajorOrder();
    }

    seqIndex = 0;
  }
}

void handleHelldiversButtons() {

  bool refreshPressed = !digitalRead(BTN_REFRESH);

  static bool downWasPressed = false;
  static bool upWasPressed = false;
  static unsigned long lastScrollTime = 0;
  const unsigned long scrollRepeatDelay = 120; // ms

  // Handle refresh/exit
  if (refreshPressed) {
    if (!refreshHeld) {
      refreshHeld = true;
      refreshPressStart = millis();
      lastRefreshTrigger = millis();
    }
    // Exit if held long enough
    if (millis() - refreshPressStart > longPressTime) {
      currentMode = MODE_RADIO;
      drawRadioScreen();
      refreshHeld = false;
      return;
    }
    // Controlled refresh while holding
    if (millis() - lastRefreshTrigger > refreshInterval) {
      fetchMajorOrder(true);
      drawHelldiversMajorOrder();
      lastRefreshTrigger = millis();
    }
  } else {
    refreshHeld = false;
  }

  // DOWN scroll (hold to repeat)
  if (!digitalRead(BTN_DOWN)) {
    if (!downWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset -= 10;
      // Calculate minScroll based on contentHeight
      int minScroll = -(contentHeight + 200);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      drawHelldiversMajorOrder();
      lastScrollTime = millis();
    }
    downWasPressed = true;
  } else {
    downWasPressed = false;
  }

  // UP scroll (hold to repeat)
  if (!digitalRead(BTN_UP)) {
    if (!upWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset += 10;
      int minScroll = -(contentHeight + 200);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      drawHelldiversMajorOrder();
      lastScrollTime = millis();
    }
    upWasPressed = true;
  } else {
    upWasPressed = false;
  }
}

void checkSecretSequence() {

  unsigned long now = millis();
  bool upState = digitalRead(BTN_UP);
  bool rightState = digitalRead(BTN_RIGHT);
  bool downState = digitalRead(BTN_DOWN);
  bool leftState = digitalRead(BTN_LEFT);

  // UP pressed
  if (prevUpState == HIGH && upState == LOW && (now - lastUpDebounce > secretDebounceTime)) {
    registerSequence(BTN_UP);
    lastUpDebounce = now;
  }

  // RIGHT pressed
  if (prevRightState == HIGH && rightState == LOW && (now - lastRightDebounce > secretDebounceTime)) {
    registerSequence(BTN_RIGHT);
    lastRightDebounce = now;
  }

  // DOWN pressed
  if (prevDownState == HIGH && downState == LOW && (now - lastDownDebounce > secretDebounceTime)) {
    registerSequence(BTN_DOWN);
    lastDownDebounce = now;
  }

  // LEFT pressed
  if (prevLeftState == HIGH && leftState == LOW && (now - lastLeftDebounce > secretDebounceTime)) {
    registerSequence(BTN_LEFT);
    lastLeftDebounce = now;
  }

  prevUpState = upState;
  prevRightState = rightState;
  prevDownState = downState;
  prevLeftState = leftState;
}

void parseMajorOrder(JsonDocument &doc) {
  objectiveCount = 0;
  // Defensive: check structure
  if (!doc[0].containsKey("setting")) {
    majorOrderTitle = "No Data";
    majorOrderBrief = "Major Order data missing.";
    rewardText = "";
    return;
  }
  majorOrderTitle = doc[0]["setting"]["overrideTitle"].as<String>();
  majorOrderBrief = doc[0]["setting"]["overrideBrief"].as<String>();
  int rewardAmount = doc[0]["setting"]["reward"]["amount"] | 0;
  rewardText = "Reward: " + String(rewardAmount) + " Medals";
  if (!doc[0]["setting"].containsKey("tasks") || !doc[0].containsKey("progress")) {
    objectiveCount = 0;
    return;
  }
  JsonArray tasks = doc[0]["setting"]["tasks"];
  JsonArray progress = doc[0]["progress"];
  int index = 0;
  for (JsonObject task : tasks) {
    if (index >= MAX_OBJECTIVES) break;
    int current = progress[index] | 0;
    int target = 0;
    // Loop through values to find the largest value greater than progress (likely the target)
    JsonArray values = task["values"];
    for (JsonVariant v : values) {
      int val = v.as<int>();
      if (val >= current && val > target) {
        target = val;
      }
    }
    if (current == 0) {
      target = 0;
    } else if (current == 1) {
      target = 1;
    }
    objectiveTarget[index] = target;
    objectiveProgress[index] = current;
    objectiveText[index] = "Objective " + String(index + 1);
    index++;
  }
  objectiveCount = index;
}

int drawWrappedText(String text, int x, int y, int maxWidth) {
  int cursorX = x;
  int cursorY = y;
  String word = "";
  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == ' ' || i == text.length() - 1) {
      if (i == text.length() - 1) word += c;
      int wordWidth = word.length() * 6;
      if (cursorX + wordWidth > maxWidth) {
        cursorX = x;
        cursorY += 10;
      }
      display.setCursor(cursorX, cursorY);
      display.print(word);
      cursorX += wordWidth + 6;
      word = "";
    } else {
      word += c;
    }
  }
  return cursorY + 10;
}



// =====================================================
// ================= RADIO FUNCTIONS ===================
// =====================================================

void startRadio() {
  DEBUG_PRINTLN("Starting radio...");
  
  // Turn off AP mode when entering radio mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    DEBUG_PRINTLN("AP mode disabled");
  }
  
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(12);
  audio.connecttohost(stations[currentStation]);
  
  currentMode = MODE_RADIO;
  currentDisplay = DISPLAY_STATION;
  drawRadioScreen();
}

void switchStation(int dir) {
  currentStation += dir;
  if (currentStation < 0) currentStation = stationCount - 1;
  if (currentStation >= stationCount) currentStation = 0;
  
  audio.stopSong();
  delay(200);
  audio.connecttohost(stations[currentStation]);
  
  if (currentDisplay == DISPLAY_STATION) {
    drawRadioScreen();
  } else {
    drawWifiInfoScreen();
  }
}

// =====================================================
// =============== SPEAKER CONTROL UI ==================
// =====================================================

void drawSpeakerControlScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Speaker");

  display.setTextSize(2);
  display.setCursor(0, 40);
  if (speakerEnabled) {
    display.print("[ ON  ]");
  } else {
    display.print("[ OFF ]");
  }

  display.setTextSize(1);
  display.setCursor(0, 80);
  display.print("L: Off   R: On");
  display.setCursor(0, 92);
  display.print("U/D: Cycle screens");

  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);

  display.display();
}

// =====================================================
// ================= BUTTON HANDLING ===================
// =====================================================

void handleButtons() {
  if (millis() - lastButtonPress < debounceTime) return;
  
  if (currentMode == MODE_RADIO) {
    // Radio mode buttons
    if (!digitalRead(BTN_UP)) {
      if (currentDisplay == DISPLAY_STATION) {
        currentDisplay = DISPLAY_WIFI_INFO;
        drawWifiInfoScreen();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        currentDisplay = DISPLAY_STATION;
        drawRadioScreen();
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        currentDisplay = DISPLAY_STATION;
        drawRadioScreen();
      }
      lastButtonPress = millis();
    }

    static bool downWasPressed = false;
    if (!digitalRead(BTN_DOWN)) {
      if (!downWasPressed) {
        downWasPressed = true;
        if (currentDisplay == DISPLAY_STATION) {
          currentDisplay = DISPLAY_SPEAKER_CTRL;
          drawSpeakerControlScreen();
        } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
          currentDisplay = DISPLAY_WIFI_INFO;
          drawWifiInfoScreen();
        } else if (currentDisplay == DISPLAY_WIFI_INFO) {
          currentDisplay = DISPLAY_STATION;
          drawRadioScreen();
        }
        lastButtonPress = millis();
      }
    } else {
      downWasPressed = false;
    }

    if (!digitalRead(BTN_LEFT)) {
      if (currentDisplay == DISPLAY_STATION) {
        switchStation(-1);
        lastButtonPress = millis();
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        // Toggle speaker off
        speakerEnabled = false;
        digitalWrite(SPEAKER_MOSFET_PIN, LOW);
        // Restore max volume
        lastMaxVolume = maxVolumeSpeakerOff;
        drawSpeakerControlScreen();
        lastButtonPress = millis();
      }
    }

    if (!digitalRead(BTN_RIGHT)) {
      if (currentDisplay == DISPLAY_STATION) {
        switchStation(1);
        lastButtonPress = millis();
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        // Toggle speaker on
        speakerEnabled = true;
        digitalWrite(SPEAKER_MOSFET_PIN, HIGH);
        // If volume is above 10, set to 10
        if (audio.getVolume() > maxVolumeSpeakerOn) {
          audio.setVolume(maxVolumeSpeakerOn);
        }
        lastMaxVolume = maxVolumeSpeakerOn;
        drawSpeakerControlScreen();
        lastButtonPress = millis();
      }
    }

    // Refresh button in radio mode - force update info screen
    if (!digitalRead(BTN_REFRESH)) {
      DEBUG_PRINTLN("Refresh button pressed - updating display");
      if (currentDisplay == DISPLAY_STATION) {
        drawRadioScreen();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        drawWifiInfoScreen();
        if (!refreshHeld) {
          refreshHeld = true;
          refreshPressTime = millis();
        } else if (millis() - refreshPressTime > 3000) {
          currentMode = MODE_MANAGE_NETWORKS;
          scrollOffset = 0;
        }
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        drawSpeakerControlScreen();
      }
      lastButtonPress = millis();
    } else {
      refreshHeld = false;
    }
  }
  
  if (currentMode == MODE_MANAGE_NETWORKS) {
    drawNetworkListScreen();
    // Network management mode buttons
    if (!digitalRead(BTN_UP) && selectedNetworkIndex > 0) {
      selectedNetworkIndex--;
      scrollOffset = 0;
      lastButtonPress = millis();
    }
    
    if (!digitalRead(BTN_DOWN) && selectedNetworkIndex < savedCount - 1) {
      selectedNetworkIndex++;
      scrollOffset = 0;
      lastButtonPress = millis();
    }
    
    if (!digitalRead(BTN_LEFT) && savedCount > 0) {
      // Delete selected network
      deleteNetwork(selectedNetworkIndex);
      lastButtonPress = millis();
    }
    
    if (!digitalRead(BTN_RIGHT)) {
      // Clear all networks
      clearAllNetworks();
      lastButtonPress = millis();
    }

    if (!digitalRead(BTN_REFRESH)) {
      if (!refreshHeld) {
        refreshHeld = true;
        refreshPressTime = millis();
      } else if (millis() - refreshPressTime > 5000) {
        ESP.restart();
      }
    } else {
      refreshHeld = false;
    }
  }
  
  if (currentMode == MODE_SETUP) {
    // Refresh button in setup mode - long press to restart
    if (!digitalRead(BTN_REFRESH)) {
      if (!refreshHeld) {
        refreshHeld = true;
        refreshPressTime = millis();
      } else if (millis() - refreshPressTime > 3000) {
        currentMode = MODE_MANAGE_NETWORKS;
        scrollOffset = 0;
      }
    } else {
      refreshHeld = false;
    }
    
    // Up/down for scrolling
    if (!digitalRead(BTN_UP) && scrollOffset > 0) {
      scrollOffset--;
      drawSetupScreen();
      lastButtonPress = millis();
    }
    
    if (!digitalRead(BTN_DOWN) && scrollOffset < totalLines - maxVisibleLines) {
      scrollOffset++;
      drawSetupScreen();
      lastButtonPress = millis();
    }
  }

  if (currentMode == MODE_HELLDIVERS) {
    if (!digitalRead(BTN_DOWN)) {

      hdScrollOffset += 10;

      if (hdScrollOffset > 80)
        hdScrollOffset = 80;

      drawHelldiversMajorOrder();
    }

    if (!digitalRead(BTN_UP)) {

      hdScrollOffset -= 10;

      if (hdScrollOffset < 0)
        hdScrollOffset = 0;

      drawHelldiversMajorOrder();
    }

    if (!digitalRead(BTN_REFRESH)) {

      display.clearDisplay();
      display.setCursor(0,20);
      display.println("CONTACTING SUPER");
      display.println("EARTH...");
      display.display();

      fetchMajorOrder(true);

      hdScrollOffset = 0;

      drawHelldiversMajorOrder();
    }
  }
}

// =====================================================
// ================= VOLUME CONTROL ====================
// =====================================================

void handleVolume() {
  if (currentMode != MODE_RADIO) return;
  
  static int lastVol = -1;
  
  int raw = analogRead(POT_PIN);
  int maxVol = speakerEnabled ? maxVolumeSpeakerOn : maxVolumeSpeakerOff;
  int vol = map(raw, 0, 4095, 0, maxVol);
  // If speaker just enabled, force volume to 10 if above
  if (speakerEnabled && audio.getVolume() > maxVolumeSpeakerOn) {
    vol = maxVolumeSpeakerOn;
    audio.setVolume(vol);
    lastVol = vol;
  }
  if (vol != lastVol) {
    audio.setVolume(vol);
    lastVol = vol;
    if (currentDisplay == DISPLAY_STATION) {
      drawRadioScreen();
    } else if (currentDisplay == DISPLAY_WIFI_INFO) {
      drawWifiInfoScreen();
    }
    // Do not redraw if on speaker control or other screens
  }
}

// =====================================================
// ================= CAPTIVE PORTAL HANDLERS ===========
// =====================================================

void handleRoot() {
  DEBUG_PRINTLN("Serving root page");
  server.send(200, "text/html", htmlPage);
}

void handleSave() {
  DEBUG_PRINTLN("Processing save request");
  
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    DEBUG_PRINTF("Saving: %s\n", ssid.c_str());
    
    // Save the network
    saveNetwork(ssid, password);
    
    // Send minimal response
    server.send(200, "text/html", 
                "<html><body style='background:#1a1a1a;color:white;text-align:center;padding:50px;'>"
                "<h2>WiFi Saved!</h2><p>Radio is connecting...</p></body></html>");
    
    // Try to connect
    if (tryConnect(ssid.c_str(), password.c_str())) {
      startRadio();
    } else {
      // Stay in setup mode
      buildConnectingText("Connection failed");
      delay(2000);
      drawSetupScreen();
    }
  } else {
    server.send(400, "text/html", "Missing SSID");
  }
}

void handleManage() {
  DEBUG_PRINTLN("Serving manage page");
  
  String page = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<style>body{font-family:Arial;text-align:center;padding:20px;background:#1a1a1a;color:white;}</style>";
  page += "</head><body>";
  page += "<h1>Saved Networks</h1>";
  
  if (savedCount == 0) {
    page += "<p>No networks saved</p>";
  } else {
    page += "<ul style='list-style:none;padding:0;text-align:left;'>";
    for (int i = 0; i < savedCount; i++) {
      page += "<li style='padding:10px;margin:5px;background:#333;border-radius:5px;'>";
      page += savedSSIDs[i];
      page += "</li>";
    }
    page += "</ul>";
  }
  
  page += "<p><a href='/clear' style='color:#f44336;'>Clear All Networks</a></p>";
  page += "<p><a href='/'>Back to Setup</a></p>";
  page += "</body></html>";
  
  server.send(200, "text/html", page);
}

void handleClear() {
  DEBUG_PRINTLN("Clearing all networks");
  clearAllNetworks();
  server.sendHeader("Location", "/manage");
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  // Only redirect GET requests, not POST
  if (server.method() == HTTP_GET) {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// =====================================================
// ================= SETUP MODE INIT ===================
// =====================================================

void enterSetupMode() {
  DEBUG_PRINTLN("Entering setup mode");
  
  currentMode = MODE_SETUP;
  scrollOffset = 0;
  buildSetupText();
  drawSetupScreen();
  
  // Configure WiFi AP mode
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(AP_SSID);
  
  // Start DNS and web servers
  dnsServer.start(53, "*", apIP);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/manage", handleManage);
  server.on("/clear", handleClear);
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  // Load saved networks
  loadSavedNetworks();
}

// =====================================================
// ================= SETUP =============================
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DEBUG_PRINTLN("\n\n=== HITSLASH RADIO ===\n");
  
  // Initialize I2C for battery monitor
  Wire.begin();
  
  // Initialize pins
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_REFRESH, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  pinMode(LTE_MOSFET_PIN, OUTPUT);
  pinMode(SPEAKER_MOSFET_PIN, OUTPUT);
  digitalWrite(LTE_MOSFET_PIN, LOW); // Start with modem off
  speakerEnabled = true;
  digitalWrite(SPEAKER_MOSFET_PIN, HIGH); // Start with speakers enabled
  modemPoweredOn = false;
  
  // Initialize display
  pinMode(OLED_RESET, OUTPUT);
  digitalWrite(OLED_RESET, LOW);
  delay(10);
  digitalWrite(OLED_RESET, HIGH);
  delay(100);
  
  display.begin(0, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("HITSLASH RADIO");
  display.println("Booting...");
  display.display();
  delay(1000);
  
  // Initialize battery monitor
  initBattery();
  
  // Check for forced setup mode
  bool setupModeForced = !digitalRead(BTN_REFRESH);
  
  if (setupModeForced) {
    DEBUG_PRINTLN("Forced setup mode");
    enterSetupMode();
    return;
  }
  
  // Normal boot sequence
  DEBUG_PRINTLN("Normal boot sequence");
  
  // Load saved networks
  loadSavedNetworks();
  
  // Try saved networks first
  if (connectToSavedNetworks()) {
    startRadio();
    return;
  }
  
  // If no saved networks or all failed, try modem
  DEBUG_PRINTLN("Saved networks failed, trying modem...");
  if (connectToModem()) {
    startRadio();
    return;
  }
  
  // If modem also fails, enter setup mode
  DEBUG_PRINTLN("All connections failed - entering setup");
  buildConnectingText("Connection failed");
  delay(2000);
  // When entering setup after all fails, ensure setup text is shown
  enterSetupMode();
}

// =====================================================
// ================= LOOP ==============================
// =====================================================

void loop() {
  static unsigned long loopCount = 0;
  loopCount++;
  
  if (loopCount % 10000 == 0) {
    DEBUG_PRINTF("Loop: %lu, Mode: %d, Modem: %s\n", 
                 loopCount, currentMode, modemPoweredOn ? "ON" : "OFF");
  }
  
  // Update battery reading
  updateBattery();
  
  // Always check for secret sequence to allow entering Helldivers mode
  checkSecretSequence();

  if (currentMode == MODE_SETUP) {
    dnsServer.processNextRequest();
    server.handleClient();
    handleButtons(); // Handle setup mode buttons
  }
  
  if (currentMode == MODE_RADIO) {
    audio.loop();
    
    if (!audio.isRunning() && millis() - lastReconnect > 5000) {
      DEBUG_PRINTLN("Audio stopped - reconnecting");
      audio.connecttohost(stations[currentStation]);
      lastReconnect = millis();
    }
    
    handleButtons();
    handleVolume();
    if (millis() - lastStatusUpdate > 2000) {
      if (currentDisplay == DISPLAY_STATION) {
        drawRadioScreen();
        lastStatusUpdate = millis();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        drawWifiInfoScreen();
        lastStatusUpdate = millis();
      }
      // Do not refresh display if on speaker control or other screens
    }
  }
  
  if (currentMode == MODE_MANAGE_NETWORKS) {
    handleButtons();
  }

  if (currentMode == MODE_HELLDIVERS) {
    audio.loop();
    drawHelldiversMajorOrder();
    handleHelldiversButtons();
  }
  
  delay(10);
}

// =====================================================
// ================= AUDIO CALLBACKS ===================
// =====================================================

void audio_info(const char *info) { 
  DEBUG_PRINT("audio_info: "); 
  DEBUG_PRINTLN(info); 
}

void audio_showstation(const char *info) { 
  DEBUG_PRINT("station: "); 
  DEBUG_PRINTLN(info); 
}

void audio_showstreamtitle(const char *info) { 
  DEBUG_PRINT("streamtitle: "); 
  DEBUG_PRINTLN(info); 
}

void audio_bitrate(const char *info) {
  DEBUG_PRINT("bitrate: "); 
  DEBUG_PRINTLN(info);
}