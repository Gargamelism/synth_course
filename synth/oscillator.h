#pragma once

#include <stdint.h>
#include "pins.h"
#include "harmonic_spreads.h"  // HarmonicSpread + per-spread weights

// Composite wavetables: one full period of the harmonic sum, precomputed per
// spread per mip level. A voice costs one lookup per sample instead of
// summing NUM_HARMONICS sine terms, which is what makes 32 voices affordable
// on the C3 (~13 cycles/voice against a 3628 cycle/frame budget).
//
// Mip levels band-limit against aliasing: level L covers base frequencies
// [WAVETABLE_MIP_BASE_HZ * 2^L, * 2^(L+1)) and is built with only the
// harmonics that stay under Nyquist across all of that range.
const int WAVETABLE_SIZE = SINE_TABLE_SIZE; // same size as the sine table, so
                                            // building a composite is an exact
                                            // lookup with no resampling
const int WAVETABLE_MIP_LEVELS = 9;         // 80 Hz -> Nyquist is log2(22050/80) ~ 8.1 octaves
const float WAVETABLE_MIP_BASE_HZ = FREQ_MIN_HZ;

// g_wavetable[spread][level] is null unless some voice in some kPatches
// entry uses that spread — initWavetables() only allocates what any patch
// references.
extern const int16_t *g_wavetable[SPREAD_COUNT][WAVETABLE_MIP_LEVELS];

// Builds the wavetables for every spread any kPatches entry mentions (not
// just the one active at startup, since the encoder can switch patches at
// runtime). Returns false if an allocation failed (each table is
// WAVETABLE_SIZE * 2 bytes, 9 per spread). Call once from setup(), after
// initSineTable().
bool initWavetables();

// Mip level for a playback frequency, clamped to the pyramid.
int mipLevelForFreq(float freqHz);

// How many harmonics level `level` keeps. Exposed for the band-limiting test.
int harmonicsForMipLevel(int level);

// Phase-accumulator (DDS) wavetable oscillator, one instance per voice.
class Oscillator {
public:
  void begin(HarmonicSpread spread);
  // Also re-picks the mip level, so this must be called whenever the
  // frequency changes (once per block per voice, off the per-sample path).
  void setFrequency(float freqHz);
  // One table lookup, bounded by +-SINE_TABLE_AMPLITUDE — the peak
  // mixOscillators assumes each voice respects.
  int16_t nextSample();

private:
  const int16_t *table_ = nullptr;
  uint32_t phase_ = 0;
  uint32_t phaseInc_ = 0;
  uint8_t spread_ = SPREAD_SAW;
};

// Full range of the 32-bit phase accumulator above (2^32), used to convert
// a frequency into a per-sample phase increment.
const double PHASE_ACCUMULATOR_RANGE = 4294967296.0;

// Shared sine lookup table, generated once in initSineTable(). Only used to
// build the wavetables above — nothing reads it on the audio path.
extern int16_t g_sineTable[SINE_TABLE_SIZE];
void initSineTable();

// Normalizes `count` relative voice levels (VoiceConfig::level) into Q15
// weights that sum to at most VOLUME_Q15_ONE. That is the headroom
// guarantee: with every voice at its normalized level and the master volume
// at full, the mix peaks at SINE_TABLE_AMPLITUDE no matter how many voices
// there are, so mixOscillators can never clip.
void normalizeVoiceLevelsQ15(const uint8_t *levels, int count, int16_t *outQ15);

// Sums `count` oscillator samples, each scaled by its Q15 volume
// (0 .. VOLUME_Q15_ONE), into one clipped int16_t output sample. Integer-only
// so it stays cheap in the per-sample audio loop on the FPU-less C3.
int16_t mixOscillators(const int16_t *samples, const int16_t *volumesQ15, int count);
