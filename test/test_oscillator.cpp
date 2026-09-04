#include "../oscillator.h"
#include "framework.h"

static void runTests() {
  initSineTable();
  initHarmonicWeights();

  // Sine table at the four quadrant boundaries (256-entry table).
  CHECK_NEAR(g_sineTable[0], 0, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 4], SINE_TABLE_AMPLITUDE, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 2], 0, 5);
  CHECK_NEAR(g_sineTable[3 * SINE_TABLE_SIZE / 4], -SINE_TABLE_AMPLITUDE, 5);

  // At freq = SAMPLE_RATE_HZ / SINE_TABLE_SIZE, phaseInc_ is exactly
  // 2^32 / SINE_TABLE_SIZE, so each nextSample() advances the fundamental's
  // table index by exactly 1. Output should walk the harmonic-summed table
  // in order (same formula as Oscillator::nextSample()), then wrap.
  Oscillator osc;
  osc.begin();
  osc.setFrequency((float)SAMPLE_RATE_HZ / SINE_TABLE_SIZE);
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    int32_t expected = 0;
    for (int h = 0; h < NUM_HARMONICS; h++) {
      int index = (i * (h + 1)) % SINE_TABLE_SIZE;
      expected += ((int32_t)g_sineTable[index] * g_harmonicWeightsQ15[h]) >> VOLUME_Q15_SHIFT;
    }
    if (expected > SINE_TABLE_AMPLITUDE) expected = SINE_TABLE_AMPLITUDE;
    if (expected < -SINE_TABLE_AMPLITUDE) expected = -SINE_TABLE_AMPLITUDE;
    CHECK(osc.nextSample() == (int16_t)expected);
  }

  // freq = 0 (post-begin(), pre-setFrequency) never advances, so every
  // harmonic's index stays put and each call returns the same value.
  Oscillator silent;
  silent.begin();
  CHECK(silent.nextSample() == silent.nextSample());

  // Headroom invariant: g_harmonicWeightsQ15 is normalized to sum to
  // VOLUME_Q15_ONE, so peak output must stay within SINE_TABLE_AMPLITUDE
  // (the same bound mixOscillators assumes a single oscillator respects) —
  // sweep a full period at the top of the pitch range to check it holds.
  Oscillator loud;
  loud.begin();
  loud.setFrequency(FREQ_MAX_HZ);
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    int16_t s = loud.nextSample();
    CHECK(s >= -SINE_TABLE_AMPLITUDE && s <= SINE_TABLE_AMPLITUDE);
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
