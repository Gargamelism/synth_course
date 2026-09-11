#pragma once

#include <stdint.h>
#include "oscillator.h"  // HarmonicSpread
#include "distortion.h"  // DistortionType

// The instrument, in one table. Edit kPatches below to change what the
// synth plays: one row per patch — a voice table (each with its own pitch
// offset, harmonic spread and relative level), a pitch mode, and a matched
// distortion flavor. The encoder (pins.h: PIN_ENCODER_*) cycles kPatches at
// runtime; its click toggles the active patch's matched distortion on/off.
// Everything else (wavetable allocation, mixer headroom, the OLED's summary
// rows) derives from this table.

// How a patch's voices are pitched relative to the pitch pot.
enum PitchMode {
  PITCH_DIATONIC,       // scale-degree offsets from the pot's root (scale.h)
  PITCH_UNISON_DETUNE,  // fixed cents offsets from the pot's root frequency
};

struct VoiceConfig
{
  int8_t scaleDegree;  // PITCH_DIATONIC: offset in SCALE DEGREES from
                       // the pot's root (see scale.h). 0 = the root itself,
                       // 2 = a third above it, 4 = a fifth, 7 = an octave.
  int16_t detuneCents; // PITCH_UNISON_DETUNE: offset from the root in cents
  uint8_t spread;      // HarmonicSpread — the per-voice harmonics knob
  uint8_t level;       // relative loudness, normalized at startup so the
                       // full mix always fits (see normalizeVoiceLevelsQ15)
};

static constexpr VoiceConfig kVoicesHarmony[] = {
    // degree, cents, spread,         level
    {0, 0, SPREAD_VIOLA, 100},
    {2, 0, SPREAD_ODD, 90},
    {4, 0, SPREAD_VIOLA, 80},
    {6, 0, SPREAD_VIOLA, 70},
    {7, 0, SPREAD_VIOLA, 60},
    {9, 0, SPREAD_NATURAL, 70},
    {11, 0, SPREAD_ODD, 40},
    {13, 0, SPREAD_VIOLA, 30},
};

static constexpr VoiceConfig kVoicesMetal[] = {
    // degree, cents, spread,         level
    {0, 0, SPREAD_VIOLA, 100},
    {4, 0, SPREAD_ODD, 100}
};

static constexpr VoiceConfig kVoicesSolo[] = {
    // degree, cents, spread,         level
    {0, 0, SPREAD_BASS_GUITAR, 100},
    // {7, -7, SPREAD_ODD, 70},
    // {12, 7, SPREAD_VIOLA, 50},
    // {14, 12, SPREAD_VIOLA, 70},
    // {16, 0, SPREAD_VIOLA, 40},
    // {18, -7, SPREAD_NATURAL, 16},
    // {19, 7, SPREAD_ODD, 18}
    // {20, 12, SPREAD_VIOLA, 3},
    // {21, -4, SPREAD_VIOLA, 5},
    // {22, 5, SPREAD_VIOLA, 7},
    // {23, 0, SPREAD_VIOLA, 2},
    // {24, -6, SPREAD_VIOLA, 1},
    // {25, 8, SPREAD_VIOLA, 5},
    // {26, 12, SPREAD_VIOLA, 3},
    // {27, -3, SPREAD_VIOLA, 6},
    // {28, -50, SPREAD_VIOLA, 1},
    // {29, -15, SPREAD_VIOLA, 16},
    // {30, 12, SPREAD_VIOLA, 4},
    // {31, 7, SPREAD_VIOLA, 3},
    // {32, 3, SPREAD_VIOLA, 2},
    // {33, 15, SPREAD_VIOLA, 6},
    // {34, 30, SPREAD_VIOLA, 3},
    // {35, 50, SPREAD_VIOLA, 1},
    // {36, 100, SPREAD_VIOLA, 7},
    // {37, 50, SPREAD_VIOLA, 1},
    // {38, 0, SPREAD_VIOLA, 2},
    // {39, -50, SPREAD_VIOLA, 1},
    // {40, -30, SPREAD_VIOLA, 3},
    // {41, -20, SPREAD_VIOLA, 4},
    // {42, -10, SPREAD_VIOLA, 1},
    // {43, 12, SPREAD_VIOLA, 3},
};

// A "type of audio": a voice table, how it's pitched, and the distortion
// flavor matched to it. detuneCents is 0 across kVoicesHarmony/kVoicesMetal,
// so PITCH_UNISON_DETUNE only does something audible when a table sets it —
// pitch mode has to travel with the voice table, not be picked separately.
struct Patch
{
  const VoiceConfig *voices;
  uint8_t voiceCount;
  PitchMode pitchMode;
  DistortionType distortion; // the "matched distortion" for this type
  const char *label;         // shown on the OLED
};

static constexpr Patch kPatches[] = {
    {kVoicesSolo, sizeof(kVoicesSolo) / sizeof(kVoicesSolo[0]), PITCH_DIATONIC, DIST_NONE, "SOLO"},
    {kVoicesHarmony, sizeof(kVoicesHarmony) / sizeof(kVoicesHarmony[0]), PITCH_DIATONIC, DIST_SOFT_CLIP, "HARM"},
    {kVoicesMetal, sizeof(kVoicesMetal) / sizeof(kVoicesMetal[0]), PITCH_UNISON_DETUNE, DIST_HARD_CLIP, "METL"},
};
#define NUM_PATCHES ((int)(sizeof(kPatches) / sizeof(kPatches[0])))

// Hard ceiling on any one patch's voice count, sized against the audio
// budget: at ~13 cycles/voice, 32 voices is ~13% of the 3628 cycles/frame
// available at 44.1 kHz on the 160 MHz C3 (~26% with pre-mix distortion).
#define MAX_VOICES 32

static_assert(NUM_PATCHES >= 1, "kPatches must have at least one patch");

// Compile-time checks over the table. All constexpr, so a bad spread or an
// oversized patch is a build error and the display's "spreads differ"
// marker costs nothing at runtime.
static constexpr bool patchVoiceCountInRange(const Patch &patch)
{
  return patch.voiceCount >= 1 && patch.voiceCount <= MAX_VOICES;
}
static constexpr bool allPatchVoiceCountsInRange()
{
  for (int p = 0; p < NUM_PATCHES; p++)
  {
    if (!patchVoiceCountInRange(kPatches[p]))
      return false;
  }
  return true;
}
static_assert(allPatchVoiceCountsInRange(), "a kPatches row's voice count is out of [1, MAX_VOICES]");

static constexpr bool patchSpreadsInRange(const Patch &patch)
{
  for (int i = 0; i < patch.voiceCount; i++)
  {
    if (patch.voices[i].spread >= SPREAD_COUNT)
      return false;
  }
  return true;
}
static constexpr bool allPatchSpreadsInRange()
{
  for (int p = 0; p < NUM_PATCHES; p++)
  {
    if (!patchSpreadsInRange(kPatches[p]))
      return false;
  }
  return true;
}
static_assert(allPatchSpreadsInRange(), "a kPatches row has an unknown HarmonicSpread");

// Whether `patch`'s voices don't all share voice 0's spread — the OLED's
// "*" marker. Not compile-time folded (the active patch is a runtime
// choice), but cheap: at most MAX_VOICES iterations, called from the 10 Hz
// display refresh.
static constexpr bool patchSpreadsMixed(const Patch &patch)
{
  for (int i = 1; i < patch.voiceCount; i++)
  {
    if (patch.voices[i].spread != patch.voices[0].spread)
      return true;
  }
  return false;
}
