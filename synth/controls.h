#pragma once

#include <Arduino.h>
#include "pins.h"
#include "voices.h"

// Live parameters shared between the loop() control path and the Core-0
// audio task. Guarded by g_paramsMutex.
struct OscParams {
  float freqHz[NUM_VOICES]; // one per kVoices row, derived from the pitch pot
  float rootHz;             // the pitch pot's root note, what the display shows
  float volume;             // master, 0.0 - 1.0 — scales every voice
  bool audioOn;             // PIN_AUDIO_SWITCH state — false mutes everything
};

extern OscParams g_oscParams;
extern SemaphoreHandle_t g_paramsMutex;

void controlsBegin();
// Reads the master volume pot, the pitch pot and the audio on/off switch,
// smooths the pots, derives every voice's frequency from kVoices, and
// updates g_oscParams under the mutex. Call from loop().
void controlsUpdate();
