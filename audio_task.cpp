#include "audio_task.h"
#include <ESP_I2S.h>
#include "pins.h"
#include "oscillator.h"
#include "voices.h"
#include "controls.h"
#include "distortion.h"

static I2SClass s_i2s;
static Oscillator s_osc[NUM_VOICES];
// kVoices' relative levels, normalized once at startup so the summed mix
// cannot clip however many voices there are (see normalizeVoiceLevelsQ15).
static int16_t s_voiceLevelQ15[NUM_VOICES];

static void audioTask(void *arg) {
  (void)arg;

  int16_t buffer[AUDIO_BLOCK_FRAMES * 2]; // interleaved L/R
  float freq[NUM_VOICES];
  int16_t volQ15[NUM_VOICES] = {0};

  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    freq[voiceIndex] = FREQ_MIN_HZ;
  }

  for (;;) {
    // Pick up the latest control values without ever blocking the audio
    // loop; if the control task momentarily holds the mutex, just reuse
    // last block's values (inaudible at block granularity).
    if (xSemaphoreTake(g_paramsMutex, 0) == pdTRUE) {
      bool audioOn = g_oscParams.audioOn;
      // Convert the master volume to Q15 once per block (not per sample) so
      // the only float math left is off the per-sample hot path. The
      // PIN_AUDIO_SWITCH gate forces this to 0 regardless of the volume
      // pot, muting every voice while it's off.
      int16_t masterQ15 = audioOn
          ? (int16_t)(g_oscParams.volume * VOLUME_Q15_ONE + 0.5f)
          : 0;
      for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
        freq[voiceIndex] = g_oscParams.freqHz[voiceIndex];
        volQ15[voiceIndex] =
            (int16_t)(((int32_t)s_voiceLevelQ15[voiceIndex] * masterQ15) >> VOLUME_Q15_SHIFT);
      }
      xSemaphoreGive(g_paramsMutex);
      for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
        s_osc[voiceIndex].setFrequency(freq[voiceIndex]);
      }
    }

    int16_t samples[NUM_VOICES];
    for (int frameIndex = 0; frameIndex < AUDIO_BLOCK_FRAMES; frameIndex++) {
      for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
        samples[voiceIndex] = s_osc[voiceIndex].nextSample();
#if defined(DISTORTION_ENABLED) && defined(DISTORTION_STAGE_PRE_MIX)
        // Each voice is distorted at full scale, before its level and the
        // master volume shrink it — so the drive gain means the same thing
        // whatever the voice count.
        samples[voiceIndex] = applyDistortion(samples[voiceIndex]);
#endif
      }
      int16_t mixed = mixOscillators(samples, volQ15, NUM_VOICES);
#if defined(DISTORTION_ENABLED) && defined(DISTORTION_STAGE_POST_MIX)
      mixed = applyDistortion(mixed);
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

  uint8_t levels[NUM_VOICES];
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    levels[voiceIndex] = kVoices[voiceIndex].level;
    s_osc[voiceIndex].begin((HarmonicSpread)kVoices[voiceIndex].spread);
  }
  normalizeVoiceLevelsQ15(levels, NUM_VOICES, s_voiceLevelQ15);

  // The audio task's stack also holds the per-voice frequency, level and
  // sample arrays, so it grows with NUM_VOICES; 4096 B leaves room at
  // MAX_VOICES (32 voices ~ 320 B of arrays).
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 1, nullptr, 0);
}
