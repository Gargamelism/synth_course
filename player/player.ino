// Speaker/amp test player: loops a short embedded audio clip over I2S,
// scaled live by the master volume pot, so you can find the speaker's max
// clean level and check its response with real material instead of a
// synthesized tone.
//
// Standalone sketch on purpose — does not include ../synth's headers, so
// distortion heard here can't be confused with distortion from the synth's
// wavetables. Wiring below mirrors synth/pins.h and must be kept in sync by
// hand if that wiring ever changes.
//
// Usage:
//   1. Convert your clip to 16-bit PCM mono WAV, e.g.:
//        ffmpeg -i input.mp3 -ac 1 -ar 22050 -sample_fmt s16 clip.wav
//   2. Generate clip.h from it: python3 wav_to_header.py clip.wav
//   3. Compile/upload this sketch, then turn the volume pot while it plays.

#include <ESP_I2S.h>
#include "clip.h"

#define PIN_I2S_BCK  20
#define PIN_I2S_LRCK 7
#define PIN_I2S_DOUT 10
#define PIN_POT_VOL1 0

const int ADC_RESOLUTION_BITS = 12;
const int ADC_MAX_COUNT = (1 << ADC_RESOLUTION_BITS) - 1;
const int ADC_OVERSAMPLE_COUNT = 8;
const float ADC_EMA_ALPHA = 0.15f;

const int BLOCK_FRAMES = 256; // stereo frames generated + written per I2S block
const uint32_t VOL_REPORT_INTERVAL_MS = 500;

static I2SClass s_i2s;
static float s_volFiltered;
static uint32_t s_clipPos = 0;
static uint32_t s_lastReportMs = 0;

static int readOversampledVolume() {
  long sum = 0;
  for (int sampleIndex = 0; sampleIndex < ADC_OVERSAMPLE_COUNT; sampleIndex++) {
    sum += analogRead(PIN_POT_VOL1);
  }
  return (int)(sum / ADC_OVERSAMPLE_COUNT);
}

void setup() {
  Serial.begin(115200);
  delay(200); // USB CDC enumeration, same board quirk as the synth sketch

  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(PIN_POT_VOL1, ADC_11db);
  s_volFiltered = readOversampledVolume();

  s_i2s.setPins(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  s_i2s.begin(I2S_MODE_STD, kClipSampleRateHz, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

  Serial.printf("player: %u samples @ %u Hz (%.1fs) — turn the volume pot to set level\n",
                (unsigned)kClipSampleCount, (unsigned)kClipSampleRateHz,
                (float)kClipSampleCount / (float)kClipSampleRateHz);
}

void loop() {
  int volRaw = readOversampledVolume();
  s_volFiltered += ADC_EMA_ALPHA * (volRaw - s_volFiltered);
  float volume = constrain(s_volFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);
  int16_t gainQ15 = (int16_t)(volume * 32767.0f + 0.5f);

  int16_t buffer[BLOCK_FRAMES * 2]; // interleaved L/R
  for (int frameIndex = 0; frameIndex < BLOCK_FRAMES; frameIndex++) {
    int16_t sample = (int16_t)(((int32_t)kClipSamples[s_clipPos] * gainQ15) >> 15);
    buffer[frameIndex * 2 + 0] = sample;
    buffer[frameIndex * 2 + 1] = sample;
    s_clipPos++;
    if (s_clipPos >= kClipSampleCount) {
      s_clipPos = 0; // loop the clip so the pot can keep being adjusted
    }
  }

  // Blocking write paces the loop to real time; no delay() needed.
  size_t written = s_i2s.write((uint8_t *)buffer, sizeof(buffer));
  if (written < sizeof(buffer)) {
    delay(1);
  }

  uint32_t now = millis();
  if (now - s_lastReportMs >= VOL_REPORT_INTERVAL_MS) {
    s_lastReportMs = now;
    Serial.printf("volume: %d%%\n", (int)(volume * 100.0f + 0.5f));
  }
}
