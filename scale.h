#pragma once

// Diatonic scale math for VOICE_PITCH_DIATONIC (see voices.h): the pitch pot
// picks a root that snaps to the key, and each voice adds a compile-time
// offset in SCALE DEGREES. Chord quality therefore falls out of where the
// root sits in the key (in C major, root C -> C E G major, root D -> D F A
// minor, root B -> B D F diminished) and is never stored anywhere.
//
// Kept free of Arduino headers so test/run.sh can build it on the host.

// The key, fixed at compile time. Pick exactly ONE mode.
#define SCALE_KEY_ROOT_MIDI 40 // E2
// #define SCALE_MAJOR
#define SCALE_MINOR

const int SCALE_DEGREES_PER_OCTAVE = 7;

// Semitone offset of `degree` from SCALE_KEY_ROOT_MIDI. Degrees run in both
// directions and past an octave: 7 -> +12, -1 -> -1 (the B below C4).
int scaleDegreeToSemitone(int degree);

// Nearest in-key scale degree to a (possibly fractional) MIDI note — this is
// what snaps the pitch pot's continuous sweep onto the key.
int snapMidiToScaleDegree(float midi);

// Frequency of a scale degree, in Hz.
float scaleDegreeToFreq(int degree);
