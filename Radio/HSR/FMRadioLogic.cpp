#include "FMRadioLogic.h"

// ── State ─────────────────────────────────────────────────────────────────────
int  fmChannel = 876;
bool fmSeekUp  = true;
bool fmPowered = false;

static uint16_t si4703_regs[16];

// ── Low-level I2C ─────────────────────────────────────────────────────────────
// Si4703 read starts at reg 0x0A and wraps around to 0x09 (32 bytes total)
static void fmReadRegisters() {
  Wire.requestFrom(SI4703_ADDR, 32);
  for (int x = 0x0A; ; x++) {
    if (x == 0x10) x = 0;
    si4703_regs[x]  = (uint16_t)Wire.read() << 8;
    si4703_regs[x] |= Wire.read();
    if (x == 0x09) break;
  }
}

// Write registers 0x02–0x07
static void fmUpdateRegisters() {
  Wire.beginTransmission(SI4703_ADDR);
  for (int r = 0x02; r < 0x08; r++) {
    Wire.write((uint8_t)(si4703_regs[r] >> 8));
    Wire.write((uint8_t)(si4703_regs[r] & 0xFF));
  }
  Wire.endTransmission();
}

// ── Init sequence (copied from SparkFun lib, unchanged) ───────────────────────
static void fmInit() {
  // RST is permanently tied to 3.3V and SEN is permanently tied to 3.3V,
  // so the chip is always on and always in I2C mode.
  // Just configure registers directly — no RST pulse needed.

  // Enable oscillator
  fmReadRegisters();
  si4703_regs[0x07] = 0x8100;
  fmUpdateRegisters();
  delay(500);

  // Power up, unmute, configure for EU/UK
  fmReadRegisters();
  si4703_regs[POWERCFG]   = (1 << SMUTE) | (1 << DMUTE) | 0x0001;
  si4703_regs[SYSCONFIG1] |= (1 << DE);
  si4703_regs[SYSCONFIG2]  = (0b00 << 6)   // BAND: 87.5-108 MHz
                            | (0b01 << 4)   // SPACE: 100 kHz
                            | 0x0008;       // volume = 8
  fmUpdateRegisters();
  delay(110);

  fmReadRegisters();
  Serial.print("POST-INIT POWERCFG=0x");   Serial.println(si4703_regs[POWERCFG],   HEX);
  Serial.print("POST-INIT SYSCONFIG2=0x"); Serial.println(si4703_regs[SYSCONFIG2], HEX);
}

// ── Tune with timeout ─────────────────────────────────────────────────────────
// channel in library units e.g. 876 = 87.6 MHz
// Internally: channelReg = (channel - 875)  with 100 kHz spacing
static void fmTune(int channel) {
  int reg = channel - 875;
  fmReadRegisters();
  si4703_regs[CHANNEL] &= 0xFE00;
  si4703_regs[CHANNEL] |= (uint16_t)(reg & 0x03FF);
  si4703_regs[CHANNEL] |= (1 << TUNE);
  fmUpdateRegisters();

  // Wait for STC with hard timeout
  unsigned long t = millis();
  while (millis() - t < FM_TUNE_TIMEOUT_MS) {
    fmReadRegisters();
    if (si4703_regs[STATUSRSSI] & (1 << STC)) break;
    delay(20);
  }

  // Clear TUNE bit
  fmReadRegisters();
  si4703_regs[CHANNEL] &= ~(1 << TUNE);
  fmUpdateRegisters();

  // Wait for STC to clear with hard timeout
  t = millis();
  while (millis() - t < FM_TUNE_TIMEOUT_MS) {
    fmReadRegisters();
    if ((si4703_regs[STATUSRSSI] & (1 << STC)) == 0) break;
    delay(20);
  }
}

