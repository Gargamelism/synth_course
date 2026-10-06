#include "../vibrato.h"
#include "../pins.h" // SAMPLE_RATE_HZ
#include "framework.h"
#include <cmath>

// A block size that advances exactly 10 ms per advanceBlock() call.
static const int kFramesPer10Ms = SAMPLE_RATE_HZ / 100;

static float centsOf(float multiplier) { return 1200.0f * std::log2(multiplier); }

static void runTests() {
  // No vibrato configured: both multipliers stay exactly 1.
  {
    Vibrato vibrato;
    vibrato.noteOn();
    for (int i = 0; i < 50; i++) vibrato.advanceBlock(kFramesPer10Ms);
    CHECK(vibrato.pitchMultiplier() == 1.0f);
    CHECK(vibrato.amplitudeMultiplier() == 1.0f);
  }

  // Depth fades in over onsetMs: tiny right after note-on, then the peak
  // deviation over a full LFO cycle reaches +/- depthCents and never beyond.
  {
    Vibrato vibrato;
    vibrato.configure({5.0f, 20, 0, 300, 0});
    vibrato.noteOn();
    vibrato.advanceBlock(kFramesPer10Ms);
    CHECK(std::fabs(centsOf(vibrato.pitchMultiplier())) < 20.0f * 10 / 300 + 0.01f);

    for (int i = 0; i < 30; i++) vibrato.advanceBlock(kFramesPer10Ms); // past onset
    float maxCents = -100.0f, minCents = 100.0f;
    for (int i = 0; i < 20; i++) { // 200 ms = one 5 Hz cycle
      vibrato.advanceBlock(kFramesPer10Ms);
      float c = centsOf(vibrato.pitchMultiplier());
      maxCents = std::fmax(maxCents, c);
      minCents = std::fmin(minCents, c);
    }
    CHECK_NEAR(maxCents, 20.0f, 0.5f);
    CHECK_NEAR(minCents, -20.0f, 0.5f);

    // Re-trigger restarts the onset fade.
    vibrato.noteOn();
    vibrato.advanceBlock(kFramesPer10Ms);
    CHECK(std::fabs(centsOf(vibrato.pitchMultiplier())) < 1.0f);
  }

  // delayMs holds the depth at exactly 0, then the fade starts from 0.
  {
    Vibrato vibrato;
    vibrato.configure({5.0f, 20, 100, 200, 10});
    vibrato.noteOn();
    for (int i = 0; i < 10; i++) { // the full 100 ms delay
      vibrato.advanceBlock(kFramesPer10Ms);
      CHECK(vibrato.pitchMultiplier() == 1.0f);
      CHECK(vibrato.amplitudeMultiplier() == 1.0f);
    }
    vibrato.advanceBlock(kFramesPer10Ms); // 10 ms into the 200 ms fade
    CHECK(std::fabs(centsOf(vibrato.pitchMultiplier())) <= 20.0f * 10 / 200 + 0.01f);
  }

  // Tremolo dips the amplitude between 1 and 1 - tremoloPercent once onset
  // is complete; pitch stays put when depthCents == 0.
  {
    Vibrato vibrato;
    vibrato.configure({5.0f, 0, 0, 0, 10});
    vibrato.noteOn();
    float minAmp = 2.0f, maxAmp = -1.0f;
    for (int i = 0; i < 20; i++) {
      vibrato.advanceBlock(kFramesPer10Ms);
      CHECK(vibrato.pitchMultiplier() == 1.0f);
      minAmp = std::fmin(minAmp, vibrato.amplitudeMultiplier());
      maxAmp = std::fmax(maxAmp, vibrato.amplitudeMultiplier());
    }
    CHECK_NEAR(minAmp, 0.90f, 0.005f);
    CHECK_NEAR(maxAmp, 1.00f, 0.005f);
  }
}

TEST_MAIN()
