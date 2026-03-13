#include "MP3Logic.h"

DFRobotDFPlayerMini dfPlayer;
int mp3TrackCount   = 0;
int mp3CurrentTrack = 1; // DFPlayer tracks are 1-indexed
bool mp3Playing     = false;
int mp3ListOffset   = 0;
int mp3ListSelected = 0;
unsigned long mp3TrackStart  = 0;
unsigned long mp3TrackLength = 0;
MP3Screen mp3Screen = MP3_LIST;

// We can't read filenames from DFPlayer directly,
// so we just label them by track number
char mp3TrackNames[MP3_MAX_TRACKS][MP3_MAX_NAME_LEN];

void initMP3Player() {
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(1000);

  if (!dfPlayer.begin(Serial1)) {
    DEBUG_PRINTLN("DFPlayer init failed - no card?");
    mp3TrackCount = 0;
    return;
  }

  dfPlayer.volume(15);
  
  mp3TrackCount = dfPlayer.readFileCounts();
  if (mp3TrackCount < 0) mp3TrackCount = 0;

  for (int i = 0; i < mp3TrackCount && i < MP3_MAX_TRACKS; i++) {
    snprintf(mp3TrackNames[i], MP3_MAX_NAME_LEN, "Track %02d", i + 1);
  }
}

void enterMP3Mode() {
  audio.stopSong();
  currentMode    = MODE_MP3;
  currentDisplay = DISPLAY_MP3;
  mp3Screen      = MP3_LIST;
  mp3ListOffset  = 0;
  mp3ListSelected = 0;
  mp3Playing     = false;
  dfPlayer.stop();
  drawMP3ListScreen();
  
  // Wait for button release before handing control to MP3 button handler
  while (!digitalRead(BTN_LEFT) || !digitalRead(BTN_RIGHT)) delay(10);
  delay(200); // extra debounce
}

// ── File list screen ──────────────────────────────────────────────────────────
void drawMP3ListScreen() {
  display.clearDisplay();
  display.setTextSize(1);

  // Heading
  display.setCursor(0, 0);
  display.println("MP3 PLAYER");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);

  // Track count
  display.setCursor(90, 0);
  display.print(mp3TrackCount);
  display.print(" trk");

  // Visible tracks (leave room for heading and battery)
  const int listY      = 14;
  const int rowHeight  = 11;
  const int maxVisible = 8;

  // Track time
  //int elapsed = dfPlayer.readCurrentFileNumber() > 0 ? dfPlayer.readCurrentFileNumber() : 0;
  //int total   = dfPlayer.readFileTotalDuration();
  //int current = dfPlayer.readFileCurrentDuration();
  //if (total < 0 || total > 3600) total = 0;
  //if (current < 0 || current > total) current = 0;

  //display.setCursor(0, 70);
  //display.print(current / 60);
  //display.print(":");
  //if (current % 60 < 10) display.print("0");
  //display.print(current % 60);
  //display.print(" / ");
  //display.print(total / 60);
  //display.print(":");
  //if (total % 60 < 10) display.print("0");
  //display.println(total % 60);

  // Keep selected item in view
  if (mp3ListSelected < mp3ListOffset)
    mp3ListOffset = mp3ListSelected;
  if (mp3ListSelected >= mp3ListOffset + maxVisible)
    mp3ListOffset = mp3ListSelected - maxVisible + 1;

  for (int i = 0; i < maxVisible; i++) {
    int idx = mp3ListOffset + i;
    if (idx >= mp3TrackCount) break;
    int y   = listY + i * rowHeight;
    bool sel = (idx == mp3ListSelected);

    if (sel) {
      display.fillRect(0, y - 1, 128, rowHeight, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    }
    display.setCursor(2, y);
    display.print(mp3TrackNames[idx]);
    display.setTextColor(SH110X_WHITE);
  }

  // Scrollbar hint if needed
  if (mp3TrackCount > maxVisible) {
    int barH   = (maxVisible * 128) / mp3TrackCount;
    int barY   = listY + (mp3ListOffset * 88) / mp3TrackCount;
    display.drawRect(125, listY, 3, 88, SH110X_WHITE);
    display.fillRect(125, barY, 3, barH, SH110X_WHITE);
  }

  if (mp3TrackCount == 0) {
    display.setCursor(0, 40);
    display.println("No SD card or");
    display.println("no tracks found.");
    display.display();
    return;
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
  display.println("MP3 PLAYER");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);

  // Track name (large-ish)
  display.setTextSize(1);
  display.setCursor(0, 14);
  display.println(mp3TrackNames[mp3CurrentTrack - 1]);

  // Track number
  display.setCursor(0, 28);
  display.print("Track ");
  display.print(mp3CurrentTrack);
  display.print(" / ");
  display.println(mp3TrackCount);

  // Play/pause indicator
  display.setTextSize(2);
  display.setCursor(54, 40);
  if (mp3Playing) {
    display.print("||"); // pause symbol
  } else {
    display.print(">"); // play symbol
  }
  display.setTextSize(1);

  // Progress bar (time-based estimate)
  unsigned long elapsed = mp3Playing ? (millis() - mp3TrackStart) : 0;
  float percent = 0;
  if (mp3TrackLength > 0) {
    percent = constrain((float)elapsed / mp3TrackLength, 0.0, 1.0);
  }
  drawProgressBar(0, 70, 128, 8, percent);

  // Hints
  display.setCursor(0, 82);
  display.println("REF: pause/play");
  display.setCursor(0, 92);
  display.println("L/R: prev/next");
  display.setCursor(0, 102);
  display.println("Hold REF: back");

  drawBatteryIcon(0, display.height() - 15);
  display.display();
}

