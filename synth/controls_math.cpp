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

int8_t quadratureStep(uint8_t prevState, uint8_t newState) {
  // Indexed by (prevState << 2) | newState. Valid single-bit transitions
  // around the Gray code cycle 00 -> 01 -> 11 -> 10 -> 00 score +1 in that
  // direction, -1 in reverse; a repeated state or a two-bit jump (only
  // possible via contact bounce, since a real detent never skips a state)
  // scores 0.
  static const int8_t kStepTable[16] = {
      0, 1, -1, 0,  // prev 00: ->00, ->01, ->10, ->11
      -1, 0, 0, 1,  // prev 01: ->00, ->01, ->10, ->11
      1, 0, 0, -1,  // prev 10: ->00, ->01, ->10, ->11
      0, -1, 1, 0,  // prev 11: ->00, ->01, ->10, ->11
  };
  return kStepTable[((prevState & 0x3) << 2) | (newState & 0x3)];
}
