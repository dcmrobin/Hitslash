#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <vector>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LC709203F.h>  // Battery monitor library

#include "AudioFileSource.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "esp_wifi.h"

/* ================= CONFIG ================= */

#define AP_SSID "HITSLASH-RADIO-POCKET"

// OLED I2C pins (your hardwired connections)
#define OLED_SDA 9
#define OLED_SCL 10

// Battery monitor uses STEMMA QT (GPIO 3/4)
// These are the DEFAULT I2C pins for Feather S2
#define BAT_SDA 3
#define BAT_SCL 4

#define I2S_BCLK 11
#define I2S_LRCK 12
#define I2S_DATA 13

// 2-POSITION STATION SWITCH PINS
#define SWITCH_STATION1 5  // Left position
#define SWITCH_STATION2 6  // Right position

// Station configurations
struct Station {
  const char* name;
  const char* host;
  const char* path;
  int port;
  const char* metadataApi;  // API endpoint for track metadata
};

Station stations[] = {
  {"Nectarine Radio", "scenestream.io", "/necta64.mp3", 443, "https://scenestream.net/demovibes/xml/queue/"},
  {"Demovibes Radio", "radio.chapter3-it.io", "/cvgm64", 443, "https://scenestream.net/demovibes/xml/queue/"}
  //https://slacker.cvgm.net/cvgm64 is the one that works sometimes. The one it is set to right now, I haven't tested. I doubt it'll work, because it's not https.
};

// Track metadata structure
struct TrackInfo {
  String artist;
  String title;
  int lengthSec;
  bool isJingle;
  bool hasMetadata;
};

// WiFi credential structure
struct WiFiCredential {
  String ssid;
  String password;
  int authType;        // 0=WPA2, 1=Open
};

// Current station index
int currentStation = 0;
float currentVolume = 0.55;  // Fixed volume
bool isSwitchingStation = false;  // Flag to prevent race conditions
bool shouldExitSetup = false;
bool showTrackInfo = true;  // Start with track info
unsigned long trackInfoStartTime = 0;  // When track info was first shown

// Buffer configuration - Using PSRAM
#define USE_PSRAM
#define BUFFER_SIZE (64 * 1024)  // 64KB buffer in PSRAM - smaller to start faster
#define MIN_BUFFER_FILL_PERCENT 10  // Start playing when buffer is 10% full

// Authentication types
#define AUTH_WPA2 0
#define AUTH_OPEN 1

/* ================= GLOBALS ================= */

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;

// ESP32-S2 has TWO hardware I2C buses: Wire (I2C0) and Wire1 (I2C1)
// OLED: Wire1 on GPIO 9/10
// Battery: Wire on GPIO 3/4

TwoWire I2C_OLED = TwoWire(1);  // Use I2C1 for OLED
Adafruit_SSD1306 display(128, 64, &I2C_OLED);

// Battery monitor uses default Wire (I2C0) on GPIO 3/4
Adafruit_LC709203F lc;

// Battery status variables
float batteryPercent = 0.0;
float batteryTemp = 0.0;
unsigned long lastBatteryRead = 0;
const unsigned long BATTERY_READ_INTERVAL = 5000;  // Read every 5 seconds

AudioGeneratorMP3 *mp3 = nullptr;
AudioOutputI2S *out = nullptr;

// Current track metadata
TrackInfo currentTrack;

// Custom buffer with parallel filling
class ParallelAudioBuffer : public AudioFileSource {
public:
  ParallelAudioBuffer(size_t bufferSize) {
    #ifdef USE_PSRAM
      if (psramFound()) {
        buffer = (uint8_t*)ps_malloc(bufferSize);
        Serial.printf("Allocated %d bytes in PSRAM\n", bufferSize);
      } else {
        buffer = (uint8_t*)malloc(bufferSize);
        Serial.printf("Allocated %d bytes in RAM\n", bufferSize);
      }
    #else
      buffer = (uint8_t*)malloc(bufferSize);
      Serial.printf("Allocated %d bytes in RAM\n", bufferSize);
    #endif
    
    if (!buffer) {
      Serial.println("Failed to allocate buffer!");
      return;
    }
    
    this->bufferSize = bufferSize;
    readPos = 0;
    writePos = 0;
    bytesAvailable = 0;
    connectionActive = false;
    totalBytesRead = 0;
    currentHost = nullptr;
    currentPath = nullptr;
  }
  
  ~ParallelAudioBuffer() {
    if (buffer) free(buffer);
    if (client && client->connected()) {
      client->stop();
      delete client;
    }
  }
  
  bool beginStream(const char* host, const char* path, int port = 443) {
    currentHost = host;
    currentPath = path;
    
    // Clean up any existing connection first
    if (client && client->connected()) {
      client->stop();
      delete client;
      client = nullptr;
    }
    
    if (!client) {
      client = new WiFiClientSecure();
      client->setInsecure();
      client->setTimeout(10000);
    }
    
    Serial.printf("Connecting to %s:%d...\n", host, port);
    if (!client->connect(host, port)) {
      Serial.println("Connection failed");
      return false;
    }
    
    Serial.println("Connected, sending request...");
    client->printf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "User-Agent: HitslashRadio/2.0\r\n"
      "Accept: audio/mpeg\r\n"
      "Connection: keep-alive\r\n\r\n",
      path, host
    );
    
