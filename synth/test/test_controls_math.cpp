#include <cmath>
#include "../controls_math.h"
#include "framework.h"

static void runTests() {
  // smoothValue: below the hysteresis threshold, filtered/lastRaw hold.
  {
    float filtered = 100.0f;
    int lastRaw = 100;
    float result = smoothValue(102, &filtered, &lastRaw, /*hysteresisCounts=*/4, /*emaAlpha=*/0.15f);
    CHECK_NEAR(result, 100.0f, 1e-4f);
    CHECK_NEAR(filtered, 100.0f, 1e-4f);
    CHECK(lastRaw == 100);
  }
  // smoothValue: at/above the threshold, applies the EMA step.
  {
    float filtered = 100.0f;
    int lastRaw = 100;
    float result = smoothValue(200, &filtered, &lastRaw, 4, 0.15f);
    float expected = 100.0f * (1.0f - 0.15f) + 200.0f * 0.15f; // 115.0
    CHECK_NEAR(result, expected, 1e-4f);
    CHECK_NEAR(filtered, expected, 1e-4f);
    CHECK(lastRaw == 200);
  }

  // mapPitchHz: exponential sweep, exact at the endpoints.
  CHECK_NEAR(mapPitchHz(0.0f, 80.0f, 1000.0f), 80.0f, 1e-2f);
  CHECK_NEAR(mapPitchHz(1.0f, 80.0f, 1000.0f), 1000.0f, 1e-1f);
  // Midpoint of an exponential sweep is the geometric mean.
  CHECK_NEAR(mapPitchHz(0.5f, 80.0f, 1000.0f), std::sqrt(80.0f * 1000.0f), 1e-1f);

  // quadratureStep: one full clockwise cycle around the Gray code (00 -> 01
  // -> 11 -> 10 -> 00) scores +1 at every edge; the reverse cycle scores -1.
  CHECK(quadratureStep(0b00, 0b01) == 1);
  CHECK(quadratureStep(0b01, 0b11) == 1);
  CHECK(quadratureStep(0b11, 0b10) == 1);
  CHECK(quadratureStep(0b10, 0b00) == 1);
  CHECK(quadratureStep(0b00, 0b10) == -1);
  CHECK(quadratureStep(0b10, 0b11) == -1);
  CHECK(quadratureStep(0b11, 0b01) == -1);
  CHECK(quadratureStep(0b01, 0b00) == -1);
  // A repeated state (no edge) or a two-bit jump (only possible via contact
  // bounce — a real detent never skips a Gray code state) both score 0.
  CHECK(quadratureStep(0b00, 0b00) == 0);
  CHECK(quadratureStep(0b01, 0b01) == 0);
  CHECK(quadratureStep(0b00, 0b11) == 0);
  CHECK(quadratureStep(0b01, 0b10) == 0);
}

TEST_MAIN()
