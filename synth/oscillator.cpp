#include "oscillator.h"
#include "voices.h"
#include <math.h>
#include <stdlib.h>

int16_t g_sineTable[SINE_TABLE_SIZE];
const int16_t *g_wavetable[SPREAD_COUNT][WAVETABLE_MIP_LEVELS] = {};

// SINE_TABLE_SIZE (1024) is 2^10, so the top 10 bits of the 32-bit phase
// accumulator select the table entry directly.
const int PHASE_INDEX_SHIFT = 22;
static_assert((1u << (32 - PHASE_INDEX_SHIFT)) == SINE_TABLE_SIZE,
              "PHASE_INDEX_SHIFT must match SINE_TABLE_SIZE");

static inline int getIndex(uint32_t phase) {
  return (int)(phase >> PHASE_INDEX_SHIFT);
}

void initSineTable() {
  for (int tableIndex = 0; tableIndex < SINE_TABLE_SIZE; tableIndex++) {
    float phase = (2.0f * (float)M_PI * tableIndex) / SINE_TABLE_SIZE;
    g_sineTable[tableIndex] = (int16_t)(sinf(phase) * SINE_TABLE_AMPLITUDE);
  }
}

// --- Wavetables ------------------------------------------------------------

int harmonicsForMipLevel(int level) {
  // Band-limit against the TOP of the level's octave, not its base, so a
  // voice anywhere inside the level stays alias-free.
  const float topHz = WAVETABLE_MIP_BASE_HZ * (float)(2 << level);
  int count = (int)((SAMPLE_RATE_HZ / 2.0f) / topHz);
  if (count > NUM_HARMONICS) count = NUM_HARMONICS;
  if (count < 1) count = 1; // the top level keeps a bare fundamental
  return count;
}

int mipLevelForFreq(float freqHz) {
  if (freqHz <= WAVETABLE_MIP_BASE_HZ) return 0;
  int level = (int)floorf(log2f(freqHz / WAVETABLE_MIP_BASE_HZ));
  if (level < 0) level = 0;
  if (level > WAVETABLE_MIP_LEVELS - 1) level = WAVETABLE_MIP_LEVELS - 1;
  return level;
}

// Margin added on top of a voice's worst-case offset in maxMipLevelForSpread()
// below, so the root pot's snap-to-scale-degree and Phase 3's future vibrato
// never reach past the mip level actually built.
static const float kDiatonicMarginSemitones = 1.5f;
static const float kDetuneMarginCents = 150.0f;

int maxMipLevelForSpread(int spread) {
  float maxHz = 0.0f;
  for (int p = 0; p < NUM_PATCHES; p++) {
    const Patch &patch = kPatches[p];
    for (int v = 0; v < patch.voiceCount; v++) {
      const VoiceConfig &voice = patch.voices[v];
      if (voice.spread != spread) continue;
      float hz;
      if (patch.pitchMode == PITCH_DIATONIC) {
        const int semitones = scaleDegreeToSemitone(voice.scaleDegree, patch.scale);
        hz = FREQ_MAX_HZ * powf(2.0f, (semitones + kDiatonicMarginSemitones) / kSemitonesPerOctave);
      } else {
        hz = FREQ_MAX_HZ * powf(2.0f, (voice.detuneCents + kDetuneMarginCents) / kCentsPerOctave);
      }
      if (hz > maxHz) maxHz = hz;
    }
  }
  return maxHz > 0.0f ? mipLevelForFreq(maxHz) : 0;
}

size_t wavetableBytesAllocated() {
  size_t bytes = 0;
  for (int spread = 0; spread < SPREAD_COUNT; spread++) {
    for (int level = 0; level < WAVETABLE_MIP_LEVELS; level++) {
      if (g_wavetable[spread][level] != nullptr) bytes += WAVETABLE_SIZE * sizeof(int16_t);
    }
  }
  return bytes;
}

