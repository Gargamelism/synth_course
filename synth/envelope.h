#pragma once

#include <stdint.h>

// Per-patch amplitude envelope: attack ramps linearly to full scale, decay
// and release ease exponentially toward their targets (sustain level and
// zero respectively). This runs at block rate (one multiply-ish update per
// audio block); audio_task.cpp ramps every sample in between the block-rate
// targets, so the block granularity never produces a zipper/click. One
// instance serves an entire patch's voices, since they all trigger together
// (see voices.h: Patch::amp).
//
// Kept free of Arduino headers so test/run.sh can build it on the host.
struct EnvelopeConfig {
  uint16_t attackMs;
  uint16_t decayMs;
  uint16_t releaseMs;
  uint8_t sustainPercent;    // 0-100
  uint16_t sustainLengthMs = 0; // how long to hold sustain before
                                 // auto-releasing, as if noteOff() had been
                                 // called; 0 = sustain indefinitely (until an
                                 // actual noteOff()) — today's behavior, and
                                 // every EnvelopeConfig that predates this
                                 // field gets it for free (aggregate init
                                 // leaves a trailing member at its default)
};

class Envelope {
public:
  // Swaps the envelope shape in place — does not reset state_/gain_, so
  // switching patches mid-note doesn't force a re-trigger.
  void configure(const EnvelopeConfig &config);

  // Rising edge: (re)starts the attack phase toward full scale from
  // whatever gain the envelope is currently at (not from 0), so a re-pluck
  // mid-decay/release doesn't produce an audible dip.
  void noteOn();

  // Starts the release phase toward silence from the current gain.
  void noteOff();

  // Advances the envelope by one block of `frames` samples and returns the
  // gain to reach by the END of this block, in Q15 (0 .. VOLUME_Q15_ONE).
  int16_t nextBlockGainQ15(int frames);

  // True once released to silence, or never triggered — no sound coming.
  bool idle() const { return state_ == STATE_IDLE; }

private:
  enum State { STATE_IDLE, STATE_ATTACK, STATE_DECAY, STATE_SUSTAIN, STATE_RELEASE };

  // gain_ has just reached 1.0 (attack's target) — moves on to decay, or
  // straight to sustain if this config has no decay stage.
  void enterDecayOrSustain();

  // Entry point for STATE_SUSTAIN from either of the above — resets the
  // sustainLengthMs timer, so a re-trigger always gets the full hold time.
  void enterSustain();

  // One of these runs per nextBlockGainQ15() call, chosen by state_ — each
  // advances gain_ (and state_, where a stage completes) by one block.
  // Defined inline in envelope.cpp: called only from nextBlockGainQ15() in
  // that same file, at block rate, so there's no cross-TU inlining to lose.
  void advanceIdle();
  void advanceAttack(float blockMs);
  void advanceDecay(float blockMs);
  void advanceSustain(float blockMs);
  void advanceRelease(float blockMs);

  EnvelopeConfig config_ = {0, 0, 0, 100};
  State state_ = STATE_IDLE;
  float gain_ = 0.0f; // 0..1
  float sustainElapsedMs_ = 0.0f; // time in STATE_SUSTAIN since enterSustain()
};
