#include "controls_math.h"
#include <math.h>
#include <stdlib.h>

float smoothValue(int raw, float *filtered, int *lastRaw, int hysteresisCounts, float emaAlpha) {
  if (abs(raw - *lastRaw) >= hysteresisCounts) {
    *filtered = (*filtered) * (1.0f - emaAlpha) + raw * emaAlpha;
    *lastRaw = raw;
  }
  return *filtered;
}

float mapPitchHz(float pitchNorm, float freqMinHz, float freqMaxHz) {
  return freqMinHz * powf(freqMaxHz / freqMinHz, pitchNorm);
}
