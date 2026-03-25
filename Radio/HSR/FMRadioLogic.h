#ifndef FM_RADIO_LOGIC_H
#define FM_RADIO_LOGIC_H

#include "HelperFunctions.h"
#include <Wire.h>
#include <SparkFunSi4703.h>

// ── Pin wiring ────────────────────────────────────────────────────────────────
// RST → GPIO16 (freed by removing software speaker MOSFET control)
// SEN tied permanently to 3.3V — chip always in I2C mode
#define resetPin 16
#define SDIO 3
#define SCLK 4
#define STC -1

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