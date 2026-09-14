#pragma once

// Diatonic scale math for PITCH_DIATONIC (see voices.h): the pitch pot
// picks a root that snaps to the key, and each voice adds a compile-time
// offset in SCALE DEGREES. Chord quality therefore falls out of where the
// root sits in the key (in C major, root C -> C E G major, root D -> D F A
// minor, root B -> B D F diminished) and is never stored anywhere.
//
// Kept free of Arduino headers so test/run.sh can build it on the host.

// The key root, fixed at compile time and shared by every patch.
#define SCALE_KEY_ROOT_MIDI 40 // E2

// A semitone is 1/12 of an octave; a cent is 1/100 of a semitone. Shared so
// every caller converting a semitone or cents offset into a frequency ratio
// (scale.cpp, oscillator.cpp's mip-level sizing, controls.cpp's unison
// detune) uses the same definition instead of retyping 12 or 1200.
const int kSemitonesPerOctave = 12;
const int kCentsPerOctave = kSemitonesPerOctave * 100;

// Which mode a patch harmonizes in — set per Patch (voices.h), not here.
enum ScaleMode {
  SCALE_MAJOR,
  SCALE_MINOR,
  SCALE_MINOR_MELODIC,
  SCALE_MINOR_HARMONIC,
  SCALE_MODE_PHRYGIAN,
  SCALE_PENTATONIC_MINOR,
  SCALE_CHROMATIC,    // all 12 semitones, evenly spaced (degree == semitone)
  SCALE_NOT_WORKING,  // PITCH_UNISON_DETUNE patches ignore Patch::scale
                      // entirely (controls.cpp never calls into scale.h for
                      // them) — assign this so the table makes that explicit
                      // instead of naming a mode that has no audible effect.
  SCALE_MODE_COUNT,
};

// Degrees per octave vary by mode (7 for the diatonic/pentatonic modes, 12
// for SCALE_CHROMATIC) — looked up internally in scale.cpp.

// Semitone offset of `degree` from SCALE_KEY_ROOT_MIDI in `mode`. Degrees run
// in both directions and past an octave: 7 -> +12, -1 -> -1 (the B below C4).
int scaleDegreeToSemitone(int degree, ScaleMode mode);

// Nearest in-key scale degree to a (possibly fractional) MIDI note — this is
// what snaps the pitch pot's continuous sweep onto the key.
int snapMidiToScaleDegree(float midi, ScaleMode mode);

// Frequency of a scale degree, in Hz.
float scaleDegreeToFreq(int degree, ScaleMode mode);
