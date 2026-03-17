#ifndef SPECTRUM_LOGIC_H
#define SPECTRUM_LOGIC_H

#include "HelperFunctions.h"
#include <arduinoFFT.h>

#define SPEC_ADC_PIN        17       // A1 (GPIO17) = ADC1_CH0
#define SPEC_SAMPLES        256      // FFT sample count - must be power of 2
#define SPEC_SAMPLE_FREQ    22050    // (not) 10kHz sample rate (covers 0–5kHz audio range)
#define SPEC_NUM_BARS       24       // number of bars across 128px display
#define SPEC_BAR_WIDTH      5        // px per bar
#define SPEC_BAR_GAP        0        // px gap between bars (fits 24 bars in 120px)
#define SPEC_MAX_HEIGHT     80       // max bar height in pixels
#define SPEC_PEAK_HOLD_MS   1000     // how long peak dot holds before falling
#define SPEC_PEAK_FALL_RATE 1        // px per update the peak falls

extern float specBarHeights[SPEC_NUM_BARS];
extern float specPeakHeights[SPEC_NUM_BARS];
extern unsigned long specPeakTimes[SPEC_NUM_BARS];

void initSpectrum();
void updateSpectrum();

#endif // SPECTRUM_LOGIC_H