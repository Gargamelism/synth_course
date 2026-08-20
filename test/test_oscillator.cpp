#include "../oscillator.h"
#include "framework.h"

static void runTests() {
  initSineTable();

  // Sine table at the four quadrant boundaries (256-entry table).
  CHECK_NEAR(g_sineTable[0], 0, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 4], SINE_TABLE_AMPLITUDE, 5);
  CHECK_NEAR(g_sineTable[SINE_TABLE_SIZE / 2], 0, 5);
  CHECK_NEAR(g_sineTable[3 * SINE_TABLE_SIZE / 4], -SINE_TABLE_AMPLITUDE, 5);

  // At freq = SAMPLE_RATE_HZ / SINE_TABLE_SIZE, phaseInc_ is exactly
  // 2^32 / SINE_TABLE_SIZE, so each nextSample() advances the table index
  // by exactly 1 — output should walk the table in order, then wrap.
  Oscillator osc;
  osc.begin();
  osc.setFrequency((float)SAMPLE_RATE_HZ / SINE_TABLE_SIZE);
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    CHECK(osc.nextSample() == g_sineTable[i]);
  }
  CHECK(osc.nextSample() == g_sineTable[0]);

  // freq = 0 (post-begin(), pre-setFrequency) never advances.
  Oscillator silent;
  silent.begin();
  CHECK(silent.nextSample() == g_sineTable[0]);
  CHECK(silent.nextSample() == g_sineTable[0]);

  // mixOscillators: positive/negative clipping at the int16_t range.
  {
    int16_t samples[3] = {30000, 30000, 30000};
    float volumes[3] = {1.0f, 1.0f, 1.0f};
    CHECK(mixOscillators(samples, volumes, 3) == INT16_MAX);
  }
  {
    int16_t samples[3] = {-30000, -30000, -30000};
    float volumes[3] = {1.0f, 1.0f, 1.0f};
    CHECK(mixOscillators(samples, volumes, 3) == INT16_MIN);
  }
  // mixOscillators: normal (non-clipping) sum, including a muted voice.
  {
    int16_t samples[3] = {1000, -1000, 500};
    float volumes[3] = {0.5f, 0.5f, 0.0f};
    CHECK(mixOscillators(samples, volumes, 3) == 0);
  }
  {
    int16_t samples[1] = {1000};
    float volumes[1] = {0.25f};
    CHECK(mixOscillators(samples, volumes, 1) == 250);
  }
}

TEST_MAIN()
