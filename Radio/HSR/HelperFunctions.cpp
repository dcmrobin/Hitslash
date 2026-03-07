#include "HelperFunctions.h"

Adafruit_MAX17048 max17048;
float batteryVoltage = 0;
float batteryPercent = 0;
unsigned long lastBatteryRead = 0;
DeviceMode currentMode = MODE_SETUP;
bool refreshHeld = false;
unsigned long refreshPressTime = 0;

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
        digitalWrite(SPEAKER_MOSFET_PIN, HIGH);
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
        digitalWrite(SPEAKER_MOSFET_PIN, LOW);
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
}