    // Wait for response
    unsigned long start = millis();
    while (client->connected() && (millis() - start < 5000)) {
      if (client->available()) {
        String line = client->readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) {
          // Empty line indicates end of headers
          Serial.println("Headers received, ready to stream");
          connectionActive = true;
          lastFillTime = millis();
          
          // Clear buffer when starting new stream
          readPos = 0;
          writePos = 0;
          bytesAvailable = 0;
          totalBytesRead = 0;
          
          return true;
        }
        
        // Check for HTTP error
        if (line.startsWith("HTTP/1.")) {
          if (line.indexOf("200") == -1 && line.indexOf("ICY 200") == -1) {
            Serial.printf("HTTP error: %s\n", line.c_str());
            return false;
          }
        }
      }
      delay(1);
    }
    
    Serial.println("Timeout waiting for headers");
    return false;
  }
  
  // This should be called frequently to fill the buffer
  bool fillBuffer() {
    if (!connectionActive || !client || !client->connected()) {
      return false;
    }
    
    // Don't fill if buffer is nearly full
    if (bytesAvailable > bufferSize * 0.9) {
      return true;
    }
    
    bool didRead = false;
    int available = client->available();
    
    if (available > 0) {
      int space = bufferSize - bytesAvailable;
      int bytesToRead = min(available, space);
      
      if (bytesToRead > 0) {
        if (writePos + bytesToRead > bufferSize) {
          // Wrap around write
          int firstPart = bufferSize - writePos;
          int secondPart = bytesToRead - firstPart;
          
          int r1 = client->read(buffer + writePos, firstPart);
          int r2 = client->read(buffer, secondPart);
          
          int total = r1 + r2;
          if (total > 0) {
            writePos = r2;
            bytesAvailable += total;
            totalBytesRead += total;
            didRead = true;
          }
        } else {
          // Contiguous write
          int r = client->read(buffer + writePos, bytesToRead);
          if (r > 0) {
            writePos = (writePos + r) % bufferSize;
            bytesAvailable += r;
            totalBytesRead += r;
            didRead = true;
          }
        }
        
        lastFillTime = millis();
      }
    }
    
    // Check for timeout
    if (millis() - lastFillTime > 15000) {
      Serial.println("Stream timeout");
      connectionActive = false;
      return false;
    }
    
    return didRead;
  }
  
  uint32_t read(void* data, uint32_t len) override {
    if (bytesAvailable == 0) {
      // Try to fill buffer if empty
      fillBuffer();
      if (bytesAvailable == 0) return 0;
    }
    
    uint32_t toRead = min(len, (uint32_t)bytesAvailable);
    
    // Handle circular buffer read
    if (readPos + toRead > bufferSize) {
      // Wrap around read
      int firstPart = bufferSize - readPos;
      int secondPart = toRead - firstPart;
      
      memcpy(data, buffer + readPos, firstPart);
      memcpy((uint8_t*)data + firstPart, buffer, secondPart);
      
      readPos = secondPart;
    } else {
      // Contiguous read
      memcpy(data, buffer + readPos, toRead);
      readPos = (readPos + toRead) % bufferSize;
    }
    
    bytesAvailable -= toRead;
    
    return toRead;
  }
  
  bool seek(int32_t pos, int dir) override {
    return false; // Not seekable
  }
  
  bool close() override {
    if (client) {
      client->stop();
      delete client;
      client = nullptr;
    }
    connectionActive = false;
    return true;
  }
  
  bool isOpen() override {
    return connectionActive;
  }
  
  int getFillPercent() {
    return bufferSize > 0 ? (bytesAvailable * 100) / bufferSize : 0;
  }
  
  bool hasMinData() {
    return getFillPercent() >= MIN_BUFFER_FILL_PERCENT;
  }
  
  size_t getBytesAvailable() {
    return bytesAvailable;
  }
  
  const char* getCurrentHost() { return currentHost; }
  const char* getCurrentPath() { return currentPath; }
  
  // Clear the buffer completely
  void clearBuffer() {
    readPos = 0;
    writePos = 0;
    bytesAvailable = 0;
    totalBytesRead = 0;
  }
  
private:
  uint8_t* buffer = nullptr;
  size_t bufferSize = 0;
  size_t readPos = 0;
  size_t writePos = 0;
  size_t bytesAvailable = 0;
  WiFiClientSecure* client = nullptr;
  bool connectionActive = false;
  unsigned long lastFillTime = 0;
  unsigned long totalBytesRead = 0;
  const char* currentHost = nullptr;
  const char* currentPath = nullptr;
};

ParallelAudioBuffer* audioBuffer = nullptr;

/* ================= WIFI CREDENTIALS MANAGEMENT ================= */

std::vector<WiFiCredential> wifiCredentials;

// Helper function to escape HTML special characters
String htmlEscape(const String& input) {
  String output = input;
  output.replace("&", "&amp;");
  output.replace("<", "&lt;");
  output.replace(">", "&gt;");
  output.replace("\"", "&quot;");
  output.replace("'", "&#39;");
  return output;
}

void loadWiFiCredentials() {
  wifiCredentials.clear();
  
  prefs.begin("wifi", true);
  int count = prefs.getInt("count", 0);
  
  for (int i = 0; i < count; i++) {
    WiFiCredential cred;
    String prefix = "cred" + String(i) + "_";
    cred.ssid = prefs.getString((prefix + "ssid").c_str(), "");
    cred.password = prefs.getString((prefix + "pass").c_str(), "");
    cred.authType = prefs.getInt((prefix + "auth").c_str(), AUTH_WPA2);
    
    if (cred.ssid.length() > 0) {
      wifiCredentials.push_back(cred);
    }
  }
  
  prefs.end();
}

