#include "SpectrumLogic.h"
#include "esp_timer.h"

float specBarHeights[SPEC_NUM_BARS] = {0};
float specPeakHeights[SPEC_NUM_BARS] = {0};
unsigned long specPeakTimes[SPEC_NUM_BARS] = {0};

static double vReal[SPEC_SAMPLES];
static double vImag[SPEC_SAMPLES];

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SPEC_SAMPLES, SPEC_SAMPLE_FREQ);

// Maps FFT bin index to one of SPEC_NUM_BARS bars using logarithmic scaling
// so bass frequencies (left) get more resolution, treble (right) gets less
static int binToBar(int bin) {
  int minBin = 1;// skip DC bin
  int maxBin = 12;
  if (bin < minBin || bin > maxBin) return -1;  // outside our window, skip
  int bar = (int)((float)(bin - minBin) / (maxBin - minBin) * SPEC_NUM_BARS);
  if (bar < 0) bar = 0;
  if (bar >= SPEC_NUM_BARS) bar = SPEC_NUM_BARS - 1;
  return bar;
}

void initSpectrum() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // allows 0–3.3V input range
  for (int i = 0; i < SPEC_NUM_BARS; i++) {
    specBarHeights[i]  = 0;
    specPeakHeights[i] = 0;
    specPeakTimes[i]   = 0;
  }
}

void updateSpectrum() {
  // Timer-accurate sampling using esp_timer
  int64_t interval = 1000000LL / SPEC_SAMPLE_FREQ;
  int64_t next = esp_timer_get_time();
  
  for (int i = 0; i < SPEC_SAMPLES; i++) {
    vReal[i] = (double)analogRead(SPEC_ADC_PIN) - 2048.0;
    vImag[i] = 0.0;
    next += interval;
    while (esp_timer_get_time() < next);
  }

  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
  vReal[0] = 0;  // kill DC
  vReal[1] = 0;  // kill near-DC

  // Accumulate FFT bins into bars
  float barMag[SPEC_NUM_BARS] = {0};
  int   barCount[SPEC_NUM_BARS] = {0};

  for (int i = 1; i < SPEC_SAMPLES / 2; i++) {
    int bar = binToBar(i);
    if (bar < 0) continue;  // add this line
    barMag[bar]   += (float)vReal[i];
    barCount[bar] += 1;
  }

  unsigned long now = millis();

  for (int i = 0; i < SPEC_NUM_BARS; i++) {
    float mag = (barCount[i] > 0) ? barMag[i] / barCount[i] : 0;

    // Scale to bar height - tune SPEC_SCALE if bars feel too tall/short
    const float SPEC_SCALE = 0.04f;
    float h = mag * SPEC_SCALE;
    if (h > SPEC_MAX_HEIGHT) h = SPEC_MAX_HEIGHT;
    if (h < 0) h = 0;

    // Smooth decay: bars fall faster than they rise
    /*if (h > specBarHeights[i]) {
      specBarHeights[i] = h; // instant attack
    } else {
      specBarHeights[i] *= 0.75f; // decay
    }*/
    specBarHeights[i] = h;

    // Peak hold logic
    if (specBarHeights[i] >= specPeakHeights[i]) {
      specPeakHeights[i] = specBarHeights[i];
      specPeakTimes[i]   = now;
    } else if (now - specPeakTimes[i] > SPEC_PEAK_HOLD_MS) {
      specPeakHeights[i] -= SPEC_PEAK_FALL_RATE;
      if (specPeakHeights[i] < 0) specPeakHeights[i] = 0;
    }
  }
}