// ── Button handler ────────────────────────────────────────────────────────────
void handleMP3Buttons() {
  static bool upWas      = false;
  static bool downWas    = false;
  static bool leftWas    = false;
  static bool rightWas   = false;
  static bool selWas     = false;
  static unsigned long selPressStart = 0;
  static unsigned long lastRepeat    = 0;
  const unsigned long repeatDelay    = 130;

  bool upNow    = !digitalRead(BTN_UP);
  bool downNow  = !digitalRead(BTN_DOWN);
  bool leftNow  = !digitalRead(BTN_LEFT);
  bool rightNow = !digitalRead(BTN_RIGHT);
  bool selNow   = !digitalRead(BTN_REFRESH);

  // ── LIST screen ────────────────────────────────────────────────────────────
  if (mp3Screen == MP3_LIST) {

    // Scroll up
    if (upNow) {
      if (!upWas || millis() - lastRepeat > repeatDelay) {
        mp3ListSelected--;
        if (mp3ListSelected < 0) mp3ListSelected = mp3TrackCount - 1;
        drawMP3ListScreen();
        lastRepeat = millis();
      }
    }

    // Scroll down
    if (downNow) {
      if (!downWas || millis() - lastRepeat > repeatDelay) {
        mp3ListSelected++;
        if (mp3ListSelected >= mp3TrackCount) mp3ListSelected = 0;
        drawMP3ListScreen();
        lastRepeat = millis();
      }
    }

    // Select track → play
    if (selNow && !selWas) {
      mp3CurrentTrack = mp3ListSelected + 1; // 1-indexed
      dfPlayer.play(mp3CurrentTrack);
      mp3Playing     = true;
      mp3TrackStart  = millis();
      mp3TrackLength = 0; // unknown until DFPlayer reports it
      mp3Screen      = MP3_PLAYING;
      drawMP3PlayScreen();
    }

    // LEFT → go back to last radio station
    if (leftNow && !leftWas) {
      dfPlayer.stop();
      mp3Playing = false;
      currentMode    = MODE_RADIO;
      currentDisplay = DISPLAY_STATION;
      audio.connecttohost(stations[currentStation]);
      drawRadioScreen();
    }

    // RIGHT → Info Terminal
    if (rightNow && !rightWas) {
      dfPlayer.stop();
      mp3Playing = false;
      currentMode    = MODE_INFO_TERMINAL;
      currentDisplay = DISPLAY_INFO_KEYBOARD;
      enterInfoTerminal();
    }
  }

  // ── PLAYING screen ─────────────────────────────────────────────────────────
  else if (mp3Screen == MP3_PLAYING) {

    // Hold REFRESH → back to list
    if (selNow) {
      if (!selWas) selPressStart = millis();
      if (millis() - selPressStart > 1500) {
        mp3Screen = MP3_LIST;
        drawMP3ListScreen();
        // Wait for release
        while (!digitalRead(BTN_REFRESH)) delay(10);
        selWas = false;
        return;
      }
    }

    // Tap REFRESH → toggle pause/play
    if (!selNow && selWas) {
      // Only trigger on release, and only if it wasn't a long press
      if (millis() - selPressStart < 1500) {
        if (mp3Playing) {
          dfPlayer.pause();
          mp3Playing = false;
        } else {
          dfPlayer.start();
          mp3Playing = true;
          mp3TrackStart = millis(); // reset for progress estimate
        }
        drawMP3PlayScreen();
      }
    }

    // LEFT → previous track
    if (leftNow && !leftWas) {
      mp3CurrentTrack--;
      if (mp3CurrentTrack < 1) mp3CurrentTrack = mp3TrackCount;
      mp3ListSelected = mp3CurrentTrack - 1;
      dfPlayer.play(mp3CurrentTrack);
      mp3Playing    = true;
      mp3TrackStart = millis();
      drawMP3PlayScreen();
    }

    // RIGHT → next track
    if (rightNow && !rightWas) {
      mp3CurrentTrack++;
      if (mp3CurrentTrack > mp3TrackCount) mp3CurrentTrack = 1;
      mp3ListSelected = mp3CurrentTrack - 1;
      dfPlayer.play(mp3CurrentTrack);
      mp3Playing    = true;
      mp3TrackStart = millis();
      drawMP3PlayScreen();
    }

    // Periodically redraw to update progress bar
    static unsigned long lastRedraw = 0;
    if (mp3Playing && millis() - lastRedraw > 500) {
      drawMP3PlayScreen();
      lastRedraw = millis();
    }
  }

  upWas    = upNow;
  downWas  = downNow;
  leftWas  = leftNow;
  rightWas = rightNow;
  selWas   = selNow;
}