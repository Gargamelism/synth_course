#pragma once

#include <Arduino.h>
#include "pins.h"

// Live parameters shared between the Core-1 control loop and the Core-0
// audio task. Guarded by g_paramsMutex.
struct OscParams {
  float freqHz[NUM_OSCILLATORS];
  float volume[NUM_OSCILLATORS]; // 0.0 - 1.0
};

extern OscParams g_oscParams;
extern SemaphoreHandle_t g_paramsMutex;

void controlsBegin();
// Reads all 6 pots, smooths them, and updates g_oscParams under the mutex.
// Call from loop() on Core 1.
void controlsUpdate();
