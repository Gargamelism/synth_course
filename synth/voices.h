#pragma once

#include <stdint.h>
#include "oscillator.h" // HarmonicSpread
#include "distortion.h" // DistortionType
#include "scale.h"      // ScaleMode

// The instrument, in one table. Edit kPatches below to change what the
// synth plays: one row per patch — a voice table (each with its own pitch
// offset, harmonic spread and relative level), a pitch mode, and a matched
// distortion flavor. The encoder (pins.h: PIN_ENCODER_*) cycles kPatches at
// runtime; its click toggles the active patch's matched distortion on/off.
// Everything else (wavetable allocation, mixer headroom, the OLED's summary
// rows) derives from this table.

// How a patch's voices are pitched relative to the pitch pot.
enum PitchMode
{
  PITCH_DIATONIC,      // scale-degree offsets from the pot's root (scale.h)
  PITCH_UNISON_DETUNE, // fixed cents offsets from the pot's root frequency
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
    {2, 0, SPREAD_SQUARE, 80},
    {4, 0, SPREAD_VIOLA, 70},
};

static constexpr VoiceConfig kVoicesMetal[] = {
    // degree, cents, spread,         level
    {0, 0, SPREAD_GUITAR, 100},
    // scaleDegree 7 == detuneCents 700 (a fifth) once read as chromatic
    // degrees (METL3, below); PITCH_UNISON_DETUNE (METL1/METL2) ignores
    // scaleDegree entirely, so this doesn't change their sound.
    {7, 0, SPREAD_GUITAR, 80}};

// One-voice audition tables for the instrument-model spreads
// (harmonic_spreads.h); each costs 18 KB of wavetables at boot.
static constexpr VoiceConfig kVoicesGuitar[] = {{0, 0, SPREAD_GUITAR, 100}};
static constexpr VoiceConfig kVoicesGuitar2[] = {{0, 0, SPREAD_GUITAR, 100},
                                                 {-2, 0, SPREAD_GUITAR, 30}};
static constexpr VoiceConfig kVoicesPiano[] = {{0, 0, SPREAD_PIANO, 100}};
static constexpr VoiceConfig kVoicesPiano2[] = {{0, 0, SPREAD_PIANO, 100},
                                                {-5, 0, SPREAD_PIANO, 30}};
static constexpr VoiceConfig kVoicesClarinet[] = {{0, 0, SPREAD_CLARINET, 100}};
static constexpr VoiceConfig kVoicesClarinet2[] = {{0, 0, SPREAD_CLARINET, 80},
                                                   {5, 0, SPREAD_CLARINET, 70}};
static constexpr VoiceConfig kVoicesFlute[] = {{0, 0, SPREAD_FLUTE, 100}};
static constexpr VoiceConfig kVoicesFlute2[] = {{0, 0, SPREAD_FLUTE, 100},
                                                {7, 0, SPREAD_FLUTE, 10},
                                                {9, 0, SPREAD_FLUTE, 50}};
static constexpr VoiceConfig kVoicesTrumpet[] = {{0, 0, SPREAD_TRUMPET, 100}};
static constexpr VoiceConfig kVoicesTrumpet2[] = {{0, 0, SPREAD_TRUMPET, 100},
                                                  {3, 0, SPREAD_TRUMPET, 40}};
static constexpr VoiceConfig kVoicesTibia[] = {{0, 0, SPREAD_HAMMOND_TIBIA, 100}};
static constexpr VoiceConfig kVoicesTibia2[] = {
    {0, 0, SPREAD_HAMMOND_TIBIA, 100},
    {2, 0, SPREAD_HAMMOND_TIBIA, 80},
    {4, 0, SPREAD_HAMMOND_TIBIA, 60},
    {6, 0, SPREAD_HAMMOND_TIBIA, 40},
};
static constexpr VoiceConfig kVoicesHammondTrumpet[] = {{0, 0, SPREAD_HAMMOND_TRUMPET, 100}};
static constexpr VoiceConfig kVoicesHammondTrumpet2[] = {
    {0, 0, SPREAD_HAMMOND_TRUMPET, 80},
    {3, 0, SPREAD_HAMMOND_TRUMPET, 60},
    {5, 0, SPREAD_HAMMOND_TRUMPET, 40},
};

static constexpr VoiceConfig kVoicesHarsh[] = {
    {0, 0, SPREAD_TRIANGLE, 80},
    {0, 10, SPREAD_PULSATING, 80},
    {0, 45, SPREAD_TRIANGLE, 50},
    {0, 55, SPREAD_TRIANGLE, 50},
    {0, 700, SPREAD_TRIANGLE, 20},
    {0, 710, SPREAD_PULSATING, 20},
};