void saveWiFiCredentials() {
  prefs.begin("wifi", false);
  
  // Clear all existing keys first
  String key;
  for (int i = 0; i < 20; i++) {
    key = "cred" + String(i) + "_ssid";
    if (prefs.isKey(key.c_str())) {
      prefs.remove(key.c_str());
    }
    key = "cred" + String(i) + "_pass";
    if (prefs.isKey(key.c_str())) {
      prefs.remove(key.c_str());
    }
    key = "cred" + String(i) + "_auth";
    if (prefs.isKey(key.c_str())) {
      prefs.remove(key.c_str());
    }
  }
  
  prefs.putInt("count", wifiCredentials.size());
  
  for (size_t i = 0; i < wifiCredentials.size(); i++) {
    String prefix = "cred" + String(i) + "_";
    prefs.putString((prefix + "ssid").c_str(), wifiCredentials[i].ssid);
    prefs.putString((prefix + "pass").c_str(), wifiCredentials[i].password);
    prefs.putInt((prefix + "auth").c_str(), wifiCredentials[i].authType);
  }
  
  prefs.end();
}

void addWiFiCredential(const WiFiCredential& cred) {
  // Check if SSID already exists
  for (size_t i = 0; i < wifiCredentials.size(); i++) {
    if (wifiCredentials[i].ssid == cred.ssid) {
      // Update existing
      wifiCredentials[i] = cred;
      saveWiFiCredentials();
      return;
    }
  }
  
  // Add new
  wifiCredentials.push_back(cred);
  saveWiFiCredentials();
}

void clearAllWiFiCredentials() {
  Serial.println("Clearing ALL WiFi credentials");
  wifiCredentials.clear();
  
  // Clear from preferences
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

/* ================= TRACK METADATA FUNCTIONS ================= */

// Parse track length string (MM:SS) to seconds
int parseTrackLength(const String& lengthStr) {
  int colonPos = lengthStr.indexOf(':');
  if (colonPos == -1) return 0;
  
  int minutes = lengthStr.substring(0, colonPos).toInt();
  int seconds = lengthStr.substring(colonPos + 1).toInt();
  
  return (minutes * 60 + seconds);
}

// Fetch track metadata from the current station's API
bool fetchTrackMetadata() {
  Serial.println("Fetching track metadata...");
  
  // Clear previous track info
  currentTrack.artist = "";
  currentTrack.title = "";
  currentTrack.lengthSec = 0;
  currentTrack.isJingle = false;
  currentTrack.hasMetadata = false;
  
  // Get the metadata API URL from current station
  String apiUrl = stations[currentStation].metadataApi;
  if (apiUrl.length() == 0) {
    Serial.println("No metadata API configured for this station");
    return false;
  }
  
  // Parse the URL to get host and path
  String host, path;
  if (apiUrl.startsWith("https://")) {
    apiUrl = apiUrl.substring(8); // Remove "https://"
  }
  
  int firstSlash = apiUrl.indexOf('/');
  if (firstSlash == -1) {
    host = apiUrl;
    path = "/";
  } else {
    host = apiUrl.substring(0, firstSlash);
    path = apiUrl.substring(firstSlash);
  }
  
  Serial.printf("Connecting to metadata API: %s%s\n", host.c_str(), path.c_str());
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000);
  
  if (!client.connect(host.c_str(), 443)) {
    Serial.println("Failed to connect to metadata API");
    return false;
  }
  
  // Request the queue XML
  client.printf(
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: HitslashRadio/2.0\r\n"
    "Accept: text/xml\r\n"
    "Connection: close\r\n\r\n",
    path.c_str(), host.c_str()
  );
  
  client.flush();
  
  // Skip HTTP headers
  unsigned long start = millis();
  bool inBody = false;
  String xmlContent = "";
  
  while (client.connected() && (millis() - start < 5000)) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      
      if (line == "\r") {
        inBody = true;
        continue;
      }
      
      if (inBody) {
        xmlContent += line;
      }
    }
    delay(1);
  }
  
  client.stop();
  
  if (xmlContent.length() == 0) {
    Serial.println("No XML data received");
    return false;
  }
  
  // Simple XML parsing
  int nowPos = xmlContent.indexOf("<now>");
  if (nowPos == -1) {
    Serial.println("No <now> tag found");
    return false;
  }
  
  int nowEndPos = xmlContent.indexOf("</now>", nowPos);
  if (nowEndPos == -1) {
    Serial.println("No closing </now> tag found");
    return false;
  }
  
  String nowSection = xmlContent.substring(nowPos, nowEndPos);
  
  // Extract artist
  int artistStart = nowSection.indexOf("<artist>");
  int artistEnd = nowSection.indexOf("</artist>", artistStart);
  if (artistStart != -1 && artistEnd != -1) {
    currentTrack.artist = nowSection.substring(artistStart + 8, artistEnd);
    currentTrack.artist.trim();
  }
  
  // Extract song title
  int songStart = nowSection.indexOf("<song");
  int songEnd = nowSection.indexOf("</song>", songStart);
  if (songStart != -1 && songEnd != -1) {
    // Get song text
    int textStart = nowSection.indexOf('>', songStart) + 1;
    currentTrack.title = nowSection.substring(textStart, songEnd);
    currentTrack.title.trim();
    
    // Get length attribute
    int lengthAttrStart = nowSection.indexOf("length=\"", songStart);
    if (lengthAttrStart != -1) {
      lengthAttrStart += 8; // Skip "length=\""
      int lengthAttrEnd = nowSection.indexOf("\"", lengthAttrStart);
      String lengthStr = nowSection.substring(lengthAttrStart, lengthAttrEnd);
      currentTrack.lengthSec = parseTrackLength(lengthStr);
      
      // Check if it's likely a jingle (short track)
      if (currentTrack.lengthSec > 0 && currentTrack.lengthSec <= 38) {
        currentTrack.isJingle = true;
      }
    }
  }
  
  currentTrack.hasMetadata = (currentTrack.artist.length() > 0 || currentTrack.title.length() > 0);
  
  if (currentTrack.hasMetadata) {
    Serial.printf("Track: %s - %s\n", currentTrack.artist.c_str(), currentTrack.title.c_str());
    Serial.printf("Length: %d seconds, Jingle: %s\n", currentTrack.lengthSec, currentTrack.isJingle ? "Yes" : "No");
  }
  
  return currentTrack.hasMetadata;
}

