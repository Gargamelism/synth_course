#include "../oscillator.h"
#include "../voices.h"
#include "framework.h"

// Expected harmonic count per mip level: level L covers [80 * 2^L, 80 *
// 2^(L+1)) Hz and keeps only the harmonics that stay under Nyquist at the
// TOP of that range, capped at NUM_HARMONICS (22).
static const int kExpectedHarmonics[WAVETABLE_MIP_LEVELS] = {22, 22, 22, 17, 8, 4, 2, 1, 1};

static void runTests() {
  initSineTable();
  CHECK(initWavetables());

  // Sine table at the four quadrant boundaries.
  CHECK_NEAR(g_sineTable[0], 0, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 4], SINE_TABLE_AMPLITUDE, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 2], 0, 5);
  CHECK_NEAR(g_sineTable[3 * SINE_TABLE_SIZE / 4], -SINE_TABLE_AMPLITUDE, 5);

  // Band-limiting: every level's harmonic count matches the pyramid above,
  // and (bar the top level, which keeps a bare fundamental) the highest
  // harmonic stays under Nyquist across the whole level.
  for (int level = 0; level < WAVETABLE_MIP_LEVELS; level++) {
    const int count = harmonicsForMipLevel(level);
    CHECK(count == kExpectedHarmonics[level]);
    const float topHz = WAVETABLE_MIP_BASE_HZ * (float)(2 << level);
    CHECK(count == 1 || count * topHz < SAMPLE_RATE_HZ / 2.0f);
  }

  // Mip selection over the pyramid's range, including the clamps at both ends.
  CHECK(mipLevelForFreq(80.0f) == 0);
  CHECK(mipLevelForFreq(40.0f) == 0);
  CHECK(mipLevelForFreq(640.0f) == 3);
  CHECK(mipLevelForFreq(1280.0f) == 4);
  CHECK(mipLevelForFreq(21000.0f) == 8);

  // Headroom invariant: each table's harmonic weights are normalized to sum
  // to VOLUME_Q15_ONE, so no entry may exceed SINE_TABLE_AMPLITUDE — the
  // bound mixOscillators assumes every voice respects.
  for (int spread = 0; spread < SPREAD_COUNT; spread++) {
    for (int level = 0; level < WAVETABLE_MIP_LEVELS; level++) {
      const int16_t *table = g_wavetable[spread][level];
      if (table == nullptr) continue; // spread not referenced by any kPatches entry
      for (int i = 0; i < WAVETABLE_SIZE; i++) {
        CHECK(table[i] >= -SINE_TABLE_AMPLITUDE && table[i] <= SINE_TABLE_AMPLITUDE);
      }
    }
  }

  // Every spread (referenced by a patch or not) yields finite, non-negative
  // weights, and keeps some energy at every mip level — a level whose
  // retained harmonics all weigh 0 would divide by zero in buildTable().
  for (int spread = 0; spread < SPREAD_COUNT; spread++) {
    for (int h = 0; h < NUM_HARMONICS; h++) {
      const float w = harmonicWeightTerm(spread, h);
      CHECK(w >= 0.0f && w <= 1.0f);
    }
    for (int level = 0; level < WAVETABLE_MIP_LEVELS; level++) {
      float sum = 0.0f;
      for (int h = 0; h < harmonicsForMipLevel(level); h++) sum += harmonicWeightTerm(spread, h);
      CHECK(sum > 0.0f);
    }
  }

  // At freq = SAMPLE_RATE_HZ / WAVETABLE_SIZE the phase increment is exactly
  // 2^32 / WAVETABLE_SIZE, so each nextSample() advances the table index by
  // exactly 1 and the oscillator must walk its table in order, then wrap.
  const HarmonicSpread spread = (HarmonicSpread)kPatches[0].voices[0].spread;
  const float walkHz = (float)SAMPLE_RATE_HZ / WAVETABLE_SIZE;
  const int16_t *walkTable = g_wavetable[spread][mipLevelForFreq(walkHz)];
  Oscillator osc;
  osc.begin(spread);
  osc.setFrequency(walkHz);
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    CHECK(osc.nextSample() == walkTable[i]);
  }
  CHECK(osc.nextSample() == walkTable[0]); // wrapped

  // A voice picks its table from the frequency, so the same oscillator reads
  // a shorter harmonic series once it climbs into the next octave.
  osc.setFrequency(FREQ_MAX_HZ);
  CHECK(g_wavetable[spread][mipLevelForFreq(FREQ_MAX_HZ)] != walkTable);

  // freq = 0 (post-begin(), pre-setFrequency) never advances the phase, so
  // every call returns the same table entry.
  Oscillator silent;
  silent.begin(spread);
  CHECK(silent.nextSample() == silent.nextSample());

  // Headroom, the part that made more voices possible: normalized levels sum
  // to at most VOLUME_Q15_ONE, so a full-scale mix of MAX_VOICES voices at
  // full master volume must not reach the clipping rails.
  {
    uint8_t levels[MAX_VOICES];
    int16_t volumes[MAX_VOICES];
    int16_t samples[MAX_VOICES];
    for (int i = 0; i < MAX_VOICES; i++) {
      levels[i] = (uint8_t)(i + 1); // deliberately lopsided
    }
    normalizeVoiceLevelsQ15(levels, MAX_VOICES, volumes);

    int32_t sum = 0;
    for (int i = 0; i < MAX_VOICES; i++) sum += volumes[i];
    CHECK(sum <= VOLUME_Q15_ONE);

    for (int i = 0; i < MAX_VOICES; i++) samples[i] = SINE_TABLE_AMPLITUDE;
    CHECK(mixOscillators(samples, volumes, MAX_VOICES) < INT16_MAX);
    for (int i = 0; i < MAX_VOICES; i++) samples[i] = -SINE_TABLE_AMPLITUDE;
    CHECK(mixOscillators(samples, volumes, MAX_VOICES) > INT16_MIN);
  }

  // A single voice at full level keeps all of the master volume.
  {
    uint8_t levels[1] = {7};
    int16_t volumes[1];
    normalizeVoiceLevelsQ15(levels, 1, volumes);
    CHECK(volumes[0] == VOLUME_Q15_ONE);
  }

  // mixOscillators: positive/negative clipping at the int16_t range.
  {
    int16_t samples[3] = {30000, 30000, 30000};
    int16_t volumes[3] = {VOLUME_Q15_ONE, VOLUME_Q15_ONE, VOLUME_Q15_ONE};
    CHECK(mixOscillators(samples, volumes, 3) == INT16_MAX);
  }
  {
    int16_t samples[3] = {-30000, -30000, -30000};
    int16_t volumes[3] = {VOLUME_Q15_ONE, VOLUME_Q15_ONE, VOLUME_Q15_ONE};
    CHECK(mixOscillators(samples, volumes, 3) == INT16_MIN);
  }
  // mixOscillators: normal (non-clipping) sum, including a muted voice.
  // Q15 volumes: 16384 == 0.5, 8192 == 0.25, 0 == muted.
  {
    int16_t samples[3] = {1000, -1000, 500};
    int16_t volumes[3] = {16384, 16384, 0};
    CHECK(mixOscillators(samples, volumes, 3) == 0);
  }
  {
    int16_t samples[1] = {1000};
    int16_t volumes[1] = {8192};
    CHECK(mixOscillators(samples, volumes, 1) == 250);
  }
}

TEST_MAIN()
