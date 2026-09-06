#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h> // snprintf

static const char *const kNoteNames[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// MIDI note number <-> frequency, the single copy of the equal-temperament
// formula (A4 = MIDI 69 = 440 Hz). scale.cpp builds on these too, so the
// conversion lives in exactly one place.
inline float freqToMidi(float freqHz) {
  return 12.0f * log2f(freqHz / 440.0f) + 69.0f;
}

inline float midiToFreq(float midi) {
  return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

// Writes the nearest note name + octave for freqHz into buf (e.g. "A4").
// buf must be at least NOTE_NAME_BUF_SIZE bytes: 2-char name (e.g. "C#") +
// 1-digit octave (this project's pitch range only ever yields octaves ~2-5) + '\0'.
const int NOTE_NAME_BUF_SIZE = 5;
inline void freqToNoteName(float freqHz, char *buf) {
  int note = (int)roundf(freqToMidi(freqHz));
  int nameIndex = ((note % 12) + 12) % 12;
  int octave = note / 12 - 1;
  snprintf(buf, NOTE_NAME_BUF_SIZE, "%s%d", kNoteNames[nameIndex], octave);
}