/* ================= BATTERY MONITOR FUNCTIONS ================= */

bool initBatteryMonitor() {
  Serial.println("Initializing battery monitor on GPIO 3/4 (Wire/I2C0)...");
  
  // Enable I2C power for Feather ESP32-S2 FIRST
  Serial.println("Enabling I2C power for Feather ESP32-S2...");
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  // turn on the I2C power by setting pin to opposite of 'rest state'
  pinMode(PIN_I2C_POWER, INPUT);
  delay(1);
  bool polarity = digitalRead(PIN_I2C_POWER);
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, !polarity);
  delay(100);
#endif

  // Initialize hardware I2C for battery monitor (Wire/I2C0 on GPIO 3/4)
  Wire.begin(BAT_SDA, BAT_SCL);
  Wire.setClock(100000);
  
  // Give some time for I2C to stabilize
  delay(100);
  
  // Initialize the LC709203F
  if (!lc.begin()) {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
    return false;
  }
  
  Serial.println(F("Found LC709203F"));
  
  // Configure the battery monitor
  lc.setThermistorB(3950);
  lc.setPackSize(LC709203F_APA_1000MAH);  // Set to 1000mAh for your battery
  lc.setAlarmVoltage(3.8);
  
  Serial.print("Thermistor B = "); Serial.println(lc.getThermistorB());
  Serial.print("Version: 0x"); Serial.println(lc.getICversion(), HEX);
  
  return true;
}

void updateBatteryStatus() {
  if (millis() - lastBatteryRead >= BATTERY_READ_INTERVAL) {
    lastBatteryRead = millis();
    
    // Read battery percentage
    batteryPercent = lc.cellPercent();
    
    // Read battery temperature (in Celsius)
    batteryTemp = lc.getCellTemperature();
    
    // Debug output
    Serial.printf("Battery: %.1f%%, Temp: %.1f°C, Voltage: %.3fV\n", 
                  batteryPercent, batteryTemp, lc.cellVoltage());
  }
}

/* ================= OLED FUNCTIONS ================= */

bool initOLED() {
  Serial.println("Initializing OLED display on GPIO 9/10 (Wire1/I2C1)...");
  
  // Initialize I2C1 for OLED on GPIO 9/10
  I2C_OLED.begin(OLED_SDA, OLED_SCL);
  I2C_OLED.setClock(100000);
  
  // Give some time for I2C to stabilize
  delay(100);
  
  // Initialize SSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at 0x3C, trying 0x3D...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("OLED initialization failed!");
      return false;
    } else {
      Serial.println("OLED found at 0x3D!");
    }
  } else {
    Serial.println("OLED found at 0x3C!");
  }
  
  display.display();
  delay(100);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hitslash Radio");
  display.println("Booting...");
  display.display();
  
  return true;
}

void oled(const String& l1, const String& l2 = "", const String& l3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(l1);
  display.println(l2);
  display.println(l3);
  display.display();
}

void oledStatus(const String& status, int bufferPercent, int rssi) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Station name (top line, truncated if too long)
  String stationName = stations[currentStation].name;
  if (stationName.length() > 21) {
    stationName = stationName.substring(0, 18) + "...";
  }
  display.println(stationName);
  
  // Status and buffer line
  display.print("Status: ");
  display.print(status);
  display.print(" ");
  display.print(bufferPercent);
  display.println("%");
  
  // WiFi signal strength
  display.print("WiFi: ");
  display.print(rssi);
  display.println(" dBm");
  
  // Battery status line
  display.print("Batt: ");
  display.print(batteryPercent, 0);  // 0 decimal places
  display.print("% ");
  display.print(batteryTemp, 0);     // 0 decimal places
  display.println("°C");
  
  display.display();
}

