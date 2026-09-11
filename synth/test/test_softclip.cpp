// Tests DIST_SOFT_CLIP's Q16 reciprocal against the integer divide it
// replaces. distortion.cpp compiles every flavor unconditionally now (the
// encoder picks one at runtime), so this includes the translation unit
// directly and calls applyDistortion(sample, DIST_SOFT_CLIP) through the
// real dispatcher — the name test_softclip.cpp (not test_distortion.cpp)
// keeps run.sh from also auto-linking ../distortion.cpp, which would
// double-define everything in it (it pairs test_<name>.cpp with
// ../<name>.cpp).
#include "../distortion.cpp"
#include "framework.h"

// The curve as it was written before the reciprocal: y = x - x^3/3 with a
// real divide, everything else identical.
static int16_t softClipWithDivide(int16_t sample) {
  int32_t x = clampToInt16((int32_t)sample * DISTORTION_DRIVE_GAIN);
  int32_t x2 = (x * x) >> VOLUME_Q15_SHIFT;
  int32_t x3 = (x2 * x) >> VOLUME_Q15_SHIFT;
  return (int16_t)(x - x3 / 3);
}

static void runTests() {
  int16_t previous = applyDistortion(INT16_MIN, DIST_SOFT_CLIP);
  for (int32_t sample = INT16_MIN; sample <= INT16_MAX; sample += 7) {
    const int16_t actual = applyDistortion((int16_t)sample, DIST_SOFT_CLIP);

    // Within 1 LSB of the divide it replaces.
    const int32_t difference = (int32_t)actual - softClipWithDivide((int16_t)sample);
    CHECK(difference >= -1 && difference <= 1);

    // The comment's claim still has to hold: monotonic and self-bounded, so
    // no final clamp is needed.
    CHECK(actual >= previous);
    previous = actual;
  }

  // Silence in, silence out; the curve is odd-symmetric about zero.
  CHECK(applyDistortion(0, DIST_SOFT_CLIP) == 0);
  CHECK_NEAR(applyDistortion(1000, DIST_SOFT_CLIP), -applyDistortion(-1000, DIST_SOFT_CLIP), 1);
}

TEST_MAIN()