static void buildTable(int16_t *table, int spread, int harmonicCount) {
  // Sum the terms first, then scale so they total VOLUME_Q15_ONE — that
  // bounds the composite's worst-case (all harmonics in phase) peak to
  // SINE_TABLE_AMPLITUDE, whichever spread and level this is.
  int16_t weightsQ15[NUM_HARMONICS];
  float sum = 0.0f;
  for (int h = 0; h < harmonicCount; h++) {
    sum += harmonicWeightTerm(spread, h);
  }
  for (int h = 0; h < harmonicCount; h++) {
    const float weight = harmonicWeightTerm(spread, h) / sum;
    weightsQ15[h] = (int16_t)(weight * VOLUME_Q15_ONE + 0.5f);
  }

  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    int32_t sample = 0;
    for (int h = 0; h < harmonicCount; h++) {
      // The nth harmonic advances n times as fast; the table is a power of
      // two, so the wrap is a mask.
      const int index = (i * (h + 1)) & (WAVETABLE_SIZE - 1);
      sample += ((int32_t)g_sineTable[index] * weightsQ15[h]) >> VOLUME_Q15_SHIFT;
    }
    // Defensive clamp against Q15 rounding in the weights above nudging the
    // peak past the bound the mixer relies on.
    if (sample > SINE_TABLE_AMPLITUDE) sample = SINE_TABLE_AMPLITUDE;
    if (sample < -SINE_TABLE_AMPLITUDE) sample = -SINE_TABLE_AMPLITUDE;
    table[i] = (int16_t)sample;
  }

  // The weight sum above bounds the peak only in the worst case, every
  // harmonic cresting together, which real harmonic sums never do (1/n
  // peaks at ~0.48 of it). Rescale to the measured peak so every spread and
  // level uses the full SINE_TABLE_AMPLITUDE; truncating toward zero keeps
  // the bound the mixer relies on.
  int32_t peak = 1;
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    const int32_t magnitude = table[i] < 0 ? -table[i] : table[i];
    if (magnitude > peak) peak = magnitude;
  }
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    table[i] = (int16_t)((int32_t)table[i] * SINE_TABLE_AMPLITUDE / peak);
  }
}

bool initWavetables() {
  // Every spread any patch might use has to be built up front: the encoder
  // can switch kPatches at runtime, so "the instrument" is now the union
  // of every patch's voices, not just the one active at startup.
  bool spreadUsed[SPREAD_COUNT] = {};
  for (int patchIndex = 0; patchIndex < NUM_PATCHES; patchIndex++) {
    const Patch &patch = kPatches[patchIndex];
    for (int voiceIndex = 0; voiceIndex < patch.voiceCount; voiceIndex++) {
      spreadUsed[patch.voices[voiceIndex].spread] = true;
    }
  }

  // Only build as high into the mip pyramid as some voice can actually reach
  // — most spreads never need the top few (unreachable) octaves.
  int maxLevel[SPREAD_COUNT] = {};
  for (int spread = 0; spread < SPREAD_COUNT; spread++) {
    if (spreadUsed[spread]) maxLevel[spread] = maxMipLevelForSpread(spread);
  }

  bool ok = true;
  for (int spread = 0; spread < SPREAD_COUNT; spread++) {
    if (!spreadUsed[spread]) continue; // unused spreads cost no RAM
    for (int level = 0; level <= maxLevel[spread]; level++) {
      if (g_wavetable[spread][level] != nullptr) continue;
      int16_t *table = (int16_t *)malloc(WAVETABLE_SIZE * sizeof(int16_t));
      if (table == nullptr) {
        ok = false;
        continue;
      }
      buildTable(table, spread, harmonicsForMipLevel(level));
      g_wavetable[spread][level] = table;
    }
  }
  return ok;
}

// --- Oscillator ------------------------------------------------------------

void Oscillator::begin(HarmonicSpread spread) {
  phase_ = 0;
  phaseInc_ = 0;
  spread_ = (uint8_t)spread;
  table_ = g_wavetable[spread_][0];
}

void Oscillator::setFrequency(float freqHz) {
  // 32-bit phase accumulator: phaseInc = freqHz * (2^32 / sampleRate)
  phaseInc_ = (uint32_t)((double)freqHz * (PHASE_ACCUMULATOR_RANGE / SAMPLE_RATE_HZ));
  const int16_t *table = g_wavetable[spread_][mipLevelForFreq(freqHz)];
  if (table != nullptr) {
    table_ = table;
  }
}

int16_t Oscillator::nextSample() {
  // Guards the case where initWavetables() could not allocate: silence is
  // survivable on the audio task, a null dereference is not.
  if (table_ == nullptr) return 0;
  const int16_t sample = table_[getIndex(phase_)];
  phase_ += phaseInc_;
  return sample;
}

// --- Mixing ----------------------------------------------------------------

void normalizeVoiceLevelsQ15(const uint8_t *levels, int count, int16_t *outQ15) {
  int32_t sum = 0;
  for (int i = 0; i < count; i++) {
    sum += levels[i];
  }
  for (int i = 0; i < count; i++) {
    // Truncating (not rounding) keeps the total at or below VOLUME_Q15_ONE
    // for any voice count, which is what makes clipping impossible rather
    // than merely unlikely.
    outQ15[i] = sum > 0 ? (int16_t)((int32_t)levels[i] * VOLUME_Q15_ONE / sum) : 0;
  }
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