void oledTrackInfo() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Station name (top line, truncated)
  String stationName = stations[currentStation].name;
  if (stationName.length() > 21) {
    stationName = stationName.substring(0, 18) + "...";
  }
  display.println(stationName);
  
  // Artist (line 2-3, wrapped if needed)
  display.setCursor(0, 10);
  if (currentTrack.artist.length() > 0) {
    if (currentTrack.artist.length() > 21) {
      // Split long artist names
      String artistLine1 = currentTrack.artist.substring(0, 21);
      String artistLine2 = currentTrack.artist.substring(21);
      if (artistLine2.length() > 21) {
        artistLine2 = artistLine2.substring(0, 18) + "...";
      }
      display.println(artistLine1);
      display.setCursor(0, 18);
      display.println(artistLine2);
      display.setCursor(0, 26);
    } else {
      display.println(currentTrack.artist);
      display.setCursor(0, 18);
      display.println(); // Empty line for spacing
      display.setCursor(0, 26);
    }
  } else {
    display.println("Now playing:");
    display.setCursor(0, 18);
    display.println();
    display.setCursor(0, 26);
  }
  
  // Title (line 4-5, wrapped if needed)
  display.setCursor(0, 34);
  if (currentTrack.title.length() > 0) {
    if (currentTrack.title.length() > 21) {
      // Split long titles
      String titleLine1 = currentTrack.title.substring(0, 21);
      String titleLine2 = currentTrack.title.substring(21);
      if (titleLine2.length() > 21) {
        titleLine2 = titleLine2.substring(0, 18) + "...";
      }
      display.println(titleLine1);
      display.setCursor(0, 42);
      display.println(titleLine2);
    } else {
      display.println(currentTrack.title);
      display.setCursor(0, 42);
      display.println(); // Empty line
    }
  } else {
    display.println("No track info");
    display.setCursor(0, 42);
    display.println();
  }
  
  // Track length or jingle indicator and battery status (bottom line)
  display.setCursor(0, 56);
  if (currentTrack.isJingle) {
    display.print("Jingle ");
  } else if (currentTrack.lengthSec > 0) {
    int minutes = currentTrack.lengthSec / 60;
    int seconds = currentTrack.lengthSec % 60;
    display.printf("%d:%02d ", minutes, seconds);
  }
  
  // Add battery status
  display.print("Batt:");
  display.print(batteryPercent, 0);
  display.print("%");
  
  display.display();
}

/* ================= SWITCH HANDLING ================= */

// Read the 2-position station switch
int readStationSwitch() {
  if (digitalRead(SWITCH_STATION1) == LOW) return 1;  // Station 1
  if (digitalRead(SWITCH_STATION2) == LOW) return 2;  // Station 2
  return 0;  // No switch active (or in transition)
}

// Handle station switch changes
void handleStationSwitch() {
  static int lastSwitchPos = 0;
  int currentPos = readStationSwitch();
  
  if (currentPos == lastSwitchPos || isSwitchingStation) return;
  
  // If switch is in middle position (both open), do nothing
  if (currentPos == 0) {
    lastSwitchPos = currentPos;
    return;
  }
  
  int newStation = currentPos - 1;  // Convert to 0-based index
  
  if (currentStation != newStation) {
    isSwitchingStation = true;
    currentStation = newStation;
    if (currentStation == 0) {
      startRadio(true);  // TRUE = show track info for 10 seconds
    } else if (currentStation == 1) {
      startRadio(false);
    }
    isSwitchingStation = false;
  }
  
  lastSwitchPos = currentPos;
}

/* ================= SIMPLE CAPTIVE PORTAL ================= */

void handleRoot() {
  loadWiFiCredentials();
  
  String page = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Hitslash Radio Setup</title>"
  "<style>"
  "body {font-family: sans-serif; padding: 20px; max-width: 500px; margin: 0 auto; background: #f0f0f0;}"
  "h2 {color: #333; text-align: center;}"
  ".box {background: white; padding: 20px; border-radius: 8px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}"
  "label {display: block; margin: 10px 0 5px; font-weight: bold;}"
  "input, select {width: 100%; padding: 10px; margin-bottom: 15px; box-sizing: border-box; border: 1px solid #ccc; border-radius: 4px; font-size: 16px;}"
  "button {background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; margin: 5px 0;}"
  "button.delete {background: #f44336;}"
  "button:hover {opacity: 0.9;}"
  ".status {padding: 10px; margin: 10px 0; border-radius: 4px; text-align: center;}"
  ".warning {background: #fff3cd; color: #856404; border: 1px solid #ffeaa7;}"
  ".info {background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb;}"
  ".cred-item {border: 1px solid #ddd; padding: 10px; margin: 10px 0; border-radius: 4px; background: #f9f9f9;}"
  ".auth-badge {display: inline-block; padding: 2px 8px; border-radius: 3px; font-size: 12px; margin-left: 10px;}"
  ".wpa2-badge {background: #4CAF50; color: white;}"
  ".open-badge {background: #2196F3; color: white;}"
  ".section {margin: 20px 0; padding: 15px; border-left: 4px solid #4CAF50; background: #f9f9f9;}"
  "</style>"
  "</head><body>"
  "<h2>Hitslash Radio WiFi Setup</h2>"
  
  "<div class='box'>"
  "  <h3>Saved Networks (" + String(wifiCredentials.size()) + ")</h3>";
  
  if (wifiCredentials.empty()) {
    page += "<div class='status warning'>No saved networks</div>";
  } else {
    for (size_t i = 0; i < wifiCredentials.size(); i++) {
      String escapedSsid = htmlEscape(wifiCredentials[i].ssid);
      page += "<div class='cred-item'>";
      page += escapedSsid;
      
      // Show auth type badge
      if (wifiCredentials[i].authType == AUTH_OPEN) {
        page += "<span class='auth-badge open-badge'>Open</span>";
      } else {
        page += "<span class='auth-badge wpa2-badge'>WPA2</span>";
      }
      
      page += "</div>";
    }
  }
  
  page += 
  "  <form action='/clear' method='post' onsubmit='return confirm(\"Delete ALL saved WiFi networks? This cannot be undone.\")'>"
  "    <button type='submit' class='delete'>DELETE ALL SAVED NETWORKS</button>"
  "  </form>"
  "</div>"
  
  "<div class='box'>"
  "  <h3>Add New Network</h3>"
  "  <form action='/save' method='post'>"
  "  "
  "  <div class='section'>"
  "    <h4>Basic Settings</h4>"
  "    <label>WiFi Name (SSID): *</label>"
  "    <input type='text' name='ssid' required placeholder='e.g., HomeWiFi'>"
  "    "
  "    <label>Authentication Type: *</label>"
  "    <select name='authType' id='authType'>"
  "      <option value='0'>WPA2 Personal (password protected)</option>"
  "      <option value='1'>Open Network (no password)</option>"
  "</select>"
  "  </div>"
  "  "
  "  <div class='section'>"
  "    <h4>Security Settings</h4>"
  "    <label>Password:</label>"
  "    <input type='password' name='password' placeholder='WiFi password (required for WPA2)'>"
  "  </div>"
  "  "
  "  <button type='submit'>SAVE & CONNECT</button>"
  "  </form>"
  "</div>"
  
  "<div class='box'>"
  "  <h3>Device Info</h3>"
  "  <p><strong>Device:</strong> Hitslash Radio Pocket</p>"
  "  <p><strong>AP Name:</strong> " + String(AP_SSID) + "</p>"
  "  <p><strong>IP:</strong> " + WiFi.softAPIP().toString() + "</p>"
  "  <form action='/restart' method='post'>"
  "    <button type='submit'>EXIT SETUP & TRY TO CONNECT</button>"
  "  </form>"
  "</div>"
  
  "</body></html>";
  
  server.send(200, "text/html", page);
}

