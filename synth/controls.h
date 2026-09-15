#pragma once

#include <Arduino.h>
#include "pins.h"
#include "voices.h"

// Live parameters shared between the loop() control path and the Core-0
// audio task. Guarded by g_paramsMutex. All of it is written together under
// one lock/unlock in controlsUpdate() — patchIndex and freqHz/voiceCount
// must never be observed out of sync with each other, or the audio task
// could mix the new patch's voice count against the old patch's
// frequencies for one block.
struct OscParams {
  float freqHz[MAX_VOICES]; // only [0, kPatches[patchIndex].voiceCount) are
                            // meaningful; derived from the pitch pot
  float rootHz;             // the pitch pot's root note, what the display shows
  float volume;             // master, 0.0 - 1.0 — scales every voice
  bool noteHeld;            // PIN_AUDIO_SWITCH state — true while held closed;
                            // the amplitude envelope's note-on/note-off gate
                            // (envelope.h), not a hard mute
  uint8_t noteOnCount;      // increments on every envelope trigger (noteHeld
                            // going true, or a new diatonic root) — a
                            // counter, not an edge flag, so audioTask()
                            // (which only reads this once per ~5.8ms block)
                            // never misses one
  uint8_t patchIndex;       // index into kPatches (voices.h) — the encoder's
                            // rotation target; voice count/pitch mode/
                            // matched distortion all derive from this
  bool distortionEnabled;   // encoder click — gates the active patch's
                            // matched distortion on/off
};

extern OscParams g_oscParams;
extern SemaphoreHandle_t g_paramsMutex;

void controlsBegin();
// Reads the master volume pot, the pitch pot, the envelope gate switch, and
// the encoder (rotation + button), smooths the pots, derives every voice's
// frequency from the active patch (kPatches, voices.h), and updates
// g_oscParams under the mutex. Call from loop().
void controlsUpdate();
