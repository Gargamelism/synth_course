#include "distortion.h"
#include "pins.h"

static inline int16_t clampToInt16(int32_t v) {
  if (v > INT16_MAX) return INT16_MAX;
  if (v < INT16_MIN) return INT16_MIN;
  return (int16_t)v;
}

#if defined(DISTORTION_HARD_CLIP)

int16_t applyDistortion(int16_t sample) {
  return clampToInt16((int32_t)sample * DISTORTION_DRIVE_GAIN);
}

#elif defined(DISTORTION_SOFT_CLIP)

// Cubic soft-clip: y = x - x^3/3, evaluated entirely in Q15
// fixed point (VOLUME_Q15_SHIFT, the same fixed-point format the mixer
// uses) so it stays integer-only on the FPU-less C3. The curve is
// monotonic and self-bounded for |x| <= 32767, so no final clamp is
// needed.
int16_t applyDistortion(int16_t sample) {
  // 1/3 as a Q16 reciprocal (65536/3 rounded up): a multiply-and-shift
  // instead of the multi-cycle hardware divide -Os emits for "/ 3". The
  // error is under 1e-4, so the curve stays monotonic and self-bounded —
  // which a power-of-two divisor would not preserve.
  const int32_t kCubicReciprocalQ16 = 21846; // ~ (1 << 16) / 3

  int32_t x = clampToInt16((int32_t)sample * DISTORTION_DRIVE_GAIN);
  int32_t x2 = (x * x) >> VOLUME_Q15_SHIFT;  // x^2, still Q15
  int32_t x3 = (x2 * x) >> VOLUME_Q15_SHIFT; // x^3, still Q15
  return (int16_t)(x - ((x3 * kCubicReciprocalQ16) >> 16));
}

#elif defined(DISTORTION_FOLDBACK)

// Reflects the signal back down/up at +-DISTORTION_FOLD_THRESHOLD instead
// of clipping it, for a metallic wavefolder timbre. Each reflection can
// overshoot the opposite rail, so the loop alternates direction until the
// sample settles inside the threshold; the iteration cap plus the final
// clamp keep this bounded and safe on the real-time audio task even in
// pathological cases.
int16_t applyDistortion(int16_t sample) {
  const int kMaxFoldIterations = 8; // bounds the loop for real-time safety

  int32_t x = (int32_t)sample * DISTORTION_DRIVE_GAIN;
  const int32_t threshold = DISTORTION_FOLD_THRESHOLD;
  for (int i = 0; i < kMaxFoldIterations && (x > threshold || x < -threshold); i++) {
    if (x > threshold) {
      x = 2 * threshold - x;
    } else {
      x = -2 * threshold - x;
    }
  }
  return clampToInt16(x);
}

#elif defined(DISTORTION_BITCRUSH)

// Zeroes the low DISTORTION_BITCRUSH_BITS bits, quantizing the waveform
// into fewer amplitude steps for a lo-fi/aliased crunch. No multiply
// needed, so it's the cheapest option here.
int16_t applyDistortion(int16_t sample) {
  const int16_t mask = (int16_t)(~((1 << DISTORTION_BITCRUSH_BITS) - 1));
  return sample & mask;
}

#else

int16_t applyDistortion(int16_t sample) {
  return sample;
}

#endif