void handleSave() {
  Serial.println("Saving WiFi credentials");
  
  WiFiCredential cred;
  cred.ssid = server.arg("ssid");
  cred.authType = server.arg("authType").toInt();
  
  // Debug output
  Serial.printf("Auth type: %d\n", cred.authType);
  Serial.printf("Form fields received:\n");
  for (int i = 0; i < server.args(); i++) {
    Serial.printf("  %s: %s\n", server.argName(i).c_str(), server.arg(i).c_str());
  }
  
  // Handle different auth types
  if (cred.authType == AUTH_OPEN) {
    cred.password = "";  // No password for open networks
    Serial.printf("Open network: %s\n", cred.ssid.c_str());
  } else { // AUTH_WPA2
    cred.password = server.arg("password");
    Serial.printf("WPA2 network: %s\n", cred.ssid.c_str());
  }
  
  addWiFiCredential(cred);
  
  // Simple success page
  String page = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Saved Successfully</title>"
  "<style>"
  "body {font-family: sans-serif; padding: 40px; text-align: center; background: #f0f0f0;}"
  ".success {background: #d4edda; color: #155724; padding: 20px; border-radius: 8px; margin: 20px auto; max-width: 500px;}"
  "button {background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px;}"
  "</style>"
  "<script>"
  "setTimeout(function() {"
  "  window.location.href = '/';"
  "}, 3000);"
  "</script>"
  "</head><body>"
  "<h2>✓ WiFi Saved!</h2>"
  "<div class='success'>"
  "<p><strong>" + htmlEscape(cred.ssid) + "</strong></p>";
  
  if (cred.authType == AUTH_OPEN) {
    page += "<p>Open network saved</p>";
  } else {
    page += "<p>WPA2 network saved</p>";
  }
  
  page += 
  "<p>Returning to setup page...</p>"
  "</div>"
  "<button onclick=\"window.location.href='/'\">BACK NOW</button>"
  "</body></html>";
  
  server.send(200, "text/html", page);
}

void handleClear() {
  Serial.println("Clearing all WiFi credentials");
  
  clearAllWiFiCredentials();
  
  // Simple confirmation page
  String page = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>All Networks Deleted</title>"
  "<style>"
  "body {font-family: sans-serif; padding: 40px; text-align: center; background: #f0f0f0;}"
  ".warning {background: #fff3cd; color: #856404; padding: 20px; border-radius: 8px; margin: 20px auto; max-width: 500px;}"
  "button {background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 10px;}"
  "</style>"
  "<script>"
  "setTimeout(function() {"
  "  window.location.href = '/';"
  "}, 3000);"
  "</script>"
  "</head><body>"
  "<h2>All Networks Deleted</h2>"
  "<div class='warning'>"
  "<p>All saved WiFi networks have been deleted.</p>"
  "<p>You need to add a new network to connect.</p>"
  "</div>"
  "<button onclick=\"window.location.href='/'\">BACK TO SETUP</button>"
  "</body></html>";
  
  server.send(200, "text/html", page);
}

void handleRestart() {
  Serial.println("Restart requested - exiting setup mode");
  
  String page = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Exiting Setup</title>"
  "<style>"
  "body {font-family: sans-serif; padding: 40px; text-align: center; background: #f0f0f0;}"
  ".info {background: #d1ecf1; color: #0c5460; padding: 20px; border-radius: 8px; margin: 20px auto; max-width: 500px;}"
  "</style>"
  "</head><body>"
  "<h2>Exiting Setup Mode</h2>"
  "<div class='info'>"
  "<p>The device will now try to connect to saved networks.</p>"
  "<p>Please disconnect from this WiFi network.</p>"
  "</div>"
  "<p>The page will close in 5 seconds...</p>"
  "<script>"
  "setTimeout(function() {"
  "  window.close();"
  "}, 5000);"
  "</script>"
  "</body></html>";
  
  server.send(200, "text/html", page);
  
  delay(2000);
  
  // Exit captive portal
  shouldExitSetup = true;
}

/* ================= WIFI CONNECTION ================= */

