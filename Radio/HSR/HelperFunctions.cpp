#include "HelperFunctions.h"

Adafruit_MAX17048 max17048;
float batteryVoltage = 0;
float batteryPercent = 0;
unsigned long lastBatteryRead = 0;
DeviceMode currentMode = MODE_SETUP;
bool offlineMode = false;
bool refreshHeld = false;
unsigned long refreshPressTime = 0;

// ================= BUTTON STATE ======================

ButtonState buttons[NUM_BUTTONS] = {
  {BTN_UP,      HIGH, false, false, false, 0, 0},
  {BTN_DOWN,    HIGH, false, false, false, 0, 0},
  {BTN_LEFT,    HIGH, false, false, false, 0, 0},
  {BTN_RIGHT,   HIGH, false, false, false, 0, 0},
  {BTN_REFRESH, HIGH, false, false, false, 0, 0},
};

// Call once at the top of loop(). Reads all GPIO pins once and populates
// pressed/held/released flags. Everything else reads from buttons[].
void updateButtons() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_BUTTONS; i++) {
    ButtonState &b = buttons[i];
    bool raw = digitalRead(b.pin);

    // Clear single-frame edge flags
    b.pressed  = false;
    b.released = false;

    if (raw != b.lastRaw && (now - b.lastDebounceTime > newDebounceTime)) {
      b.lastDebounceTime = now;
      b.lastRaw = raw;

      if (raw == LOW) {       // active-low: button just pressed
        b.pressed  = true;
        b.held     = true;
        b.pressTime = now;
      } else {                // button just released
        b.released = true;
        b.held     = false;
      }
    }
  }
}

// ================= BATTERY ===========================

void initBattery() {
  DEBUG_PRINTLN("Initializing MAX17048 battery monitor...");
  if (!max17048.begin()) {
    DEBUG_PRINTLN("Could not find MAX17048 battery monitor!");
  } else {
    DEBUG_PRINTLN("MAX17048 found!");
  }
}

void updateBattery() {
  if (millis() - lastBatteryRead > 1000) {
    batteryPercent = max17048.cellPercent();
    batteryVoltage = max17048.cellVoltage();
    lastBatteryRead = millis();
    DEBUG_PRINTF("Battery: %.2fV %.1f%%\n", batteryVoltage, batteryPercent);
  }
}

// ================= BUTTON HANDLING ===================

