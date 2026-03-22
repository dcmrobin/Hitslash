#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Adafruit_MAX1704X.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "Audio.h"
#include "HelldiversLogic.h"
#include "DisplayLogic.h"
#include "WIFILogic.h"
#include "RadioAudioLogic.h"
#include "InfoTerminal.h"
#include "APIKeys.h"
#include "MP3Logic.h"
#include "SpectrumLogic.h"
#include "FMRadioLogic.h"          // ← FM radio (raw I2C, no SparkFun lib)

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

// ================= BATTERY MONITOR ===================

extern Adafruit_MAX17048 max17048;
extern float batteryVoltage;
extern float batteryPercent;
extern unsigned long lastBatteryRead;

// ================= MODES =============================

enum DeviceMode {
  MODE_SETUP,
  MODE_RADIO,
  MODE_CONNECTING,
  MODE_MANAGE_NETWORKS,
  MODE_HELLDIVERS,
  MODE_INFO_TERMINAL,
  MODE_MP3,
  MODE_FM_RADIO
};

extern DeviceMode currentMode;

// ================= BUTTONS ===========================

#define BTN_UP      6
#define BTN_DOWN    5
#define BTN_REFRESH 14
#define BTN_LEFT    8
#define BTN_RIGHT   37

extern bool refreshHeld;
extern unsigned long refreshPressTime;

void initBattery();
void updateBattery();
void handleButtons();

#endif // HELPER_FUNCTIONS_H