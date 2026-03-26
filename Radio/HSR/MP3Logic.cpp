// MP3Logic.cpp
#include "MP3Logic.h"

int mp3TrackCount    = 0;
int mp3CurrentTrack  = 1;
bool mp3Playing      = false;
int mp3ListOffset    = 0;
int mp3ListSelected  = 0;
unsigned long mp3PausedAt    = 0;
unsigned long mp3TotalPaused = 0;
unsigned long mp3TrackStart = 0;
MP3Screen mp3Screen  = MP3_LIST;
char mp3TrackNames[MP3_MAX_TRACKS][MP3_MAX_NAME_LEN];

// ── Raw UART command layer ────────────────────────────────────────────────────

void mp3SendCommand(byte command, int param) {
  byte packet[10];
  int checkSum = -(0xFF + 0x06 + command + 0x00 + highByte(param) + lowByte(param));
  packet[0] = 0x7E;
  packet[1] = 0xFF;
  packet[2] = 0x06;
  packet[3] = command;
  packet[4] = 0x00;        // no feedback - avoids blocking waits
  packet[5] = highByte(param);
  packet[6] = lowByte(param);
  packet[7] = highByte(checkSum);
  packet[8] = lowByte(checkSum);
  packet[9] = 0xEF;
  for (int i = 0; i < 10; i++) Serial1.write(packet[i]);
  delay(100); // GT3200B needs this between commands
}

void mp3Play(int track)    { mp3SendCommand(0x03, track); }
void mp3Pause()            { mp3SendCommand(0x0E, 0); }
void mp3Resume()           { mp3SendCommand(0x0D, 0); }
void mp3Stop()             { mp3SendCommand(0x16, 0); }
void mp3SetVolume(int vol) { mp3SendCommand(0x06, vol); }
void mp3Next()             { mp3SendCommand(0x01, 0); }
void mp3Prev()             { mp3SendCommand(0x02, 0); }

int mp3GetTrackCount() {
  byte packet[10];
  int checkSum = -(0xFF + 0x06 + 0x48 + 0x01 + 0x00 + 0x00);
  packet[0] = 0x7E;
  packet[1] = 0xFF;
  packet[2] = 0x06;
  packet[3] = 0x48;
  packet[4] = 0x01; // request feedback for this query
  packet[5] = 0x00;
  packet[6] = 0x00;
  packet[7] = highByte(checkSum);
  packet[8] = lowByte(checkSum);
  packet[9] = 0xEF;
  for (int i = 0; i < 10; i++) Serial1.write(packet[i]);

  // Wait for response with timeout
  unsigned long start = millis();
  while (millis() - start < 500) {
    if (Serial1.available() >= 10) {
      byte response[10];
      for (int i = 0; i < 10; i++) response[i] = Serial1.read();
      if (response[0] == 0x7E && response[9] == 0xEF) {
        return (response[5] << 8) | response[6];
      }
    }
  }
  return 0; // timeout - no card or no tracks
}

// ── Init ──────────────────────────────────────────────────────────────────────

void initMP3Player() {
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(2000); // GT3200B needs a longer boot delay

  mp3SetVolume(15);
  delay(100);

  mp3TrackCount = mp3GetTrackCount();
  if (mp3TrackCount < 0) mp3TrackCount = 0;

  for (int i = 0; i < mp3TrackCount && i < MP3_MAX_TRACKS; i++) {
    snprintf(mp3TrackNames[i], MP3_MAX_NAME_LEN, "Track %02d", i + 1);
  }

  DEBUG_PRINTF("MP3 ready. Tracks: %d\n", mp3TrackCount);
}

// ── Entry point ───────────────────────────────────────────────────────────────

void enterMP3Mode() {
  audio.stopSong();
  currentMode     = MODE_MP3;
  currentDisplay  = DISPLAY_MP3;
  mp3Screen       = MP3_LIST;
  mp3ListOffset   = 0;
  mp3ListSelected = 0;
  mp3Playing      = false;

  // Only rescan if we didn't get tracks at boot
  if (mp3TrackCount == 0) {
    delay(500);
    mp3TrackCount = mp3GetTrackCount();
    if (mp3TrackCount < 0) mp3TrackCount = 0;
    for (int i = 0; i < mp3TrackCount && i < MP3_MAX_TRACKS; i++) {
      snprintf(mp3TrackNames[i], MP3_MAX_NAME_LEN, "Track %02d", i + 1);
    }
  }

  mp3Stop();
  drawMP3ListScreen();

  while (buttons[BTN_IDX_LEFT].held || buttons[BTN_IDX_RIGHT].held) {
      updateButtons();
      delay(10);
  }
  delay(200);
}

