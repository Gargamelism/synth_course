#include "scale.h"
#include "notes.h"
#include <math.h>

static const int kScaleSemitones[SCALE_MODE_COUNT][SCALE_DEGREES_PER_OCTAVE] = {
    /* SCALE_MAJOR          */ {0, 2, 4, 5, 7, 9, 11},
    /* SCALE_MINOR          */ {0, 2, 3, 5, 7, 8, 10},
    /* SCALE_MINOR_MELODIC  */ {0, 2, 3, 5, 7, 9, 11},
    /* SCALE_MINOR_HARMONIC */ {0, 2, 3, 5, 7, 8, 11},
    /* SCALE_MODE_PHRYGIAN  */ {0, 1, 3, 5, 7, 8, 10},
    /* SCALE_PENTATONIC_MINOR*/ {0, 3, 5, 7, 10, 12, 15},
};

// Floor division / modulo, so negative degrees walk down into the octave
// below instead of folding back on themselves (C's / and % truncate toward
// zero, which would map degree -1 to degree 0's semitone).
static int floorDiv(int a, int b) {
  int q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

int scaleDegreeToSemitone(int degree, ScaleMode mode) {
  const int octave = floorDiv(degree, SCALE_DEGREES_PER_OCTAVE);
  const int index = degree - octave * SCALE_DEGREES_PER_OCTAVE;
  return octave * 12 + kScaleSemitones[mode][index];
}

int snapMidiToScaleDegree(float midi, ScaleMode mode) {
  // Search the degree either side of the unsnapped position rather than the
  // whole range: the nearest in-key degree is always within one degree of
  // round(relative semitones * 7/12).
  const float relative = midi - (float)SCALE_KEY_ROOT_MIDI;
  const int guess = (int)lroundf(relative * SCALE_DEGREES_PER_OCTAVE / 12.0f);

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