static constexpr VoiceConfig kVoicesPulse[] = {
    {0, 0, SPREAD_TRIANGLE, 80},
    {0, 3, SPREAD_TRIANGLE, 60},
    {0, 5, SPREAD_TRIANGLE, 80},
    {0, 300, SPREAD_TRIANGLE, 60},
    {0, 303, SPREAD_TRIANGLE, 40},
    {0, 305, SPREAD_TRIANGLE, 60},
    {0, 450, SPREAD_TRIANGLE, 50},
    {0, 453, SPREAD_TRIANGLE, 30},
    {0, 455, SPREAD_TRIANGLE, 50},
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
  ScaleMode scale;           // key mode for PITCH_DIATONIC; ignored otherwise
  DistortionType distortion; // the "matched distortion" for this type
  const char *label;         // shown on the OLED
};

static constexpr Patch kPatches[] = {
    {kVoicesHarmony, sizeof(kVoicesHarmony) / sizeof(kVoicesHarmony[0]), PITCH_DIATONIC, SCALE_MINOR, DIST_SOFT_CLIP, "HARM"},
    {kVoicesMetal, sizeof(kVoicesMetal) / sizeof(kVoicesMetal[0]), PITCH_DIATONIC, SCALE_CHROMATIC, DIST_HARD_CLIP, "METL1"},
    {kVoicesMetal, sizeof(kVoicesMetal) / sizeof(kVoicesMetal[0]), PITCH_DIATONIC, SCALE_CHROMATIC, DIST_FOLDBACK, "METL2"},
    {kVoicesGuitar, sizeof(kVoicesGuitar) / sizeof(kVoicesGuitar[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "GTR"},
    {kVoicesGuitar2, sizeof(kVoicesGuitar2) / sizeof(kVoicesGuitar2[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_NONE, "GTR2"},
    {kVoicesPiano, sizeof(kVoicesPiano) / sizeof(kVoicesPiano[0]), PITCH_UNISON_DETUNE, SCALE_NOT_WORKING, DIST_SOFT_CLIP, "PNO"},
    {kVoicesPiano2, sizeof(kVoicesPiano2) / sizeof(kVoicesPiano2[0]), PITCH_DIATONIC, SCALE_MINOR, DIST_NONE, "PNO2"},
    {kVoicesClarinet, sizeof(kVoicesClarinet) / sizeof(kVoicesClarinet[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "CLR"},
    {kVoicesClarinet2, sizeof(kVoicesClarinet2) / sizeof(kVoicesClarinet2[0]), PITCH_DIATONIC, SCALE_MAJOR, DIST_SOFT_CLIP, "CLR2"},
    {kVoicesFlute, sizeof(kVoicesFlute) / sizeof(kVoicesFlute[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "FLT"},
    {kVoicesFlute2, sizeof(kVoicesFlute2) / sizeof(kVoicesFlute2[0]), PITCH_DIATONIC, SCALE_PENTATONIC_MINOR, DIST_SOFT_CLIP, "FLT2"},
    {kVoicesTrumpet, sizeof(kVoicesTrumpet) / sizeof(kVoicesTrumpet[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "TPT"},
    {kVoicesTrumpet2, sizeof(kVoicesTrumpet2) / sizeof(kVoicesTrumpet2[0]), PITCH_DIATONIC, SCALE_PENTATONIC_MINOR, DIST_SOFT_CLIP, "TPT2"},
    {kVoicesTibia, sizeof(kVoicesTibia) / sizeof(kVoicesTibia[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "TIB"},
    {kVoicesTibia2, sizeof(kVoicesTibia2) / sizeof(kVoicesTibia2[0]), PITCH_DIATONIC, SCALE_MINOR_MELODIC, DIST_SOFT_CLIP, "TIB2"},
    {kVoicesHammondTrumpet, sizeof(kVoicesHammondTrumpet) / sizeof(kVoicesHammondTrumpet[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "HTP"},
    {kVoicesHammondTrumpet2, sizeof(kVoicesHammondTrumpet2) / sizeof(kVoicesHammondTrumpet2[0]), PITCH_DIATONIC, SCALE_MINOR_HARMONIC, DIST_SOFT_CLIP, "HTP2"},
    {kVoicesHarsh, sizeof(kVoicesHarsh) / sizeof(kVoicesHarsh[0]), PITCH_UNISON_DETUNE, SCALE_NOT_WORKING, DIST_SOFT_CLIP, "HARS"},
    {kVoicesPulse, sizeof(kVoicesPulse) / sizeof(kVoicesPulse[0]), PITCH_UNISON_DETUNE, SCALE_NOT_WORKING, DIST_SOFT_CLIP, "PULS"},
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
