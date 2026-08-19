#pragma once

// Initializes I2S output and spawns the audio-generation task pinned to
// Core 0. Call once from setup(), after controlsBegin() and initSineTable().
void audioTaskBegin();
