#include "RadioAudioLogic.h"
#include <freertos/queue.h>

Audio audio;

// FreeRTOS queue for MP3 volume commands (offloaded to Core 0)
static QueueHandle_t mp3VolumeQueue = NULL;
static TaskHandle_t mp3VolumeTaskHandle = NULL;

// Task that runs on Core 0 and handles MP3 volume changes
static void mp3VolumeTask(void* parameter) {
  int volume_cmd;
  while (true) {
    // Wait for volume command (blocks without consuming CPU)
    if (xQueueReceive(mp3VolumeQueue, &volume_cmd, portMAX_DELAY)) {
      mp3SetVolume(volume_cmd);  // This 100ms delay happens on Core 0, not main loop
    }
  }
}

// Initialize the MP3 volume background task
void initMP3VolumeTask() {
  // Create queue to hold volume commands
  mp3VolumeQueue = xQueueCreate(2, sizeof(int));
  
  // Create task on Core 0 with lower priority than main loop
  xTaskCreatePinnedToCore(
    mp3VolumeTask,           // Task function
    "MP3VolumeTask",         // Name
    2048,                    // Stack size (bytes)
    NULL,                    // Parameter
    1,                       // Priority (lower than default)
    &mp3VolumeTaskHandle,    // Task handle
    0                        // Core 0 (background)
  );
  DEBUG_PRINTLN("MP3 volume task initialized on Core 0");
}
const char* stations[] = {
  "https://hypr.website/hypr.mp3",
  "https://scenestream.io/necta64.mp3",
  "http://radio.chapter3-it.io/cvgm64",
  "http://stream.keygen-fm.ru:8082/listen.mp3",
  "https://relay.rainwave.cc/chiptune.mp3",
  "http://ZXM.cz:8000/zx",
  "http://www.lmp.d2g.com:8003/;",
  "https://Kohina.Brona.dk/icecast/stream.ogg",
  "http://Oscar.SceneSat.com:8000/scenesat",
  "http://radio-paralax.de:8000/stream/1/;",
  "http://195.201.9.210:1541/stream/1/",
  "http://radio.modules.pl:8500/;",
  "http://82.165.247.194:8250/server-02_main_tsa-radio-02_hiqh-quality",
  "https://Stream.Zeno.FM/nwmvh8t41k8uv",
  "https://RadioHyrule.com:8443/listen-lo",
  "http://legacy.ericade.net:8000/stream/2/",
  "https://Radio.ERB.pw/listen/subspace/radio-64.mp3"
};
const char* stationNames[] = {
  "HYPR", "NECTARINE", "CVGM", "KEYGEN", "RAINWAVE", "ZXMUSIC", "UKSCENE", "KOHINA", "SCENESAT", "PARALAX", "VGCLASSIC", "SHOUTCAST", "TSA", "PLAYPIXEL", "HYRULE", "ERICADE", "QUANTUM"
};
const int stationCount = 17;
int currentStation = 0;
unsigned long lastReconnect = 0;
bool speakerEnabled = true;
int maxVolumeSpeakerOn = 13;
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
  const int VOLUME_THRESHOLD = 1; // Only send command if volume changes by at least this much

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
    // Only queue volume command if volume changed by more than threshold
    if (abs(vol - lastVol) >= VOLUME_THRESHOLD) {
      // Queue the volume command to the background task on Core 0
      // This way the 100ms delay doesn't block the spectrum updates
      xQueueSendToBack(mp3VolumeQueue, &vol, 0);
      lastVol = vol;
    }
  }
}

void startRadio() {
  DEBUG_PRINTLN("Starting radio...");
  
  // Turn off AP mode when entering radio mode (only if WiFi was initialized)
  if (!offlineMode && (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    DEBUG_PRINTLN("AP mode disabled");
  }
  
  currentMode = MODE_RADIO;
  currentDisplay = DISPLAY_STATION;
  drawRadioScreen();
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  // Only try to connect to internet radio if WiFi is available
  if (!offlineMode) {
    audio.connecttohost(stations[currentStation]);
  }
}

void switchStation(int dir) {
  int timeTillConnect = 0;
  // ── Scrolling RIGHT past the last internet station → FM Radio ──────────────
  if (dir == 1 && currentStation == stationCount - 1) {
    audio.stopSong();
    //enterFMRadioMode();
    drawMP3ListScreen();
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
  
  if (currentDisplay == DISPLAY_STATION) {
    drawRadioScreen();
  } else {
    drawWifiInfoScreen();
  }

  audio.stopSong();
  while (timeTillConnect < (offlineMode ? 10 : 1000)) {
    updateButtons();
    timeTillConnect++;

    if (buttons[BTN_IDX_LEFT].pressed) {
      timeTillConnect = 0;
      switchStation(-1);
      break;
    } else if (buttons[BTN_IDX_RIGHT].pressed) {
      timeTillConnect = 0;
      switchStation(1);
      break;
    }

    if (timeTillConnect > 100) {
      timeTillConnect = 0;
      if (!offlineMode) {
        audio.connecttohost(stations[currentStation]);
      }
      break;
    }
    delay(10);
  }
}