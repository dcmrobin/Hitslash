/********************************************************************
  HITSLASH RADIO - WITH BATTERY MONITOR AND FIXED AP BEHAVIOR
********************************************************************/
#include "HelperFunctions.h"

// ================= SETUP =============================

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
  digitalWrite(SPEAKER_MOSFET_PIN, LOW); // Start with speakers enabled
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

  // Initialize MP3 player
  initMP3Player();
  
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

// ================= LOOP ==============================

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
    if (currentDisplay == DISPLAY_MAJOR_ORDER) {
      drawHelldiversMajorOrder();
    } else if (currentDisplay == DISPLAY_NEWS) {
      drawHelldiversNews();
    }
    handleHelldiversButtons();
  }

  if (currentMode == MODE_INFO_TERMINAL) {
    audio.loop();
    handleInfoTerminalButtons();
  }

  if (currentMode == MODE_MP3) {
    handleVolume();
    mp3CheckFinished();
    handleMP3Buttons();
  }
  
  delay(10);
}

// ================= AUDIO CALLBACKS ===================

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