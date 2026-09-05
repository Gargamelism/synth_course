#include "audio_task.h"
#include <ESP_I2S.h>
#include "pins.h"
#include "oscillator.h"
#include "controls.h"
#include "distortion.h"

static I2SClass s_i2s;
static Oscillator s_osc[NUM_OSCILLATORS];

static void audioTask(void *arg) {
  (void)arg;

  int16_t buffer[AUDIO_BLOCK_FRAMES * 2]; // interleaved L/R
  float freq[NUM_OSCILLATORS];
  int16_t volQ15[NUM_OSCILLATORS] = {0};

  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    freq[oscIndex] = FREQ_MIN_HZ;
  }

  for (;;) {
    // Pick up the latest control values without ever blocking the audio
    // loop; if the control task momentarily holds the mutex, just reuse
    // last block's values (inaudible at block granularity).
    if (xSemaphoreTake(g_paramsMutex, 0) == pdTRUE) {
      bool audioOn = g_oscParams.audioOn;
      for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
        freq[oscIndex] = g_oscParams.freqHz[oscIndex];
        // Convert 0.0-1.0 volume to Q15 once per block (not per sample) so
        // the only float math left is off the per-sample hot path. The
        // PIN_AUDIO_SWITCH gate forces this to 0 regardless of the volume
        // pot, muting every oscillator while it's off.
        volQ15[oscIndex] = audioOn
            ? (int16_t)(g_oscParams.volume[oscIndex] * VOLUME_Q15_ONE + 0.5f)
            : 0;
      }
      xSemaphoreGive(g_paramsMutex);
      for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
        s_osc[oscIndex].setFrequency(freq[oscIndex]);
      }
    }

    int16_t samples[NUM_OSCILLATORS];
    for (int frameIndex = 0; frameIndex < AUDIO_BLOCK_FRAMES; frameIndex++) {
      for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
        samples[oscIndex] = s_osc[oscIndex].nextSample();
      }
      int16_t mixed = applyDistortion(mixOscillators(samples, volQ15, NUM_OSCILLATORS));
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

  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    s_osc[oscIndex].begin();
  }

  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 1, nullptr, 0);
}
