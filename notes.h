#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h> // snprintf

static const char *const kNoteNames[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Writes the nearest note name + octave for freqHz into buf (e.g. "A4").
// buf must be at least NOTE_NAME_BUF_SIZE bytes: 2-char name (e.g. "C#") +
// 1-digit octave (this project's pitch range only ever yields octaves ~2-5) + '\0'.
const int NOTE_NAME_BUF_SIZE = 5;
inline void freqToNoteName(float freqHz, char *buf) {
  int note = (int)roundf(12.0f * log2f(freqHz / 440.0f) + 69.0f);
  int nameIndex = ((note % 12) + 12) % 12;
  int octave = note / 12 - 1;
  snprintf(buf, NOTE_NAME_BUF_SIZE, "%s%d", kNoteNames[nameIndex], octave);
}