// ── Seek with timeout ─────────────────────────────────────────────────────────
// Returns found channel, or 0 on failure/timeout
static int fmSeek(bool up) {
  fmReadRegisters();
  si4703_regs[POWERCFG] |= (1 << SKMODE);   // wrap at band limits
  if (up) si4703_regs[POWERCFG] |=  (1 << SEEKUP);
  else    si4703_regs[POWERCFG] &= ~(1 << SEEKUP);
  si4703_regs[POWERCFG] |= (1 << SEEK);
  fmUpdateRegisters();

  // Wait for STC with hard timeout
  unsigned long t = millis();
  while (millis() - t < FM_SEEK_TIMEOUT_MS) {
    fmReadRegisters();
    if (si4703_regs[STATUSRSSI] & (1 << STC)) break;
    delay(30);
  }

  bool failed = (millis() - t >= FM_SEEK_TIMEOUT_MS);

  fmReadRegisters();
  bool sfbl = si4703_regs[STATUSRSSI] & (1 << SFBL);  // seek fail / band limit
  si4703_regs[POWERCFG] &= ~(1 << SEEK);
  fmUpdateRegisters();

  // Wait for STC to clear with hard timeout
  t = millis();
  while (millis() - t < FM_TUNE_TIMEOUT_MS) {
    fmReadRegisters();
    if ((si4703_regs[STATUSRSSI] & (1 << STC)) == 0) break;
    delay(20);
  }

  if (failed || sfbl) return 0;

  int found = (si4703_regs[READCHAN] & 0x03FF) + 875;
  return found;
}

static void fmSetVolume(int vol) {
  if (vol < 0)  vol = 0;
  if (vol > 15) vol = 15;
  fmReadRegisters();
  si4703_regs[SYSCONFIG2] = (si4703_regs[SYSCONFIG2] & 0xFFF0) | (uint16_t)vol;
  fmUpdateRegisters();
}

// ── Screen ────────────────────────────────────────────────────────────────────
void drawFMRadioScreen(bool seeking) {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("FM RADIO");

  const int cx = 64;
  // Up arrow
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

  // Down arrow
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

  // Always do a full re-init — the Audio library disrupts I2C while streaming,
  // so the chip loses its register state every time we leave FM mode.
  Serial.println("FM: full init");
  fmInit();
  fmPowered = true;
  fmTune(fmChannel);
  fmSetVolume(10);

  fmReadRegisters();
  Serial.print("POWERCFG=0x"); Serial.println(si4703_regs[POWERCFG], HEX);

  drawFMRadioScreen();

  while (!digitalRead(BTN_RIGHT) || !digitalRead(BTN_LEFT)) delay(10);
  delay(150);
}

// ── Exit — mute only, leave chip alive so re-entry doesn't need re-init ───────
void exitFMRadioMode() {
  if (fmPowered) {
    fmReadRegisters();
    si4703_regs[POWERCFG] &= ~((1 << SMUTE) | (1 << DMUTE));
    fmUpdateRegisters();
  }
}

// ── Button handler ────────────────────────────────────────────────────────────
void handleFMRadioButtons() {
  static bool upWas    = true;
  static bool downWas  = true;
  static bool leftWas  = true;
  static bool rightWas = true;
  static bool selWas   = true;

  bool upNow    = !digitalRead(BTN_UP);
  bool downNow  = !digitalRead(BTN_DOWN);
  bool leftNow  = !digitalRead(BTN_LEFT);
  bool rightNow = !digitalRead(BTN_RIGHT);
  bool selNow   = !digitalRead(BTN_REFRESH);

  if (upNow && !upWas) {
    fmSeekUp = true;
    fmChannel++;
    if (fmChannel > 1080) fmChannel = 875;
    fmTune(fmChannel);
    drawFMRadioScreen();
  }

  if (downNow && !downWas) {
    fmSeekUp = false;
    fmChannel--;
    if (fmChannel < 875) fmChannel = 1080;
    fmTune(fmChannel);
    drawFMRadioScreen();
  }

  if (selNow && !selWas) {
    drawFMRadioScreen(true);
    int prev  = fmChannel;
    int found = fmSeek(fmSeekUp);
    if (found == 0) {
      fmTune(prev);  // restore on failure
    } else {
      fmChannel = found;
    }
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