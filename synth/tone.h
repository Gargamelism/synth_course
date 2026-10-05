#pragma once

#include <stdint.h>
#include "envelope.h" // EnvelopeConfig

// Per-patch brightness: a one-pole low-pass on the mixed signal
// whose cutoff is swept by its own envelope and opened further by the master
// volume pot, so plucks darken as they decay and brass brightens as it gets
// louder. Block rate: toneCutoffHz() -> toneCoefficientQ16(). Sample rate:
// OnePoleLowpass::process().
//
// Kept free of Arduino headers so test/run.sh can build it on the host.
struct ToneConfig {
  EnvelopeConfig filter; // sweeps the cutoff from cutoffMinHz (0) to cutoffMaxHz (1)
  uint16_t cutoffMinHz;
  uint16_t cutoffMaxHz;  // >= SAMPLE_RATE_HZ / 2 with cutoffMinHz the same: filter bypassed
};

// cutoffMin + (cutoffMax - cutoffMin) * filterEnvGain * (0.3 + 0.7 * volume);
// filterEnvGain and volume are both 0..1.
float toneCutoffHz(const ToneConfig &tone, float filterEnvGain, float volume);

// k = 1 - exp(-2*pi*fc/fs) in Q16. At or above Nyquist returns exactly
// 1 << 16, which makes OnePoleLowpass a bit-exact pass-through.
int32_t toneCoefficientQ16(float cutoffHz);

class OnePoleLowpass {
public:
  void setCoefficientQ16(int32_t kQ16) { kQ16_ = kQ16; }

  // y += (x - y) * k. The state carries kStateFracBits extra fractional bits
  // so a low cutoff's tiny per-sample step doesn't truncate to 0 and leave
  // the output stuck short of its input.
  int16_t process(int16_t x) {
    const int32_t xFixed = (int32_t)x * (1 << kStateFracBits);
    y_ += (int32_t)(((int64_t)(xFixed - y_) * kQ16_) >> 16);
    return (int16_t)(y_ >> kStateFracBits);
  }

private:
  static const int kStateFracBits = 8;
  int32_t kQ16_ = 1 << 16;
  int32_t y_ = 0;
};
