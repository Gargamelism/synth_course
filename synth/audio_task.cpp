#include "audio_task.h"
#include <ESP_I2S.h>
#include "pins.h"
#include "oscillator.h"
#include "voices.h"
#include "controls.h"
#include "distortion.h"
#include "envelope.h"

static I2SClass s_i2s;
static Oscillator s_osc[MAX_VOICES];
// The active patch's relative levels, normalized so the summed mix cannot
// clip however many voices the patch has (see normalizeVoiceLevelsQ15).
// Rebuilt by applyPatch() whenever the encoder switches patches.
static int16_t s_voiceLevelQ15[MAX_VOICES];
// One amplitude envelope for the whole active patch — its voices all
// trigger together (see voices.h: Patch::amp), so they share one gain
// applied to the mixed sample rather than each voice tracking its own.
static Envelope s_ampEnvelope;

// Extra fractional bits carried by the per-sample gain ramp in audioTask():
// a block-to-block Q15 gain change smaller than AUDIO_BLOCK_FRAMES would
// round to a 0 per-sample step in plain integer division, flattening the
// ramp instead of smoothing it. Widening the accumulator by this many bits
// keeps that fractional step instead of truncating it away.
static const int kGainRampFracBits = 16;
static const int32_t kGainRampFracScale = 1 << kGainRampFracBits; // 65536

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
  bool lastNoteHeld = false;
  uint8_t lastNoteOnCount = 0;
  int16_t prevGainQ15 = 0;  // last block's envelope gain, for the per-sample ramp below
  int16_t targetGainQ15 = 0; // this block's envelope gain target

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++) {
    freq[voiceIndex] = FREQ_MIN_HZ;
  }

  for (;;) {
    // Pick up the latest control values without ever blocking the audio
    // loop; if the control task momentarily holds the mutex, just reuse
    // last block's values (inaudible at block granularity).
    if (xSemaphoreTake(g_paramsMutex, 0) == pdTRUE) {
      bool noteHeld = g_oscParams.noteHeld;
      uint8_t noteOnCount = g_oscParams.noteOnCount;
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
        // Swapped in place, not re-triggered: a patch switch mid-note keeps
        // whatever gain the envelope is currently at (see voices.h's
        // Patch::amp comment / the Phase 1 plan's design decisions).
        s_ampEnvelope.configure(kPatches[patchIndex].amp);
      }
      voiceCount = liveVoiceCount;

      // noteHeld going true (or a new diatonic root, controls.cpp) is a
      // note-on; noteHeld going false is a note-off. Both drive the shared
      // envelope rather than muting the mix directly (see below).
      if (noteOnCount != lastNoteOnCount) {
        s_ampEnvelope.noteOn();
        lastNoteOnCount = noteOnCount;
      }
      if (!noteHeld && lastNoteHeld) {
        s_ampEnvelope.noteOff();
      }
      lastNoteHeld = noteHeld;
      targetGainQ15 = s_ampEnvelope.nextBlockGainQ15(AUDIO_BLOCK_FRAMES);

      // Convert the master volume to Q15 once per block (not per sample) so
      // the only float math left is off the per-sample hot path. Muting is
      // now the envelope's job (its release ramps to 0), not noteHeld.
      int16_t masterQ15 = (int16_t)(volume * VOLUME_Q15_ONE + 0.5f);
      for (int voiceIndex = 0; voiceIndex < voiceCount; voiceIndex++) {
        volQ15[voiceIndex] =
            (int16_t)(((int32_t)s_voiceLevelQ15[voiceIndex] * masterQ15) >> VOLUME_Q15_SHIFT);
        s_osc[voiceIndex].setFrequency(freq[voiceIndex]);
      }
    }

    // Per-sample linear ramp from last block's envelope gain to this
    // block's target, in a fixed-point accumulator with 16 extra bits of
    // precision — one add and one shift per frame, no zipper noise at the
    // block boundary (see envelope.h).
    int32_t gainQ15Fixed = (int32_t)prevGainQ15 << kGainRampFracBits;
    // Multiplication, not a shift: the difference can be negative (a
    // release), and left-shifting a negative int is undefined behavior
    // pre-C++20.
    const int32_t gainStepFixed = ((int32_t)(targetGainQ15 - prevGainQ15) * kGainRampFracScale) / AUDIO_BLOCK_FRAMES;

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
      const int16_t gainQ15 = (int16_t)(gainQ15Fixed >> kGainRampFracBits);
      mixed = (int16_t)(((int32_t)mixed * gainQ15) >> VOLUME_Q15_SHIFT);
      gainQ15Fixed += gainStepFixed;

      buffer[frameIndex * 2 + 0] = mixed; // L
      buffer[frameIndex * 2 + 1] = mixed; // R
    }
    prevGainQ15 = targetGainQ15;

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
  s_ampEnvelope.configure(kPatches[0].amp);

  // The audio task's stack also holds the per-voice frequency, level and
  // sample arrays, sized to MAX_VOICES since any patch can become active at
  // runtime; 4096 B leaves ample room (MAX_VOICES=32 voices ~ 320 B of
  // arrays).
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 1, nullptr, 0);
}
