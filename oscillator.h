#pragma once

#include <stdint.h>
#include "pins.h"

// Phase-accumulator (DDS) sine oscillator, one instance per voice.
class Oscillator {
public:
  void begin();
  void setFrequency(float freqHz);
  int16_t nextSample();

private:
  uint32_t phase_ = 0;
  uint32_t phaseInc_ = 0;
};

// Full range of the 32-bit phase accumulator above (2^32), used to convert
// a frequency into a per-sample phase increment.
const double PHASE_ACCUMULATOR_RANGE = 4294967296.0;

// Shared sine lookup table, generated once in Oscillator::initTable().
extern int16_t g_sineTable[SINE_TABLE_SIZE];
void initSineTable();

// Sums `count` oscillator samples scaled by their volumes (0.0-1.0) into one
// clipped int16_t output sample.
int16_t mixOscillators(const int16_t *samples, const float *volumes, int count);
