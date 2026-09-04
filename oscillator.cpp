#include "oscillator.h"
#include <math.h>

int16_t g_sineTable[SINE_TABLE_SIZE];

// SINE_TABLE_SIZE (256) is 2^8, so the top 8 bits of the 32-bit phase
// accumulator select the table entry directly.
const int PHASE_INDEX_SHIFT = 24;

static inline uint8_t getIndex(uint32_t phase) {
  return (uint8_t)(phase >> PHASE_INDEX_SHIFT);
}

void initSineTable() {
  for (int tableIndex = 0; tableIndex < SINE_TABLE_SIZE; tableIndex++) {
    float phase = (2.0f * (float)M_PI * tableIndex) / SINE_TABLE_SIZE;
    g_sineTable[tableIndex] = (int16_t)(sinf(phase) * SINE_TABLE_AMPLITUDE);
  }
}

int16_t g_harmonicWeightsQ15[NUM_HARMONICS];

void initHarmonicWeights() {
  // Natural harmonic series: the nth harmonic is 1/n as loud as the
  // fundamental. Sum first, then normalize so the weights add up to exactly
  // VOLUME_Q15_ONE — this bounds the worst-case (all harmonics in phase)
  // peak to SINE_TABLE_AMPLITUDE, same as a single un-enriched sine.
  float sum = 0.0f;
  for (int h = 0; h < NUM_HARMONICS; h++) {
    sum += 1.0f / (h + 1);
  }
  for (int h = 0; h < NUM_HARMONICS; h++) {
    float weight = (1.0f / (h + 1)) / sum;
    g_harmonicWeightsQ15[h] = (int16_t)(weight * VOLUME_Q15_ONE + 0.5f);
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
  int32_t sum = 0;
  for (int h = 0; h < NUM_HARMONICS; h++) {
    // (phase_ * n) mod 2^32 is the correct instantaneous phase for the nth
    // harmonic — uint32_t wraparound gives the modulo for free, so no extra
    // phase accumulators are needed per harmonic.
    uint32_t harmonicPhase = phase_ * (uint32_t)(h + 1);
    sum += ((int32_t)g_sineTable[getIndex(harmonicPhase)] * g_harmonicWeightsQ15[h]) >> VOLUME_Q15_SHIFT;
  }
  phase_ += phaseInc_;
  // Defensive clamp mirroring mixOscillators' own belt-and-suspenders clamp:
  // guards against a future initHarmonicWeights() edit breaking the
  // sum-to-VOLUME_Q15_ONE invariant the weights are normalized to.
  if (sum > SINE_TABLE_AMPLITUDE) sum = SINE_TABLE_AMPLITUDE;
  if (sum < -SINE_TABLE_AMPLITUDE) sum = -SINE_TABLE_AMPLITUDE;
  return (int16_t)sum;
}

int16_t mixOscillators(const int16_t *samples, const int16_t *volumesQ15, int count) {
  int32_t mixed = 0;
  for (int i = 0; i < count; i++) {
    // samples[i] is <= SINE_TABLE_AMPLITUDE, volumesQ15[i] <= 32767, so the
    // product fits int32_t with room to spare before the >> 15 rescale.
    mixed += ((int32_t)samples[i] * volumesQ15[i]) >> VOLUME_Q15_SHIFT;
  }
  if (mixed > INT16_MAX) mixed = INT16_MAX;
  if (mixed < INT16_MIN) mixed = INT16_MIN;
  return (int16_t)mixed;
}
