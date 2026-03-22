#ifndef FM_RADIO_LOGIC_H
#define FM_RADIO_LOGIC_H

#include "HelperFunctions.h"
#include <Wire.h>

// ── Pin wiring ────────────────────────────────────────────────────────────────
// RST and SEN both tied permanently to 3.3V on the breakout.
// No pin control needed — chip is always on and always in I2C mode.
#define SI4703_ADDR  0x10

// ── Si4703 register indices ───────────────────────────────────────────────────
#define DEVICEID     0x00
#define CHIPID       0x01
#define POWERCFG     0x02
#define CHANNEL      0x03
#define SYSCONFIG1   0x04
#define SYSCONFIG2   0x05
#define STATUSRSSI   0x0A
#define READCHAN     0x0B

// ── Bit positions ─────────────────────────────────────────────────────────────
#define SMUTE   15
#define DMUTE   14
#define SKMODE  10
#define SEEKUP   9
#define SEEK     8
#define TUNE    15
#define STC     14
#define SFBL    13
#define STEREO   8
#define RDS     12
#define DE      11
#define SPACE0   4

#define SEEK_UP   1
#define SEEK_DOWN 0

// ── Timeouts ──────────────────────────────────────────────────────────────────
#define FM_TUNE_TIMEOUT_MS  500
#define FM_SEEK_TIMEOUT_MS  10000

// ── State ─────────────────────────────────────────────────────────────────────
extern int  fmChannel;
extern bool fmSeekUp;
extern bool fmPowered;

// ── Public API ────────────────────────────────────────────────────────────────
void enterFMRadioMode();
void exitFMRadioMode();
void drawFMRadioScreen(bool seeking = false);
void handleFMRadioButtons();

#endif // FM_RADIO_LOGIC_H