void handleButtons() {

  if (currentMode == MODE_RADIO) {

    // UP cycles screens
    if (buttons[BTN_IDX_UP].pressed) {
      if (currentDisplay == DISPLAY_STATION || (currentDisplay == DISPLAY_MP3 && mp3Screen == MP3_PLAYING)) {
        previousDisplay = currentDisplay;
        currentDisplay = DISPLAY_SPECTRUM;
        specBinOffset = 0;
      } else if (currentDisplay == DISPLAY_SPECTRUM && previousDisplay == DISPLAY_STATION) {
        currentDisplay = DISPLAY_WIFI_INFO;
        drawWifiInfoScreen();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        currentDisplay = DISPLAY_STATION;
        drawRadioScreen();
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        currentDisplay = DISPLAY_STATION;
        drawRadioScreen();
      }
    }

    // DOWN cycles screens (other direction)
    if (buttons[BTN_IDX_DOWN].pressed) {
      if (currentDisplay == DISPLAY_STATION) {
        currentDisplay = DISPLAY_WIFI_INFO;
        drawWifiInfoScreen();
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        currentDisplay = DISPLAY_WIFI_INFO;
        drawWifiInfoScreen();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        currentDisplay = DISPLAY_STATION;
        drawRadioScreen();
      } else if (currentDisplay == DISPLAY_SPECTRUM) {
        currentDisplay = previousDisplay;
        if (previousDisplay == DISPLAY_STATION) {
          drawRadioScreen();
        }
      }
    }

    // LEFT — previous station or speaker toggle
    if (buttons[BTN_IDX_LEFT].pressed) {
      if (currentDisplay == DISPLAY_STATION) {
        switchStation(-1);
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        speakerEnabled = false;
        lastMaxVolume = maxVolumeSpeakerOff;
        drawSpeakerControlScreen();
      }
    }

    // RIGHT — next station or speaker toggle
    if (buttons[BTN_IDX_RIGHT].pressed) {
      if (currentDisplay == DISPLAY_STATION) {
        switchStation(1);
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        speakerEnabled = true;
        if (audio.getVolume() > maxVolumeSpeakerOn) {
          audio.setVolume(maxVolumeSpeakerOn);
        }
        lastMaxVolume = maxVolumeSpeakerOn;
        drawSpeakerControlScreen();
      }
    }

    // REFRESH — force redraw; hold 3 s on WiFi info to enter network management
    if (buttons[BTN_IDX_REFRESH].held) {
      if (currentDisplay == DISPLAY_STATION) {
        if (buttons[BTN_IDX_REFRESH].pressed) drawRadioScreen();
      } else if (currentDisplay == DISPLAY_WIFI_INFO) {
        if (buttons[BTN_IDX_REFRESH].pressed) {
          drawWifiInfoScreen();
          refreshHeld = true;
          refreshPressTime = buttons[BTN_IDX_REFRESH].pressTime;
        }
        if (refreshHeld && millis() - refreshPressTime > 3000) {
          currentMode = MODE_MANAGE_NETWORKS;
          scrollOffset = 0;
          refreshHeld = false;
        }
      } else if (currentDisplay == DISPLAY_SPEAKER_CTRL) {
        if (buttons[BTN_IDX_REFRESH].pressed) drawSpeakerControlScreen();
      }
    } else {
      if (buttons[BTN_IDX_REFRESH].released) refreshHeld = false;
    }
  }

  if (currentMode == MODE_MANAGE_NETWORKS) {
    drawNetworkListScreen();

    if (buttons[BTN_IDX_UP].pressed && selectedNetworkIndex > 0) {
      selectedNetworkIndex--;
      scrollOffset = 0;
    }

    if (buttons[BTN_IDX_DOWN].pressed && selectedNetworkIndex < savedCount - 1) {
      selectedNetworkIndex++;
      scrollOffset = 0;
    }

    if (buttons[BTN_IDX_LEFT].pressed && savedCount > 0) {
      deleteNetwork(selectedNetworkIndex);
    }

    if (buttons[BTN_IDX_RIGHT].pressed) {
      clearAllNetworks();
    }

    // Hold REFRESH 5 s to restart
    if (buttons[BTN_IDX_REFRESH].held) {
      if (buttons[BTN_IDX_REFRESH].pressed) {
        refreshHeld = true;
        refreshPressTime = buttons[BTN_IDX_REFRESH].pressTime;
      }
      if (refreshHeld && millis() - refreshPressTime > 5000) {
        ESP.restart();
      }
    } else {
      if (buttons[BTN_IDX_REFRESH].released) refreshHeld = false;
    }
  }

  if (currentMode == MODE_SETUP) {
    // Hold REFRESH 3 s to enter network management
    if (buttons[BTN_IDX_REFRESH].held) {
      if (buttons[BTN_IDX_REFRESH].pressed) {
        refreshHeld = true;
        refreshPressTime = buttons[BTN_IDX_REFRESH].pressTime;
      }
      if (refreshHeld && millis() - refreshPressTime > 3000) {
        currentMode = MODE_MANAGE_NETWORKS;
        scrollOffset = 0;
        refreshHeld = false;
      }
    } else {
      if (buttons[BTN_IDX_REFRESH].released) refreshHeld = false;
    }

    if (buttons[BTN_IDX_UP].pressed && scrollOffset > 0) {
      scrollOffset--;
      drawSetupScreen();
    }

    if (buttons[BTN_IDX_DOWN].pressed && scrollOffset < totalLines - maxVisibleLines) {
      scrollOffset++;
      drawSetupScreen();
    }
  }
}