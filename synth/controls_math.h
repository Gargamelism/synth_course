#pragma once

// Pure control-mapping math, split out of controls.cpp so it can be unit
// tested on the host (no Arduino/ESP32 dependency) — see test/.

// Smooths `raw` into `*filtered`/`*lastRaw` with an EMA + hysteresis gate,
// returns the value to use this cycle (filtered, but only updated if the
// change exceeds the hysteresis threshold — kills ADC/pot-wiper jitter).
float smoothValue(int raw, float *filtered, int *lastRaw, int hysteresisCounts, float emaAlpha);

// Maps a normalized ADC reading (0.0-1.0) to an exponential frequency sweep
// between freqMinHz and freqMaxHz.
float mapPitchHz(float pitchNorm, float freqMinHz, float freqMaxHz);
