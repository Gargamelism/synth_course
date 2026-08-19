#include "oscillator.h"
#include <math.h>

int16_t g_sineTable[SINE_TABLE_SIZE];

void initSineTable() {
  for (int tableIndex = 0; tableIndex < SINE_TABLE_SIZE; tableIndex++) {
    float phase = (2.0f * (float)M_PI * tableIndex) / SINE_TABLE_SIZE;
    g_sineTable[tableIndex] = (int16_t)(sinf(phase) * SINE_TABLE_AMPLITUDE);
  }
}

void Oscillator::begin() {
  phase_ = 0;
  phaseInc_ = 0;
}

void Oscillator::setFrequency(float freqHz) {
  // 32-bit phase accumulator: phaseInc = freqHz * (2^32 / sampleRate)
  phaseInc_ = (uint32_t)((double)freqHz * (PHASE_ACCUMULATOR_RANGE / SAMPLE_RATE_HZ));
}

int16_t Oscillator::nextSample() {
  uint8_t index = (uint8_t)(phase_ >> 24); // top 8 bits index the 256-entry table
  phase_ += phaseInc_;
  return g_sineTable[index];
}

int16_t mixOscillators(const int16_t *samples, const float *volumes, int count) {
  int32_t mixed = 0;
  for (int i = 0; i < count; i++) {
    mixed += (int32_t)(samples[i] * volumes[i]);
  }
  if (mixed > INT16_MAX) mixed = INT16_MAX;
  if (mixed < INT16_MIN) mixed = INT16_MIN;
  return (int16_t)mixed;
}