bool connectToWiFi(const WiFiCredential& cred) {
  Serial.printf("\n=== Attempting connection to: %s ===\n", cred.ssid.c_str());
  
  oled("Connecting to:", cred.ssid.substring(0, 16));
  
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  if (cred.authType == AUTH_OPEN) {
    Serial.println("Open network");
    WiFi.begin(cred.ssid.c_str());
  } else {
    Serial.println("WPA2 Personal network");
    WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
  }
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
      WiFi.setSleep(false);
      esp_wifi_set_ps(WIFI_PS_NONE);
      Serial.printf("IP: %s, RSSI: %d dBm\n", 
                   WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
    
    delay(100);
  }
  
  Serial.println("WiFi connection timeout");
  return false;
}

/* ================= CAPTIVE PORTAL ================= */

void startCaptivePortal() {
  Serial.println("Starting simple captive portal");
  
  loadWiFiCredentials();
  
  oled("Setup Mode", "Connect WiFi", AP_SSID);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, NULL);

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(apIP);

  dnsServer.start(53, "*", apIP);

  // Simple routes
  server.on("/", HTTP_GET, []() {
    handleRoot();
  });
  
  server.on("/save", HTTP_POST, []() {
    handleSave();
  });
  
  server.on("/clear", HTTP_POST, []() {
    handleClear();
  });
  
  server.on("/restart", HTTP_POST, []() {
    handleRestart();
  });

  // Redirect everything else to main page
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web server started");
  Serial.println("Browse to: http://192.168.4.1");
  
  oled("Setup Mode", "IP:", apIP.toString());

  shouldExitSetup = false;
  
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (shouldExitSetup) {
      Serial.println("Exiting setup mode");
      break;
    }
    
    // Check if station switch is moved to exit setup mode
    if (readStationSwitch() == 1) {
      Serial.println("Exiting setup mode via switch");
      break;
    }
    
    delay(10);
  }
  
  // Clean up
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  
  oled("Reconnecting", "to WiFi...");
  delay(2000);
  
  // Try to connect to WiFi
  connectToBestWiFi();
}

/* ================= CONNECT TO BEST WIFI ================= */

void connectToBestWiFi() {
  loadWiFiCredentials();
  
  if (wifiCredentials.empty()) {
    Serial.println("No WiFi credentials saved");
    oled("No WiFi saved", "Starting setup", "mode...");
    delay(2000);
    startCaptivePortal();
    return;
  }
  
  Serial.printf("Trying %d saved networks...\n", wifiCredentials.size());
  
  for (size_t i = 0; i < wifiCredentials.size(); i++) {
    Serial.printf("\nTrying network %d/%d: %s (Type: %s)\n", 
                 i + 1, wifiCredentials.size(), 
                 wifiCredentials[i].ssid.c_str(),
                 wifiCredentials[i].authType == AUTH_OPEN ? "Open" : "WPA2");
    
    if (connectToWiFi(wifiCredentials[i])) {
      Serial.println("Successfully connected!");
      oled("Connected to:", wifiCredentials[i].ssid.substring(0, 16));
      delay(2000);
      return;
    }
    
    oled("Failed:", wifiCredentials[i].ssid.substring(0, 16), "Trying next...");
    delay(2000);
  }
  
  Serial.println("All networks failed, starting captive portal");
  oled("All networks", "failed", "Setup mode...");
  delay(2000);
  startCaptivePortal();
}

/* ================= RADIO CONTROL ================= */

void startRadio(bool showMetaData) {
  Serial.println("Starting radio");
  
  // Clear current track info
  currentTrack.artist = "";
  currentTrack.title = "";
  currentTrack.lengthSec = 0;
  currentTrack.isJingle = false;
  currentTrack.hasMetadata = false;
  
  // Show "Switching..." message
  oled(stations[currentStation].name, "Switching...");
  
  // Clean up previous instances
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  
  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }
  
  if (audioBuffer) {
    audioBuffer->close();
    delete audioBuffer;
    audioBuffer = nullptr;
  }
  
  delay(200);
  
  // Initialize audio output
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DATA);
  out->SetGain(currentVolume);
  
  // Initialize buffer
  audioBuffer = new ParallelAudioBuffer(BUFFER_SIZE);
  
  // Initialize MP3 decoder
  mp3 = new AudioGeneratorMP3();
  
  // Start stream connection
  Serial.println("Connecting to stream...");
  oled(stations[currentStation].name, "Connecting...");
  
  if (!audioBuffer->beginStream(stations[currentStation].host, 
                                 stations[currentStation].path,
                                 stations[currentStation].port)) {
    oled("Stream Error", "Can't connect");
    Serial.println("Failed to connect to stream");
    delay(2000);
    return;
  }
  
  // Pre-fill buffer
  Serial.println("Buffering...");
  oled(stations[currentStation].name, "Buffering...");
  
  unsigned long startFill = millis();
  while (millis() - startFill < 10000) {
    audioBuffer->fillBuffer();
    
    int fillPercent = audioBuffer->getFillPercent();
    Serial.printf("Buffer: %d%%\n", fillPercent);
    
    if (audioBuffer->hasMinData()) {
      Serial.println("Minimum data reached, starting playback");
      break;
    }
    
    delay(50);
  }
  
  // Start MP3 decoder
  if (!mp3->begin(audioBuffer, out)) {
    oled("Decoder Error");
    Serial.println("Failed to start MP3 decoder");
    return;
  }
  
  Serial.println("Radio started!");
  
  if (showMetaData) {
    Serial.println("Fetching track metadata...");
    // Fetch track metadata once
    if (strlen(stations[currentStation].metadataApi) > 0) {
      fetchTrackMetadata();
    } else {
      Serial.println("No metadata API configured for this station");
    }

    // Show track info screen and set timer
    showTrackInfo = true;
    trackInfoStartTime = millis();  // Start timer NOW
    
    // Display track info immediately
    if (currentTrack.hasMetadata) {
      oledTrackInfo();
    } else {
      // Show status screen instead if no metadata
      showTrackInfo = false;
      oledStatus("Playing", audioBuffer->getFillPercent(), WiFi.RSSI());
    }
    
    Serial.println("Showing track info for 10 seconds");
  }
}

