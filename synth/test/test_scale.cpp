#include "../scale.h"
#include "../notes.h"
#include "framework.h"

// A triad is degrees {0, +2, +4} from the root — the same offsets voices.h
// uses. Quality is never stored: it falls out of where the root sits in the
// key, so the same three offsets give major on C, minor on D and diminished
// on B. Semitones are relative to SCALE_KEY_ROOT_MIDI, whatever note that is.
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

  // Snapping the pot's continuous note onto the key. All relative to the
  // key root, so these hold whatever SCALE_KEY_ROOT_MIDI is set to.
  const float root = (float)SCALE_KEY_ROOT_MIDI;
  CHECK(snapMidiToScaleDegree(root, SCALE_MAJOR) == 0);        // the key root itself
  CHECK(snapMidiToScaleDegree(root + 2.0f, SCALE_MAJOR) == 1); // a whole step up
  CHECK(snapMidiToScaleDegree(root + 0.9f, SCALE_MAJOR) == 0); // between root and +2, nearer root
  CHECK(snapMidiToScaleDegree(root + 1.4f, SCALE_MAJOR) == 1); // ... nearer +2
  CHECK(snapMidiToScaleDegree(root + 9.0f, SCALE_MAJOR) == 5); // a 6th up
  CHECK(snapMidiToScaleDegree(root + 12.0f, SCALE_MAJOR) == 7);// an octave up
  CHECK(snapMidiToScaleDegree(root - 1.0f, SCALE_MAJOR) == -1); // the leading tone below the key root

  // Out-of-key notes snap to the nearest degree in the key; an exact tie
  // (root+6 sits a semitone from both the 4th and the 5th) resolves downward.
  CHECK(snapMidiToScaleDegree(root + 6.0f, SCALE_MAJOR) == 3);

  // Degrees resolve to real frequencies, checked against the same
  // midi<->freq conversion scale.cpp itself builds on (notes.h), so this
  // pins scaleDegreeToFreq's composition of the root and the semitone table
  // rather than re-deriving a root-specific constant by hand.
  CHECK_NEAR(scaleDegreeToFreq(0, SCALE_MAJOR), midiToFreq(root), 0.01f);
  CHECK_NEAR(scaleDegreeToFreq(5, SCALE_MAJOR), midiToFreq(root + 9.0f), 0.01f);
  CHECK_NEAR(scaleDegreeToFreq(7, SCALE_MAJOR), midiToFreq(root + 12.0f), 0.01f);

  // The pot's own mapping feeds snapMidiToScaleDegree through freqToMidi,
  // so the round trip has to land back on the same note.
  CHECK_NEAR(freqToMidi(440.0f), 69.0f, 0.001f);
  CHECK_NEAR(midiToFreq(69.0f), 440.0f, 0.001f);
  CHECK(snapMidiToScaleDegree(freqToMidi(scaleDegreeToFreq(3, SCALE_MAJOR)), SCALE_MAJOR) == 3);
}

TEST_MAIN()
