#pragma once

// Initializes I2S output and spawns the audio-generation task pinned to
// Core 0. On single-core targets (e.g. ESP32-C3) Core 0 is the only core,
// so loop() shares it with the audio task; the audio task's max priority
// still lets FreeRTOS preempt loop() whenever it needs to run.
// Call once from setup(), after controlsBegin() and initSineTable().
void audioTaskBegin();