/* ================= SETUP ================= */

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nHitslash Radio Pocket");
  Serial.println("=====================");
  
  // Initialize station switch pins
  pinMode(SWITCH_STATION1, INPUT_PULLUP);
  pinMode(SWITCH_STATION2, INPUT_PULLUP);
  
  // *** Initialize OLED first (Wire1/I2C1 on GPIO 9/10) ***
  Serial.println("\nInitializing OLED on I2C1 (GPIO 9/10)...");
  if (!initOLED()) {
    Serial.println("OLED init failed, continuing without display...");
  }
  
  // *** Initialize battery monitor (Wire/I2C0 on GPIO 3/4) ***
  Serial.println("\nInitializing battery monitor on I2C0 (GPIO 3/4)...");
  if (!initBatteryMonitor()) {
    Serial.println("Battery monitor init failed, continuing without battery data...");
  }
  
  // Check switch position on boot
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Checking switch...");
  display.display();
  
  int switchPos = readStationSwitch();
  Serial.printf("Boot switch position: %d\n", switchPos);
  
  // If station 2 is selected on boot, go to setup mode
  if (switchPos == 2) {
    Serial.println("Entering setup mode");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Setup Mode");
    display.println("Station 2 selected");
    display.display();
    delay(2000);
    startCaptivePortal();
  }
  
  // Try to connect to WiFi
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();
  
  connectToBestWiFi();
  
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected");
  display.println(WiFi.localIP().toString());
  display.display();
  delay(1000);
  
  // Start appropriate station
  if (switchPos == 1 || switchPos == 2) {
    currentStation = switchPos - 1;
  } else {
    currentStation = 0;
  }
  
  startRadio(true);
}

/* ================= LOOP ================= */

void loop() {
  static unsigned long lastStatusUpdate = 0;
  static unsigned long lastSwitchCheck = 0;
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastMetadataCheck = 0;
  static bool wasPlaying = false;
  
  // Update battery status periodically
  updateBatteryStatus();
  
  // Check station switch position
  if (millis() - lastSwitchCheck > 100 && !isSwitchingStation) {
    lastSwitchCheck = millis();
    handleStationSwitch();
  }
  
  if (isSwitchingStation) {
    delay(100);
    return;
  }
  
  // Check WiFi periodically
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected");
      oled("WiFi Lost", "Reconnecting...");
      WiFi.reconnect();
      
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
      }
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnection failed");
        connectToBestWiFi();
      }
    }
  }
  
  // Fill buffer if we have one
  if (audioBuffer) {
    audioBuffer->fillBuffer();
  }
  
  // Handle MP3 playback
  if (mp3) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        Serial.println("MP3 playback stopped");
        wasPlaying = false;
        
        // Try to restart if buffer has data
        if (audioBuffer && audioBuffer->getBytesAvailable() > 1024) {
          Serial.println("Restarting MP3 decoder...");
          if (!mp3->begin(audioBuffer, out)) {
            Serial.println("Failed to restart MP3 decoder");
          }
        } else {
          Serial.println("Buffer empty, restarting stream...");
          delay(1000);
          startRadio(false);  // FALSE = don't show track info
        }
      } else {
        wasPlaying = true;
      }
    } else {
      // Try to start MP3 if we have enough data
      if (audioBuffer && audioBuffer->hasMinData()) {
        Serial.println("Starting MP3 playback");
        if (!mp3->begin(audioBuffer, out)) {
          Serial.println("Failed to start MP3 decoder");
          delay(2000);
          startRadio(false);  // FALSE = don't show track info
        }
      } else if (wasPlaying) {
        Serial.println("Buffer underrun, waiting for data...");
        wasPlaying = false;
      }
    }
  }
  
  // Check if we should switch from track info to status screen
  if (showTrackInfo && (millis() - trackInfoStartTime > 10000)) {  // Changed to 10 seconds
    showTrackInfo = false;
    Serial.println("Switching to status screen");
  }
  
  // Update metadata periodically (every 60 seconds)
  //if (millis() - lastMetadataCheck > 60000) {
  //  lastMetadataCheck = millis();
  //  if (strlen(stations[currentStation].metadataApi) > 0) {
  //    fetchTrackMetadata();
  //  }
  //}
  //             NO
  
  // Update display
  if (millis() - lastStatusUpdate > 1000) {
    lastStatusUpdate = millis();
    
    if (audioBuffer) {
      int bufferPercent = audioBuffer->getFillPercent();
      int rssi = WiFi.RSSI();
      String status = mp3 && mp3->isRunning() ? "Playing" : "Buffering";
      
      if (showTrackInfo) {
        // Show track info screen
        oledTrackInfo();
      } else {
        // Show normal status screen with battery info
        oledStatus(status, bufferPercent, rssi);
      }
    }
  }
  
  delay(10);
}