// ── File list screen ──────────────────────────────────────────────────────────

void drawMP3ListScreen() {
  display.clearDisplay();
  display.setTextSize(1);

  // Heading
  display.setCursor(0, 0);
  display.print("MP3 PLAYER");
  display.setCursor(90, 0);
  display.print(mp3TrackCount);
  display.print(" trk");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);

  if (mp3TrackCount == 0) {
    display.setCursor(0, 40);
    display.println("No SD card or");
    display.println("no tracks found.");
    display.display();
    return;
  }

  const int listY      = 14;
  const int rowHeight  = 11;
  const int maxVisible = 8;

  // Keep selected item in view
  if (mp3ListSelected < mp3ListOffset)
    mp3ListOffset = mp3ListSelected;
  if (mp3ListSelected >= mp3ListOffset + maxVisible)
    mp3ListOffset = mp3ListSelected - maxVisible + 1;

  for (int i = 0; i < maxVisible; i++) {
    int idx  = mp3ListOffset + i;
    if (idx >= mp3TrackCount) break;
    int y    = listY + i * rowHeight;
    bool sel = (idx == mp3ListSelected);

    if (sel) {
      display.fillRect(0, y - 1, 128, rowHeight, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    }
    display.setCursor(2, y);
    display.print(mp3TrackNames[idx]);
    display.setTextColor(SH110X_WHITE);
  }

  // Scrollbar
  if (mp3TrackCount > maxVisible) {
    int barH = (maxVisible * 88) / mp3TrackCount;
    int barY = listY + (mp3ListOffset * 88) / mp3TrackCount;
    display.drawRect(125, listY, 3, 88, SH110X_WHITE);
    display.fillRect(125, barY, 3, barH, SH110X_WHITE);
  }

  drawBatteryIcon(0, display.height() - 15);
  display.display();
}

// ── Now playing screen ────────────────────────────────────────────────────────

void drawMP3PlayScreen() {
  display.clearDisplay();
  display.setTextSize(1);

  // Heading
  display.setCursor(0, 0);
  display.print("MP3 PLAYER");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);

  // Track name
  display.setCursor(0, 14);
  display.println(mp3TrackNames[mp3CurrentTrack - 1]);

  // Track number
  display.setCursor(0, 26);
  display.print("Track ");
  display.print(mp3CurrentTrack);
  display.print(" / ");
  display.println(mp3TrackCount);

  // Play/pause indicator
  display.setTextSize(2);
  display.setCursor(54, 38);
  display.print(mp3Playing ? "||" : " >");
  display.setTextSize(1);

  // Elapsed time using millis() - no blocking UART reads
  unsigned long elapsed = ((mp3Playing ? millis() : mp3PausedAt) - mp3TrackStart - mp3TotalPaused) / 1000;
  display.setCursor(0, 62);
  display.print(elapsed / 60);
  display.print(":");
  if (elapsed % 60 < 10) display.print("0");
  display.print(elapsed % 60);
  display.print(" elapsed");

  // Hints
  display.setCursor(0, 80);
  display.println("REF: pause/play");
  display.setCursor(0, 90);
  display.println("L/R: prev/next");
  display.setCursor(0, 100);
  display.println("Hold REF: back");

  drawBatteryIcon(0, display.height() - 15);
  display.display();
}

void mp3CheckFinished() {
  if (Serial1.available() >= 10) {
    byte response[10];
    // Drain bytes until we find a valid packet start
    if (Serial1.peek() != 0x7E) {
      Serial1.read();
      return;
    }
    for (int i = 0; i < 10; i++) response[i] = Serial1.read();
    if (response[0] == 0x7E && response[9] == 0xEF && response[3] == 0x3D) {
      // Track finished!
      mp3Playing = false;
    }
  }
}

