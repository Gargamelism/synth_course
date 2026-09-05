#pragma once

#include <Arduino.h>
#include "pins.h"

// Live parameters shared between the Core-1 control loop and the Core-0
// audio task. Guarded by g_paramsMutex.
struct OscParams {
  float freqHz[NUM_OSCILLATORS];
  float volume[NUM_OSCILLATORS]; // 0.0 - 1.0
  bool audioOn; // PIN_AUDIO_SWITCH state — false mutes all oscillators
};

extern OscParams g_oscParams;
extern SemaphoreHandle_t g_paramsMutex;

void controlsBegin();
// Reads all 5 pots (3 volume + 2 pitch; osc 3 has no pitch pot and holds
// OSC3_FIXED_FREQ_HZ) and the audio on/off switch, smooths the pots, and
// updates g_oscParams under the mutex. Call from loop().
void controlsUpdate();
