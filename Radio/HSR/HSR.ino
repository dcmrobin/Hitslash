/********************************************************************
  HITSLASH RADIO - WITH BATTERY MONITOR AND FIXED AP BEHAVIOR
********************************************************************/
#include "HelperFunctions.h"

// ================= SETUP =============================

void setup() {
  Serial.begin(115200);
  delay(2000);

  DEBUG_PRINTLN("\n\n=== HITSLASH RADIO ===\n");

  // Initialize pins
  pinMode(BTN_UP,      INPUT_PULLUP);
  pinMode(BTN_DOWN,    INPUT_PULLUP);
  pinMode(BTN_REFRESH, INPUT_PULLUP);
  pinMode(BTN_LEFT,    INPUT_PULLUP);
  pinMode(BTN_RIGHT,   INPUT);      // GPIO37 has external 10k pull-up, no internal

  pinMode(LTE_MOSFET_PIN, OUTPUT);
  digitalWrite(LTE_MOSFET_PIN, LOW);
  modemPoweredOn = false;

  // Initialize display
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

  // Init spectrum analyzer
  initSpectrum();

  // Check for forced setup mode
  bool setupModeForced = !digitalRead(BTN_REFRESH);

  if (setupModeForced) {
    DEBUG_PRINTLN("Forced setup mode");
    enterSetupMode();
    return;
  }

  // Normal boot sequence
  DEBUG_PRINTLN("Normal boot sequence");

  loadSavedNetworks();

  if (connectToSavedNetworks()) {
    startRadio();
    return;
  }

  DEBUG_PRINTLN("Saved networks failed, trying modem...");
  if (connectToModem()) {
    startRadio();
    return;
  }

  DEBUG_PRINTLN("All connections failed - entering setup");
  buildConnectingText("Connection failed");
  delay(2000);
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

  updateBattery();
  checkSecretSequence();

  if (currentMode == MODE_SETUP) {
    dnsServer.processNextRequest();
    server.handleClient();
    handleButtons();
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
    }
  }

  if (currentMode == MODE_MANAGE_NETWORKS) {
    handleButtons();
  }

  if (currentMode == MODE_HELLDIVERS) {
    audio.loop();
    if (currentDisplay == DISPLAY_MAJOR_ORDER) drawHelldiversMajorOrder();
    else if (currentDisplay == DISPLAY_NEWS) drawHelldiversNews();
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

  if (currentMode == MODE_FM_RADIO) {
    handleFMRadioButtons();
  }

  if (currentDisplay == DISPLAY_SPECTRUM) {
    drawSpectrumScreen();
  } else if (millis() - lastStatusUpdate > 2000) {
    if (currentDisplay == DISPLAY_STATION) drawRadioScreen();
    else if (currentDisplay == DISPLAY_WIFI_INFO) drawWifiInfoScreen();
    lastStatusUpdate = millis();
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
