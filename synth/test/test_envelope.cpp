#include "../envelope.h"
#include "../pins.h" // SAMPLE_RATE_HZ, VOLUME_Q15_ONE
#include "framework.h"

// A block size that makes each nextBlockGainQ15() call advance exactly
// 100ms, so the state-machine math below works out to round numbers.
static const int kFramesPer100Ms = SAMPLE_RATE_HZ / 10;

static void runTests() {
  // Attack reaches full scale in exactly one block when blockMs == attackMs;
  // decay eases toward the 50% sustain level; release eases to 0 and idle()
  // follows; a re-trigger mid-release resumes from the current level rather
  // than restarting from 0.
  {
    Envelope env;
    env.configure({100, 100, 120, 50}); // A100 D100 R120 S50%

    CHECK(env.idle()); // never triggered

    env.noteOn();
    int16_t gain = env.nextBlockGainQ15(kFramesPer100Ms);
    CHECK(gain == VOLUME_Q15_ONE);
    CHECK(!env.idle());

    // Several more blocks of decay should settle near the 50% sustain level.
    for (int i = 0; i < 20; i++) {
      gain = env.nextBlockGainQ15(kFramesPer100Ms);
    }
    CHECK_NEAR(gain, VOLUME_Q15_ONE / 2, VOLUME_Q15_ONE / 100); // within 1%

    env.noteOff();
    int16_t partialRelease = env.nextBlockGainQ15(kFramesPer100Ms);
    CHECK(partialRelease > 0);
    CHECK(partialRelease < gain); // released some, but not fully

    // Re-trigger mid-release: the next block should climb back up from
    // partialRelease, not reset to 0 first.
    env.noteOn();
    int16_t afterRetrigger = env.nextBlockGainQ15(kFramesPer100Ms);
    CHECK(afterRetrigger > partialRelease);

    // Let it release fully this time.
    env.noteOff();
    for (int i = 0; i < 50 && !env.idle(); i++) {
      gain = env.nextBlockGainQ15(kFramesPer100Ms);
    }
    CHECK(env.idle());
    CHECK(gain == 0);
  }

  // Organ-style envelope (attack/release both instant): full scale on the
  // very first block, silence on the very first block after noteOff().
  {
    Envelope env;
    env.configure({0, 0, 0, 100}); // A0 D0 R0 S100%

    env.noteOn();
    CHECK(env.nextBlockGainQ15(kFramesPer100Ms) == VOLUME_Q15_ONE);
    CHECK(!env.idle());

    env.noteOff();
    CHECK(env.nextBlockGainQ15(kFramesPer100Ms) == 0);
    CHECK(env.idle());
  }

  // No-sustain (plucked) envelope: after enough decay, gain reaches 0 while
  // still nominally "sustaining" — idle() only flips once noteOff() releases.
  {
    Envelope env;
    env.configure({10, 50, 30, 0}); // A10 D50 R30 S0%

    env.noteOn();
    int16_t gain = 0;
    for (int i = 0; i < 30; i++) {
      gain = env.nextBlockGainQ15(kFramesPer100Ms);
    }
    CHECK_NEAR(gain, 0, VOLUME_Q15_ONE / 100);
    CHECK(!env.idle()); // still "sustaining" at ~0, hasn't been released
  }

  // sustainLengthMs auto-releases without an explicit noteOff() — a capped
  // hold time (e.g. so a stuck switch doesn't sustain forever).
  {
    Envelope env;
    env.configure({0, 0, 50, 100, 300}); // A0 D0 R50 S100% sustainLengthMs=300

    env.noteOn();
    CHECK(env.nextBlockGainQ15(kFramesPer100Ms) == VOLUME_Q15_ONE);  // 100ms
    CHECK(env.nextBlockGainQ15(kFramesPer100Ms) == VOLUME_Q15_ONE);  // 200ms

    // By 300ms the hold time is up; it should start releasing on its own.
    bool sawRelease = false;
    int16_t gain = VOLUME_Q15_ONE;
    for (int i = 0; i < 10; i++) {
      gain = env.nextBlockGainQ15(kFramesPer100Ms);
      if (gain < VOLUME_Q15_ONE) {
        sawRelease = true;
        break;
      }
    }
    CHECK(sawRelease);

    for (int i = 0; i < 20 && !env.idle(); i++) {
      env.nextBlockGainQ15(kFramesPer100Ms);
    }
    CHECK(env.idle());
  }

  // sustainLengthMs == 0 (the default for every EnvelopeConfig written
  // before this field existed) holds indefinitely — no surprise auto-release.
  {
    Envelope env;
    env.configure({0, 0, 0, 100}); // sustainLengthMs defaults to 0
    env.noteOn();
    for (int i = 0; i < 50; i++) {
      CHECK(env.nextBlockGainQ15(kFramesPer100Ms) == VOLUME_Q15_ONE);
    }
    CHECK(!env.idle());
  }
}

TEST_MAIN()
