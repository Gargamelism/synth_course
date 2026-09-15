#include "envelope.h"
#include "pins.h" // SAMPLE_RATE_HZ, VOLUME_Q15_ONE
#include <math.h>

// Below this, snap exactly to the target rather than asymptoting forever —
// the gap is already under a Q15 LSB, so nothing audible is lost.
static const float kGainSnapEpsilon = 0.0005f;

static const float kMsPerSecond = 1000.0f;

void Envelope::configure(const EnvelopeConfig &config) {
  config_ = config;
}

void Envelope::enterDecayOrSustain() {
  if (config_.decayMs == 0) {
    gain_ = config_.sustainPercent / 100.0f;
    enterSustain();
  } else {
    state_ = STATE_DECAY;
  }
}

void Envelope::enterSustain() {
  state_ = STATE_SUSTAIN;
  sustainElapsedMs_ = 0.0f;
}

void Envelope::noteOn() {
  if (config_.attackMs == 0) {
    gain_ = 1.0f;
    enterDecayOrSustain();
  } else {
    state_ = STATE_ATTACK;
  }
}

void Envelope::noteOff() {
  state_ = STATE_RELEASE;
}

inline void Envelope::advanceIdle() {
  gain_ = 0.0f;
}

inline void Envelope::advanceAttack(float blockMs) {
  gain_ += blockMs / config_.attackMs;
  if (gain_ >= 1.0f) {
    gain_ = 1.0f;
    enterDecayOrSustain();
  }
}

inline void Envelope::advanceDecay(float blockMs) {
  const float target = config_.sustainPercent / 100.0f;
  const float coeff = expf(-blockMs / config_.decayMs);
  gain_ = target + (gain_ - target) * coeff;
  if (fabsf(gain_ - target) < kGainSnapEpsilon) {
    gain_ = target;
    enterSustain();
  }
}

inline void Envelope::advanceSustain(float blockMs) {
  gain_ = config_.sustainPercent / 100.0f;
  if (config_.sustainLengthMs == 0) {
    return; // 0 = hold indefinitely, until an actual noteOff()
  }
  sustainElapsedMs_ += blockMs;
  if (sustainElapsedMs_ >= config_.sustainLengthMs) {
    state_ = STATE_RELEASE; // auto-release, as if noteOff() had been called
  }
}

inline void Envelope::advanceRelease(float blockMs) {
  if (config_.releaseMs == 0) {
    gain_ = 0.0f;
  } else {
    gain_ *= expf(-blockMs / config_.releaseMs);
  }
  if (gain_ < kGainSnapEpsilon) {
    gain_ = 0.0f;
    state_ = STATE_IDLE;
  }
}

int16_t Envelope::nextBlockGainQ15(int frames) {
  const float blockMs = kMsPerSecond * frames / SAMPLE_RATE_HZ;

  switch (state_) {
    case STATE_IDLE:    advanceIdle();           break;
    case STATE_ATTACK:  advanceAttack(blockMs);  break;
    case STATE_DECAY:   advanceDecay(blockMs);   break;
    case STATE_SUSTAIN: advanceSustain(blockMs); break;
    case STATE_RELEASE: advanceRelease(blockMs); break;
  }

  const float clamped = gain_ < 0.0f ? 0.0f : (gain_ > 1.0f ? 1.0f : gain_);
  return (int16_t)(clamped * VOLUME_Q15_ONE + 0.5f);
}
