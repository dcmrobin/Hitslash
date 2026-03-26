#include "FMRadioLogic.h"

// ── State ─────────────────────────────────────────────────────────────────────
int  fmChannel = 876;
bool fmSeekUp  = true;
bool fmPowered = false;
Si4703_Breakout radio(resetPin, SDIO, SCLK, STC);

// ── Init ──────────────────────────────────────────────────────────────────────
static void fmInit() {
    if (!fmPowered) {
        radio.powerOn();
    }
    radio.setVolume(15);
}

// ── Screen ────────────────────────────────────────────────────────────────────
void drawFMRadioScreen(bool seeking) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("FM RADIO");

  const int cx = 64;
  display.drawLine(cx,     18, cx - 7, 28, SH110X_WHITE);
  display.drawLine(cx,     18, cx + 7, 28, SH110X_WHITE);
  display.drawLine(cx - 7, 28, cx + 7, 28, SH110X_WHITE);

  display.setTextSize(2);
  if (seeking) {
    display.setCursor(4, 40);
    display.print("SEEKING...");
    display.setTextSize(1);
    display.setCursor(30, 62);
    display.print(fmSeekUp ? "direction: UP" : "direction: DN");
  } else {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d.%d MHz", fmChannel / 10, fmChannel % 10);
    int startX = (128 - (int)strlen(buf) * 12) / 2;
    if (startX < 0) startX = 0;
    display.setCursor(startX, 40);
    display.print(buf);
    display.setTextSize(1);
  }

  display.drawLine(cx - 7, 66, cx + 7, 66, SH110X_WHITE);
  display.drawLine(cx - 7, 66, cx,     76, SH110X_WHITE);
  display.drawLine(cx + 7, 66, cx,     76, SH110X_WHITE);

  display.setCursor(0, 84);
  display.print("U/D: step freq");
  display.setCursor(0, 95);
  display.print("REF: seek ");
  display.print(fmSeekUp ? "UP" : "DOWN");
  display.setCursor(0, 106);
  display.print("L: last stn  R: MP3");

  drawBatteryIcon(0, display.height() - 15);
  display.display();
}

// ── Entry ─────────────────────────────────────────────────────────────────────
void enterFMRadioMode() {
  audio.stopSong();
  delay(100);

  currentMode    = MODE_FM_RADIO;
  currentDisplay = DISPLAY_FM_RADIO;
  fmSeekUp       = true;

  fmInit();
  fmPowered = true;
  //fmChannel = radio.getChannel();

  drawFMRadioScreen();
}

// ── Exit ──────────────────────────────────────────────────────────────────────
void exitFMRadioMode() {
  if (fmPowered) {
    radio.setVolume(0);
  }
}

// ── Button handler ────────────────────────────────────────────────────────────
void handleFMRadioButtons() {
  static bool upWas    = true;
  static bool downWas  = true;
  static bool leftWas  = true;
  static bool rightWas = true;
  static bool selWas   = true;
// uprade these to use the new button stuffs
  bool upNow    = !digitalRead(BTN_UP);
  bool downNow  = !digitalRead(BTN_DOWN);
  bool leftNow  = !digitalRead(BTN_LEFT);
  bool rightNow = !digitalRead(BTN_RIGHT);
  bool selNow   = !digitalRead(BTN_REFRESH);

  if (upNow && !upWas) {
    fmSeekUp = true;
    fmChannel++;
    if (fmChannel > 1080) fmChannel = 875;
    radio.seekUp();
    drawFMRadioScreen();
  }

  if (downNow && !downWas) {
    fmSeekUp = false;
    fmChannel--;
    if (fmChannel < 875) fmChannel = 1080;
    radio.seekDown();
    drawFMRadioScreen();
  }

  if (selNow && !selWas) {
    drawFMRadioScreen(true);
    // seek until a station is found or timeout occurs
    drawFMRadioScreen();
  }

  if (leftNow && !leftWas) {
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

  if (rightNow && !rightWas) {
    exitFMRadioMode();
    enterMP3Mode();
    return;
  }

  upWas    = upNow;
  downWas  = downNow;
  leftWas  = leftNow;
  rightWas = rightNow;
  selWas   = selNow;
}