#pragma once

#include <stdint.h>
#include "oscillator.h" // HarmonicSpread

// The instrument, in one table. Edit kVoices below to change what the synth
// plays: one row per voice, each with its own pitch offset, harmonic spread
// and relative level. Everything else (wavetable allocation, mixer headroom,
// the OLED's summary rows) derives from this table.

// How the voices are pitched relative to the pitch pot. Pick exactly ONE,
// the same way distortion.h picks a distortion flavor — only the selected
// path is compiled.
#define VOICE_PITCH_DIATONIC
// #define VOICE_PITCH_UNISON_DETUNE

#if defined(VOICE_PITCH_DIATONIC) == defined(VOICE_PITCH_UNISON_DETUNE)
#error "Define exactly one of VOICE_PITCH_DIATONIC / VOICE_PITCH_UNISON_DETUNE"
#endif

struct VoiceConfig {
  int8_t  scaleDegree; // VOICE_PITCH_DIATONIC: offset in SCALE DEGREES from
                       // the pot's root (see scale.h). 0 = the root itself,
                       // 2 = a third above it, 4 = a fifth, 7 = an octave.
  int16_t detuneCents; // VOICE_PITCH_UNISON_DETUNE: offset from the root in cents
  uint8_t spread;      // HarmonicSpread — the per-voice harmonics knob
  uint8_t level;       // relative loudness, normalized at startup so the
                       // full mix always fits (see normalizeVoiceLevelsQ15)
};

// A voiced triad plus the octave above it: the root bowed, the third and
// fifth thinner, the octave sparse and quiet on top.
static constexpr VoiceConfig kVoices[] = {
  // degree, cents, spread,         level
  {  0,        0,   SPREAD_VIOLA,   100},
  {  2,       -7,   SPREAD_NATURAL,  70},
  {  4,        7,   SPREAD_ODD,      60},
  {  7,       12,   SPREAD_VIOLA,    40},
};

// Hard ceiling on voice count, sized against the audio budget: at ~13
// cycles/voice, 32 voices is ~13% of the 3628 cycles/frame available at
// 44.1 kHz on the 160 MHz C3 (~26% with pre-mix distortion).
#define MAX_VOICES 32
#define NUM_VOICES ((int)(sizeof(kVoices) / sizeof(kVoices[0])))

static_assert(NUM_VOICES >= 1, "kVoices must have at least one voice");
static_assert(NUM_VOICES <= MAX_VOICES, "kVoices exceeds MAX_VOICES");

// Compile-time checks and summaries over the table. All constexpr, so a bad
// spread is a build error and the display's "spreads differ" marker costs
// nothing at runtime.
static constexpr bool voiceSpreadsInRange() {
  for (int i = 0; i < NUM_VOICES; i++) {
    if (kVoices[i].spread >= SPREAD_COUNT) return false;
  }
  return true;
}
static_assert(voiceSpreadsInRange(), "a kVoices row has an unknown HarmonicSpread");

static constexpr bool voiceSpreadsMixed() {
  for (int i = 1; i < NUM_VOICES; i++) {
    if (kVoices[i].spread != kVoices[0].spread) return true;
  }
  return false;
}
