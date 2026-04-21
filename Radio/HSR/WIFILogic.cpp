
#include "WIFILogic.h"

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
IPAddress apIP(192,168,4,1);
IPAddress netMask(255,255,255,0);

bool modemPoweredOn = false;
unsigned long lastStatusUpdate = 0;
const char* AP_SSID = "HITSLASH-RADIO-SETUP";
String savedSSIDs[10];
String savedPasswords[10];
int savedCount = 0;
int selectedNetworkIndex = 0;
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

bool tryConnect(const char* ssid, const char* password) {
  DEBUG_PRINTF("Attempting to connect to SSID: %s\n", ssid);
  
  buildConnectingText(ssid);
  
  WiFi.disconnect();  // Clean up any previous connection attempt
  delay(100);
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
      //digitalWrite(LTE_MOSFET_PIN, LOW);// no this is wrong, because then it just turns off the router after connecting to it. bad.
      //modemPoweredOn = false;
      DEBUG_PRINTLN("Modem powered off- ACTUALLY NO I am not. just kidding.");
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

  while (!buttons[BTN_IDX_UP].pressed && !buttons[BTN_IDX_REFRESH].pressed) {
    updateButtons();
    display.clearDisplay();
    drawWrappedText("Powering modem. Press UP to connect to modem, but only when the modem light is blue. To boot offline, press REFRESH now.", 0, 20, 128, 10);
    display.display();
    delay(100);
  }

  if (buttons[BTN_IDX_REFRESH].pressed) {
    DEBUG_PRINTLN("Booting offline");
    digitalWrite(LTE_MOSFET_PIN, LOW);
    modemPoweredOn = false;
    WiFi.mode(WIFI_OFF);  // Explicitly disable WiFi
    offlineMode = true;
    // Clear button state to prevent it from affecting next boot
    buttons[BTN_IDX_REFRESH].pressed = false;
    buttons[BTN_IDX_REFRESH].held = false;
    return false;
  }

  // Wait for modem to boot
  for (int waitTime = 0; waitTime < 3000; waitTime += 100) {
      updateButtons();

      if (waitTime % 1000 == 0) {
          int secondsLeft = 3 - (waitTime/1000);
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

void enterSetupMode() {
  offlineMode = false; // reset offline mode in case we were there before
  DEBUG_PRINTF("ES: Free heap: %d\n", ESP.getFreeHeap());
  DEBUG_PRINTF("ES: Min free heap ever: %d\n", ESP.getMinFreeHeap());
  DEBUG_PRINTLN("ES: 1 - start");
  currentMode = MODE_SETUP;
  scrollOffset = 0;
  buildSetupText();
  drawSetupScreen();
  DEBUG_PRINTLN("ES: 2 - display done");
  
  WiFi.setSleep(false);
  DEBUG_PRINTLN("ES: 3 - sleep off");
  WiFi.mode(WIFI_AP);
  DEBUG_PRINTLN("ES: 4 - mode set");
  delay(100);
  WiFi.softAPConfig(apIP, apIP, netMask);
  DEBUG_PRINTLN("ES: 5 - AP config");
  WiFi.softAP(AP_SSID);
  DEBUG_PRINTLN("ES: 6 - AP started");
  DEBUG_PRINTF("ES: heap after AP start: %d\n", ESP.getFreeHeap());
  delay(500);
  
  dnsServer.start(53, "*", apIP);
  DEBUG_PRINTLN("ES: 7 - DNS started");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/manage", handleManage);
  server.on("/clear", handleClear);
  server.onNotFound(handleNotFound);
  DEBUG_PRINTLN("ES: 8 - routes registered");
  
  server.begin();
  DEBUG_PRINTLN("ES: 9 - server started");
  DEBUG_PRINTF("ES: heap after server begin: %d\n", ESP.getFreeHeap());
  
  loadSavedNetworks();
  DEBUG_PRINTLN("ES: 10 - done");
}