// ── Button handler ────────────────────────────────────────────────────────────

void handleMP3Buttons() {
  static unsigned long lastRedraw    = 0;

  // ── LIST screen ────────────────────────────────────────────────────────────
  if (mp3Screen == MP3_LIST) {

    if (buttons[BTN_IDX_UP].pressed) {
      mp3ListSelected--;
      if (mp3ListSelected < 0) mp3ListSelected = mp3TrackCount - 1;
      drawMP3ListScreen();
    }

    if (buttons[BTN_IDX_DOWN].pressed) {
      mp3ListSelected++;
      if (mp3ListSelected >= mp3TrackCount) mp3ListSelected = 0;
      drawMP3ListScreen();
    }

    // Select track → play
    if (buttons[BTN_IDX_REFRESH].pressed) {
      mp3CurrentTrack = mp3ListSelected + 1;
      mp3SetVolume(15);
      mp3Play(mp3CurrentTrack);
      mp3Playing    = true;
      mp3TrackStart = millis();
      mp3PausedAt    = 0;
      mp3TotalPaused = 0;
      mp3Screen     = MP3_PLAYING;
      drawMP3PlayScreen();
    }

    // LEFT → FM Radio
    if (buttons[BTN_IDX_LEFT].pressed) {
      mp3Stop();
      mp3Playing = false;
      //enterFMRadioMode();

      exitFMRadioMode();
      currentStation = stationCount - 1;
      currentMode    = MODE_RADIO;
      currentDisplay = DISPLAY_STATION;
      audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
      audio.setVolume(12);
      audio.connecttohost(stations[currentStation]);
      drawRadioScreen();
      return;
    }

    // RIGHT → Info Terminal
    if (buttons[BTN_IDX_RIGHT].pressed) {
      mp3Stop();
      mp3Playing     = false;
      currentMode    = MODE_INFO_TERMINAL;
      currentDisplay = DISPLAY_INFO_KEYBOARD;
      enterInfoTerminal();
      return;
    }
  }

  // ── PLAYING screen ─────────────────────────────────────────────────────────
  else if (mp3Screen == MP3_PLAYING) {

    // Hold REFRESH → back to list
    if (buttons[BTN_IDX_REFRESH].held && millis() - buttons[BTN_IDX_REFRESH].pressTime > 1500) {
        mp3Screen  = MP3_LIST;
        mp3Playing = false;
        mp3Stop();
        drawMP3ListScreen();
        return;
    }

    // Tap REFRESH → toggle pause/play on press
    if (buttons[BTN_IDX_REFRESH].pressed) {
      if (mp3Playing) {
        mp3Pause();
        mp3Playing  = false;
        mp3PausedAt = millis();
      } else {
        mp3Resume();
        mp3TotalPaused += millis() - mp3PausedAt;
        mp3Playing      = true;
      }
      drawMP3PlayScreen();
    }

    // LEFT → previous track
    if (buttons[BTN_IDX_LEFT].pressed) {
      mp3CurrentTrack--;
      if (mp3CurrentTrack < 1) mp3CurrentTrack = mp3TrackCount;
      mp3ListSelected = mp3CurrentTrack - 1;
      mp3Play(mp3CurrentTrack);
      mp3Playing    = true;
      mp3TrackStart = millis();
      mp3PausedAt    = 0;
      mp3TotalPaused = 0;
      drawMP3PlayScreen();
    }

    // RIGHT → next track
    if (buttons[BTN_IDX_RIGHT].pressed) {
      mp3CurrentTrack++;
      if (mp3CurrentTrack > mp3TrackCount) mp3CurrentTrack = 1;
      mp3ListSelected = mp3CurrentTrack - 1;
      mp3Play(mp3CurrentTrack);
      mp3Playing    = true;
      mp3TrackStart = millis();
      mp3PausedAt    = 0;
      mp3TotalPaused = 0;
      drawMP3PlayScreen();
    }

    // Periodic redraw for elapsed time - no UART reads
    if (millis() - lastRedraw > 500) {
      drawMP3PlayScreen();
      lastRedraw = millis();
    }
  }
}