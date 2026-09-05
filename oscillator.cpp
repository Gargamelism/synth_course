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

#if defined(HARMONIC_SPREAD_NATURAL)

// The nth harmonic is 1/n as loud as the fundamental — a real string/wind
// instrument's overtone falloff.
static inline float harmonicWeightTerm(int h) {
  return 1.0f / (h + 1);
}

#elif defined(HARMONIC_SPREAD_OCTAVE)

// Only octave harmonics (1x, 2x, 4x, 8x, 16x...) sound, each at the natural
// series' 1/n falloff; every non-octave harmonic is silent. Sparse and
// hollow, like an organ's octave-only stops.
static inline bool isPowerOfTwo(int n) {
  return n > 0 && (n & (n - 1)) == 0;
}
static inline float harmonicWeightTerm(int h) {
  const int n = h + 1;
  return isPowerOfTwo(n) ? 1.0f / n : 0.0f;
}

#elif defined(HARMONIC_SPREAD_ODD)

// Only odd harmonics (1x, 3x, 5x...) sound, at the natural series' 1/n
// falloff; even harmonics are silent — a square/clarinet-like spectrum.
static inline float harmonicWeightTerm(int h) {
  const int n = h + 1;
  return (n % 2 == 1) ? 1.0f / n : 0.0f;
}

#elif defined(HARMONIC_SPREAD_EQUAL)

// Every harmonic at equal weight, no falloff — dense and buzzy.
static inline float harmonicWeightTerm(int h) {
  (void)h;
  return 1.0f;
}

#elif defined(HARMONIC_SPREAD_VIOLA)

// Cheap bowed-string approximation: natural 1/n falloff, plus a boosted
// "formant" bump on the 3rd-6th harmonics (the nasal, woody quality that
// separates a viola from a plain sawtooth spectrum), plus a faster rolloff
// above the 8th harmonic for a mellower top end. The bump sits on fixed
// harmonic numbers rather than a fixed frequency, so — unlike a real
// viola's body resonance — it shifts with pitch instead of staying put;
// still close enough to be recognizable across this synth's pitch range.
static inline float harmonicWeightTerm(int h) {
  const int n = h + 1;
  const float falloff = 1.0f / n;
  const float formant = (n >= 3 && n <= 6) ? 1.6f : 1.0f;
  const float highRolloff = (n > 8) ? 0.5f : 1.0f;
  return falloff * formant * highRolloff;
}

#endif

void initHarmonicWeights() {
  // Sum first, then normalize so the weights add up to exactly
  // VOLUME_Q15_ONE — this bounds the worst-case (all harmonics in phase)
  // peak to SINE_TABLE_AMPLITUDE, same as a single un-enriched sine,
  // regardless of which HARMONIC_SPREAD_* is selected in oscillator.h.
  float sum = 0.0f;
  for (int h = 0; h < NUM_HARMONICS; h++) {
    sum += harmonicWeightTerm(h);
  }
  for (int h = 0; h < NUM_HARMONICS; h++) {
    const float weight = harmonicWeightTerm(h) / sum;
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
