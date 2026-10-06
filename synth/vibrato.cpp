#include "vibrato.h"
#include "pins.h" // SAMPLE_RATE_HZ
#include <math.h>

static const float kMsPerSecond = 1000.0f;
static const float kCentsPerOctave = 1200.0f;
static const float kPercentScale = 100.0f;
static const float kTwoPi = 2.0f * (float)M_PI;

void Vibrato::noteOn() {
  phase_ = 0.0f;
  msSinceNoteOn_ = 0.0f;
}

void Vibrato::advanceBlock(int frames) {
  const float blockMs = frames * kMsPerSecond / SAMPLE_RATE_HZ;
  msSinceNoteOn_ += blockMs;
  phase_ += config_.rateHz * blockMs / kMsPerSecond;
  phase_ -= floorf(phase_);

  // 0 during the delay, fade-in during onsetMs, then 1 for the rest of
  // the note (onsetMs == 0: straight to full depth once the delay is over).
  const float msSinceDelay = msSinceNoteOn_ - config_.delayMs;
  float onset = 1.0f;
  if (msSinceDelay < 0.0f) {
    onset = 0.0f;
  } else if (msSinceDelay < config_.onsetMs) {
    onset = msSinceDelay / config_.onsetMs;
  }
  const float lfo = sinf(kTwoPi * phase_);
  pitchMultiplier_ = exp2f(onset * config_.depthCents * lfo / kCentsPerOctave);
  amplitudeMultiplier_ = 1.0f - onset * (config_.tremoloPercent / kPercentScale) * (0.5f + 0.5f * lfo);
}
