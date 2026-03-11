#include "RadioAudioLogic.h"

Audio audio;
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
bool speakerEnabled = true;
int maxVolumeSpeakerOn = 10;
int maxVolumeSpeakerOff = 21;
int lastMaxVolume = 21;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

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
  // Scrolling right past last station → Info Terminal
  if (dir == 1 && currentStation == stationCount - 1) {
    currentMode    = MODE_INFO_TERMINAL;
    currentDisplay = DISPLAY_INFO_KEYBOARD;
    audio.stopSong();
    enterInfoTerminal();
    return;
  }
  // Scrolling left past first station → Info Terminal
  if (dir == -1 && currentStation == 0) {
    currentMode    = MODE_INFO_TERMINAL;
    currentDisplay = DISPLAY_INFO_KEYBOARD;
    audio.stopSong();
    enterInfoTerminal();
    return;
  }

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