#pragma once

#include <stdint.h>

// Per-patch vibrato (pitch) and tremolo (amplitude) from one sine LFO,
// advanced once per audio block. Each note starts plain for delayMs, then
// the depth fades in linearly over onsetMs — a delayed onset is what reads
// as "played" rather than "modulated". One instance serves the whole patch, like Envelope.
//
// Kept free of Arduino headers so test/run.sh can build it on the host.
struct VibratoConfig {
  float rateHz;
  uint8_t depthCents;     // peak pitch deviation, +/- cents
  uint16_t delayMs;       // depth held at 0 this long after note-on
  uint16_t onsetMs;       // then ramps 0 -> full over this long
  uint8_t tremoloPercent; // peak amplitude dip, 0-100
};

class Vibrato {
public:
  // Swaps the shape in place — keeps phase and onset timer, so a patch
  // switch mid-note doesn't restart the vibrato.
  void configure(const VibratoConfig &config) { config_ = config; }

  // Restarts the onset fade and the LFO phase, so every note begins plain.
  void noteOn();

  // Advances by one block of `frames` samples and updates the multipliers.
  void advanceBlock(int frames);

  // Multiply each voice's frequency by this (1.0 when depthCents == 0).
  float pitchMultiplier() const { return pitchMultiplier_; }

  // Multiply the amp envelope's gain by this (0..1; 1.0 with no tremolo).
  float amplitudeMultiplier() const { return amplitudeMultiplier_; }

private:
  VibratoConfig config_ = {0.0f, 0, 0, 0, 0};
  float phase_ = 0.0f;           // LFO phase in cycles, 0..1
  float msSinceNoteOn_ = 0.0f;
  float pitchMultiplier_ = 1.0f;
  float amplitudeMultiplier_ = 1.0f;
};
