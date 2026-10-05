#include "../tone.h"
#include "../pins.h" // SAMPLE_RATE_HZ
#include "framework.h"
#include <cmath>
#include <cstdlib>

// Peak output over the second half of n samples of a full-scale-ish sine.
static int peakOfSine(OnePoleLowpass &filter, float hz, int n) {
  int peak = 0;
  for (int i = 0; i < n; i++) {
    int16_t x = (int16_t)(16000.0f * sinf(2.0f * (float)M_PI * hz * i / SAMPLE_RATE_HZ));
    int16_t y = filter.process(x);
    if (i >= n / 2 && std::abs(y) > peak) peak = std::abs(y);
  }
  return peak;
}

static void runTests() {
  // Low cutoff still settles exactly on a DC input: the extra state bits
  // keep tiny steps from truncating to 0 and stalling short of the target.
  {
    OnePoleLowpass filter;
    filter.setCoefficientQ16(toneCoefficientQ16(100.0f));
    int16_t y = 0;
    for (int i = 0; i < SAMPLE_RATE_HZ; i++) y = filter.process(1000);
    CHECK_NEAR(y, 1000, 1);
    for (int i = 0; i < SAMPLE_RATE_HZ; i++) y = filter.process(-1000);
    CHECK_NEAR(y, -1000, 1);
  }

  // A 10 kHz sine through a 500 Hz cutoff is cut to roughly the first-order
  // |H| = fc / f (~1/20); well below the cutoff it passes almost unchanged.
  {
    OnePoleLowpass filter;
    filter.setCoefficientQ16(toneCoefficientQ16(500.0f));
    int high = peakOfSine(filter, 10000.0f, 4410);
    CHECK(high < 16000 / 10);
    int low = peakOfSine(filter, 50.0f, 8820);
    CHECK(low > 16000 * 9 / 10);
  }

  // At/above Nyquist the filter is a bit-exact pass-through (kToneOpen).
  {
    CHECK(toneCoefficientQ16(SAMPLE_RATE_HZ / 2) == (1 << 16));
    OnePoleLowpass filter;
    filter.setCoefficientQ16(toneCoefficientQ16(SAMPLE_RATE_HZ / 2));
    CHECK(filter.process(12345) == 12345);
    CHECK(filter.process(-32768) == -32768);
    CHECK(filter.process(32767) == 32767);
  }

  // Cutoff mapping: env 0 -> min; env 1 at full volume -> max; env 1 at zero
  // volume -> 30% of the range.
  {
    ToneConfig tone = {{1, 300, 20, 150}, 1000, 11000};
    CHECK_NEAR(toneCutoffHz(tone, 0.0f, 1.0f), 1000.0f, 0.5f);
    CHECK_NEAR(toneCutoffHz(tone, 1.0f, 1.0f), 11000.0f, 0.5f);
    CHECK_NEAR(toneCutoffHz(tone, 1.0f, 0.0f), 4000.0f, 0.5f);
  }
}

TEST_MAIN()
