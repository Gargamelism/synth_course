#include "scale.h"
#include "notes.h"
#include <math.h>

static const int kSemitonesPerOctave = 12;

// One named array per mode, so degreesPerOctave (below) is always derived
// from the actual data via sizeof — never a hand-typed count that can drift
// out of sync when a row is edited. SCALE_PENTATONIC_MINOR is genuinely only
// 5 notes but pads out to 7 slots by repeating its first two degrees an
// octave up (12, 15), matching every diatonic mode's width.
static const int kMajorSemitones[]           = {0, 2, 4, 5, 7, 9, 11};
static const int kMinorSemitones[]           = {0, 2, 3, 5, 7, 8, 10};
static const int kMinorMelodicSemitones[]    = {0, 2, 3, 5, 7, 9, 11};
static const int kMinorHarmonicSemitones[]   = {0, 2, 3, 5, 7, 8, 11};
static const int kPhrygianSemitones[]        = {0, 1, 3, 5, 7, 8, 10};
static const int kPentatonicMinorSemitones[] = {0, 3, 5, 7, 10, 12, 15};
static const int kChromaticSemitones[]       = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const int kNotWorkingSemitones[]      = {0};  // never read; see scale.h

struct ScaleDef {
  const int *semitones;
  int degreesPerOctave;
};

#define SCALE_DEF(array) {array, (int)(sizeof(array) / sizeof(array[0]))}

// One row per ScaleMode, in enum order — a row count mismatch is a compile
// error below rather than a silently zero-filled mode.
static const ScaleDef kScales[SCALE_MODE_COUNT] = {
    SCALE_DEF(kMajorSemitones),           // SCALE_MAJOR
    SCALE_DEF(kMinorSemitones),           // SCALE_MINOR
    SCALE_DEF(kMinorMelodicSemitones),    // SCALE_MINOR_MELODIC
    SCALE_DEF(kMinorHarmonicSemitones),   // SCALE_MINOR_HARMONIC
    SCALE_DEF(kPhrygianSemitones),        // SCALE_MODE_PHRYGIAN
    SCALE_DEF(kPentatonicMinorSemitones), // SCALE_PENTATONIC_MINOR
    SCALE_DEF(kChromaticSemitones),       // SCALE_CHROMATIC
    SCALE_DEF(kNotWorkingSemitones),      // SCALE_NOT_WORKING
};
static_assert(sizeof(kScales) / sizeof(kScales[0]) == SCALE_MODE_COUNT,
              "kScales must have exactly one row per ScaleMode, in enum order");

// Floor division / modulo, so negative degrees walk down into the octave
// below instead of folding back on themselves (C's / and % truncate toward
// zero, which would map degree -1 to degree 0's semitone).
static int floorDiv(int a, int b) {
  int q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

int scaleDegreeToSemitone(int degree, ScaleMode mode) {
  const ScaleDef &def = kScales[mode];
  const int octave = floorDiv(degree, def.degreesPerOctave);
  const int index = degree - octave * def.degreesPerOctave;
  return octave * kSemitonesPerOctave + def.semitones[index];
}

int snapMidiToScaleDegree(float midi, ScaleMode mode) {
  // Search the degree either side of the unsnapped position rather than the
  // whole range: the nearest in-key degree is always within one degree of
  // round(relative semitones * 7/12).
  const float relative = midi - (float)SCALE_KEY_ROOT_MIDI;
  const int guess = (int)lroundf(relative * kScales[mode].degreesPerOctave / (float)kSemitonesPerOctave);

  int best = guess;
  float bestDistance = -1.0f;
  for (int degree = guess - 1; degree <= guess + 1; degree++) {
    const float distance = fabsf((float)scaleDegreeToSemitone(degree, mode) - relative);
    if (bestDistance < 0.0f || distance < bestDistance) {
      bestDistance = distance;
      best = degree;
    }
  }
  return best;
}

float scaleDegreeToFreq(int degree, ScaleMode mode) {
  return midiToFreq((float)(SCALE_KEY_ROOT_MIDI + scaleDegreeToSemitone(degree, mode)));
}
