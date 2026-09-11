#include "audio_task.h"
#include <ESP_I2S.h>
#include "pins.h"
#include "oscillator.h"
#include "voices.h"
#include "controls.h"
#include "distortion.h"

static I2SClass s_i2s;
static Oscillator s_osc[MAX_VOICES];
// The active patch's relative levels, normalized so the summed mix cannot
// clip however many voices the patch has (see normalizeVoiceLevelsQ15).
// Rebuilt by applyPatch() whenever the encoder switches patches.
static int16_t s_voiceLevelQ15[MAX_VOICES];

// (Re)points every active voice's oscillator at patchIndex's wavetables and
// rebuilds s_voiceLevelQ15 for its voice count. Called once at startup and
// again whenever audioTask() sees patchIndex change — never on the
// per-sample path. Oscillator::begin() resets phase, so this causes a
// small audible click across affected voices on a patch change (acceptable,
// mirrors what already happens at startup).
static void applyPatch(uint8_t patchIndex) {
  const Patch &patch = kPatches[patchIndex];
  uint8_t levels[MAX_VOICES];
  for (int voiceIndex = 0; voiceIndex < patch.voiceCount; voiceIndex++) {
    levels[voiceIndex] = patch.voices[voiceIndex].level;
    s_osc[voiceIndex].begin((HarmonicSpread)patch.voices[voiceIndex].spread);
  }
  normalizeVoiceLevelsQ15(levels, patch.voiceCount, s_voiceLevelQ15);
}

static void audioTask(void *arg) {
  (void)arg;

  int16_t buffer[AUDIO_BLOCK_FRAMES * 2]; // interleaved L/R
  float freq[MAX_VOICES];
  int16_t volQ15[MAX_VOICES] = {0};
  uint8_t voiceCount = kPatches[0].voiceCount;    // matches audioTaskBegin()'s
  DistortionType distortion = kPatches[0].distortion; // initial applyPatch(0)
  bool distortionEnabled = false;
  uint8_t lastPatchIndex = 0;

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++) {
    freq[voiceIndex] = FREQ_MIN_HZ;
  }

  for (;;) {
    // Pick up the latest control values without ever blocking the audio
    // loop; if the control task momentarily holds the mutex, just reuse
    // last block's values (inaudible at block granularity).
    if (xSemaphoreTake(g_paramsMutex, 0) == pdTRUE) {
      bool audioOn = g_oscParams.audioOn;
      float volume = g_oscParams.volume;
      uint8_t patchIndex = g_oscParams.patchIndex;
      distortionEnabled = g_oscParams.distortionEnabled;
      // patchIndex and freqHz[] were published together under the same
      // lock in controls.cpp, so reading voiceCount from kPatches here
      // (compile-time data, not shared mutable state) stays consistent
      // with the freqHz entries just copied below.
      const uint8_t liveVoiceCount = kPatches[patchIndex].voiceCount;
      for (int voiceIndex = 0; voiceIndex < liveVoiceCount; voiceIndex++) {
        freq[voiceIndex] = g_oscParams.freqHz[voiceIndex];
      }
      xSemaphoreGive(g_paramsMutex);

      if (patchIndex != lastPatchIndex) {
        // Rebuild s_voiceLevelQ15 for the new patch BEFORE computing this
        // block's volQ15 below — otherwise this block would scale the new
        // voice count against the old patch's stale normalized levels.
        applyPatch(patchIndex);
        lastPatchIndex = patchIndex;
        distortion = kPatches[patchIndex].distortion;
      }
      voiceCount = liveVoiceCount;

      // Convert the master volume to Q15 once per block (not per sample) so
      // the only float math left is off the per-sample hot path. The
      // PIN_AUDIO_SWITCH gate forces this to 0 regardless of the volume
      // pot, muting every voice while it's off.
      int16_t masterQ15 = audioOn
          ? (int16_t)(volume * VOLUME_Q15_ONE + 0.5f)
          : 0;
      for (int voiceIndex = 0; voiceIndex < voiceCount; voiceIndex++) {
        volQ15[voiceIndex] =
            (int16_t)(((int32_t)s_voiceLevelQ15[voiceIndex] * masterQ15) >> VOLUME_Q15_SHIFT);
        s_osc[voiceIndex].setFrequency(freq[voiceIndex]);
      }
    }

    int16_t samples[MAX_VOICES];
    for (int frameIndex = 0; frameIndex < AUDIO_BLOCK_FRAMES; frameIndex++) {
      for (int voiceIndex = 0; voiceIndex < voiceCount; voiceIndex++) {
        samples[voiceIndex] = s_osc[voiceIndex].nextSample();
#if defined(DISTORTION_STAGE_PRE_MIX)
        // Each voice is distorted at full scale, before its level and the
        // master volume shrink it — so the drive gain means the same thing
        // whatever the voice count. Skipped entirely (not called with
        // DIST_NONE) when the encoder has toggled distortion off.
        if (distortionEnabled) {
          samples[voiceIndex] = applyDistortion(samples[voiceIndex], distortion);
        }
#endif
      }
      int16_t mixed = mixOscillators(samples, volQ15, voiceCount);
#if defined(DISTORTION_STAGE_POST_MIX)
      if (distortionEnabled) {
        mixed = applyDistortion(mixed, distortion);
      }
#endif
      buffer[frameIndex * 2 + 0] = mixed; // L
      buffer[frameIndex * 2 + 1] = mixed; // R
    }

    // Blocking write paces the loop to real time; no delay() needed. If the
    // write comes up short (I2S not started, or a driver error) this
    // max-priority task would otherwise spin Core 0 — the C3's only core —
    // and freeze loop(), the display, and the liveness LED. Yield instead.
    size_t written = s_i2s.write((uint8_t *)buffer, sizeof(buffer));
    if (written < sizeof(buffer)) {
      vTaskDelay(1);
    }
  }
}

void audioTaskBegin() {
  s_i2s.setPins(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  s_i2s.begin(I2S_MODE_STD, SAMPLE_RATE_HZ, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

  applyPatch(0); // matches controls.cpp's initial patchIndex

  // The audio task's stack also holds the per-voice frequency, level and
  // sample arrays, sized to MAX_VOICES since any patch can become active at
  // runtime; 4096 B leaves ample room (MAX_VOICES=32 voices ~ 320 B of
  // arrays).
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 1, nullptr, 0);
}
