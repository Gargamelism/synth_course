#include "../scale.h"
#include "../notes.h"
#include "framework.h"

// A triad is degrees {0, +2, +4} from the root — the same offsets voices.h
// uses. Quality is never stored: it falls out of where the root sits in the
// key, so the same three offsets give major on C, minor on D and diminished
// on B. Semitones are relative to SCALE_KEY_ROOT_MIDI (C4).
#define CHECK_TRIAD(rootDegree, s0, s1, s2)                                    \
  do {                                                                         \
    CHECK(scaleDegreeToSemitone((rootDegree) + 0, SCALE_MAJOR) == (s0));       \
    CHECK(scaleDegreeToSemitone((rootDegree) + 2, SCALE_MAJOR) == (s1));       \
    CHECK(scaleDegreeToSemitone((rootDegree) + 4, SCALE_MAJOR) == (s2));       \
  } while (0)

static void runTests() {
  // C E G (major), D F A (minor), E G B (minor), F A C, G B D, A C E, B D F
  // (diminished) — the fixture confirmed when this was planned.
  CHECK_TRIAD(0, 0, 4, 7);
  CHECK_TRIAD(1, 2, 5, 9);
  CHECK_TRIAD(2, 4, 7, 11);
  CHECK_TRIAD(3, 5, 9, 12);
  CHECK_TRIAD(4, 7, 11, 14);
  CHECK_TRIAD(5, 9, 12, 16);
  CHECK_TRIAD(6, 11, 14, 17);

  // Degrees wrap into the octave above and below; negative degrees must
  // floor-divide rather than fold back onto degree 0.
  CHECK(scaleDegreeToSemitone(7, SCALE_MAJOR) == 12);
  CHECK(scaleDegreeToSemitone(-1, SCALE_MAJOR) == -1);  // the B below the key root
  CHECK(scaleDegreeToSemitone(-7, SCALE_MAJOR) == -12);
  CHECK(scaleDegreeToSemitone(-8, SCALE_MAJOR) == -13);

  // Snapping the pot's continuous note onto the key.
  CHECK(snapMidiToScaleDegree(60.0f, SCALE_MAJOR) == 0);   // C4, the key root
  CHECK(snapMidiToScaleDegree(62.0f, SCALE_MAJOR) == 1);   // D4
  CHECK(snapMidiToScaleDegree(60.9f, SCALE_MAJOR) == 0);   // between C and D, nearer C
  CHECK(snapMidiToScaleDegree(61.4f, SCALE_MAJOR) == 1);   // ... nearer D
  CHECK(snapMidiToScaleDegree(69.0f, SCALE_MAJOR) == 5);   // A4
  CHECK(snapMidiToScaleDegree(72.0f, SCALE_MAJOR) == 7);   // C5, an octave up
  CHECK(snapMidiToScaleDegree(59.0f, SCALE_MAJOR) == -1);  // B3, below the key root

  // Out-of-key notes snap to the nearest degree in the key; an exact tie
  // (F#4 sits a semitone from both F4 and G4) resolves downward.
  CHECK(snapMidiToScaleDegree(66.0f, SCALE_MAJOR) == 3);   // F#4 -> F4

  // Degrees resolve to real frequencies (C4 = 261.63 Hz, A4 = 440 Hz).
  CHECK_NEAR(scaleDegreeToFreq(0, SCALE_MAJOR), 261.626f, 0.01f);
  CHECK_NEAR(scaleDegreeToFreq(5, SCALE_MAJOR), 440.0f, 0.01f);
  CHECK_NEAR(scaleDegreeToFreq(7, SCALE_MAJOR), 523.251f, 0.01f);

  // The pot's own mapping feeds snapMidiToScaleDegree through freqToMidi,
  // so the round trip has to land back on the same note.
  CHECK_NEAR(freqToMidi(440.0f), 69.0f, 0.001f);
  CHECK_NEAR(midiToFreq(69.0f), 440.0f, 0.001f);
  CHECK(snapMidiToScaleDegree(freqToMidi(scaleDegreeToFreq(3, SCALE_MAJOR)), SCALE_MAJOR) == 3);
}

TEST_MAIN()
