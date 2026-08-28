#include "../notes.h"
#include "framework.h"
#include <cstring>

#define CHECK_NOTE(freqHz, expected)               \
  do {                                             \
    char buf[NOTE_NAME_BUF_SIZE];                   \
    freqToNoteName((freqHz), buf);                  \
    CHECK(std::strcmp(buf, (expected)) == 0);       \
  } while (0)

static void runTests() {
  // Concert A and its octaves — exact semitone integers.
  CHECK_NOTE(440.0f, "A4");
  CHECK_NOTE(220.0f, "A3"); // OSC3_FIXED_FREQ_HZ
  CHECK_NOTE(880.0f, "A5");

  // Middle C, and a sharp name pulled from kNoteNames.
  CHECK_NOTE(262.0f, "C4");
  CHECK_NOTE(470.0f, "A#4");

  // Nearest-note rounding across the A4/A#4 boundary (~452.9 Hz).
  CHECK_NOTE(450.0f, "A4");
  CHECK_NOTE(456.0f, "A#4");

  // Project pitch-sweep endpoints (FREQ_MIN_HZ .. FREQ_MAX_HZ).
  CHECK_NOTE(80.0f, "D#2");
  CHECK_NOTE(1000.0f, "B5");
}

TEST_MAIN()
