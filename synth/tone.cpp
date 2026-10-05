#include "tone.h"
#include "pins.h" // SAMPLE_RATE_HZ
#include <math.h>

float toneCutoffHz(const ToneConfig &tone, float filterEnvGain, float volume) {
  const float range = (float)tone.cutoffMaxHz - (float)tone.cutoffMinHz;
  return tone.cutoffMinHz + range * filterEnvGain * (0.3f + 0.7f * volume);
}

int32_t toneCoefficientQ16(float cutoffHz) {
  if (cutoffHz >= SAMPLE_RATE_HZ / 2) {
    return 1 << 16;
  }
  const float k = 1.0f - expf(-2.0f * (float)M_PI * cutoffHz / SAMPLE_RATE_HZ);
  return (int32_t)(k * (1 << 16) + 0.5f);
}
