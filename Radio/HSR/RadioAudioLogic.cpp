#include "RadioAudioLogic.h"

Audio audio;
const char* stations[] = {
  "https://hypr.website/hypr.mp3",
  "https://scenestream.io/necta64.mp3",
  "http://radio.chapter3-it.io/cvgm64",
  "http://stream.keygen-fm.ru:8082/listen.mp3",
  "https://relay.rainwave.cc/chiptune.mp3",
  "http://www.lmp.d2g.com:8003/;",
  "https://Kohina.Brona.dk/icecast/stream.ogg",
  "http://Oscar.SceneSat.com:8000/scenesat",
  "http://radio-paralax.de:8000/stream/1/;",
  "http://195.201.9.210:1541/stream/1/",
  "http://radio.modules.pl:8500/;",
  "http://server10.reliastream.com:9000/stream2_autodj"
};
const char* stationNames[] = {
  "HYPR", "NECTARINE", "CVGM", "KEYGEN", "RAINWAVE", "UKSCENE", "KOHINA", "SCENESAT", "PARALAX", "VGCLASSIC", "SHOUTCAST", "ARCADE"
};
const int stationCount = 12;
int currentStation = 0;
unsigned long lastReconnect = 0;
bool speakerEnabled = true;
int maxVolumeSpeakerOn = 15;
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
  if (currentMode != MODE_RADIO && currentMode != MODE_MP3) return;

  static int lastVol = -1;

  int raw = analogRead(POT_PIN);
  
  if (currentMode == MODE_RADIO) {
    int maxVol = speakerEnabled ? maxVolumeSpeakerOn : maxVolumeSpeakerOff;
    int vol = map(raw, 0, 4095, 0, maxVol);
    if (speakerEnabled && audio.getVolume() > maxVolumeSpeakerOn) {
      vol = maxVolumeSpeakerOn;
      audio.setVolume(vol);
      lastVol = vol;
    }
    if (vol != lastVol) {
      audio.setVolume(vol);
      lastVol = vol;
      if (currentDisplay == DISPLAY_STATION) drawRadioScreen();
      else if (currentDisplay == DISPLAY_WIFI_INFO) drawWifiInfoScreen();
    }
  }
  
  if (currentMode == MODE_MP3) {
    int maxVol = speakerEnabled ? 20 : 30;
    int vol = map(raw, 0, 4095, 0, maxVol);
    if (vol != lastVol) {
      mp3SetVolume(vol);
      lastVol = vol;
    }
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
  // ── Scrolling RIGHT past the last internet station → FM Radio ──────────────
  if (dir == 1 && currentStation == stationCount - 1) {
    audio.stopSong();
    //enterFMRadioMode();
    enterMP3Mode();
    return;
  }

  // ── Scrolling LEFT past the first internet station → Info Terminal ─────────
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