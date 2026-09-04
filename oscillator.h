#pragma once

#include <stdint.h>
#include "pins.h"

// Phase-accumulator (DDS) sine oscillator, one instance per voice.
class Oscillator {
public:
  void begin();
  void setFrequency(float freqHz);
  // Sums the fundamental with NUM_HARMONICS-1 natural overtones (see
  // g_harmonicWeightsQ15), still clipped to the same +-SINE_TABLE_AMPLITUDE
  // peak a single sine had — mixOscillators/distortion.h rely on that.
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

// Per-harmonic Q15 weight (index 0 = fundamental/1x .. NUM_HARMONICS-1 =
// highest overtone), generated once in initHarmonicWeights(): a natural
// (1/n) harmonic series, normalized so the weights sum to VOLUME_Q15_ONE —
// keeps a single oscillator's peak within SINE_TABLE_AMPLITUDE, the same
// invariant mixOscillators already assumes. Computed at startup (not
// hardcoded) so NUM_HARMONICS can change without hand-updating weights.
extern int16_t g_harmonicWeightsQ15[NUM_HARMONICS];
void initHarmonicWeights();

// Sums `count` oscillator samples, each scaled by its Q15 volume
// (0 .. VOLUME_Q15_ONE), into one clipped int16_t output sample. Integer-only
// so it stays cheap in the per-sample audio loop on the FPU-less C3.
int16_t mixOscillators(const int16_t *samples, const int16_t *volumesQ15, int count);
