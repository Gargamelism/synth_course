#pragma once

#include <stdint.h>

// Pure control-mapping math, split out of controls.cpp so it can be unit
// tested on the host (no Arduino/ESP32 dependency) — see test/.

// Smooths `raw` into `*filtered`/`*lastRaw` with an EMA + hysteresis gate,
// returns the value to use this cycle (filtered, but only updated if the
// change exceeds the hysteresis threshold — kills ADC/pot-wiper jitter).
float smoothValue(int raw, float *filtered, int *lastRaw, int hysteresisCounts, float emaAlpha);

// Maps a normalized ADC reading (0.0-1.0) to an exponential frequency sweep
// between freqMinHz and freqMaxHz.
float mapPitchHz(float pitchNorm, float freqMinHz, float freqMaxHz);

// One quadrature edge of a 2-bit encoder state (bit1 = A, bit0 = B), read
// by the encoder ISR on every A/B CHANGE. Returns +1 or -1 for a valid
// single-bit transition (four of these make one mechanical detent — see
// ENCODER_STEPS_PER_DETENT in pins.h), or 0 for a repeated state or an
// invalid two-bit jump (contact bounce, correctly dropped rather than
// mis-counted as a step).
int8_t quadratureStep(uint8_t prevState, uint8_t newState);
