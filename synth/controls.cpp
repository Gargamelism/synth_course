#include "controls.h"
#include "controls_math.h"
#include "notes.h"
#include "scale.h"
#include <math.h>

OscParams g_oscParams;
SemaphoreHandle_t g_paramsMutex;

// One master volume pot and one pitch pot serve every voice, so neither
// scales with the active patch's voice count.
static float s_volFiltered;
static float s_pitchFiltered;
static int s_volLastRaw;
static int s_pitchLastRaw;

// Voice frequencies are recomputed only when the pot actually moves to a new
// root, or the patch changes — the diatonic path costs a powf per voice,
// which is fine in loop() but pointless to repeat while the pot sits still.
static float s_voiceFreq[MAX_VOICES];
static float s_rootHz;
static int s_lastRootDegree = INT32_MIN;       // PITCH_DIATONIC cache
static float s_lastRootHz = -1.0f;             // PITCH_UNISON_DETUNE cache
static float s_detuneRatio[MAX_VOICES];        // PITCH_UNISON_DETUNE ratios,
                                                // rebuilt whenever the patch
                                                // changes (each patch has
                                                // its own table)
static int s_lastPatchIndexForFreq = -1;       // forces a recompute above on
                                                // a patch change even if the
                                                // pot hasn't moved

// The encoder's rotation target (index into kPatches) and its button's
// toggle state. Both are plain statics — only controlsUpdate() (loop()'s
// thread) touches them outside the ISR below.
static uint8_t s_patchIndex = 0;
static bool s_distortionEnabled = true;

// Raw quadrature edges accumulated by the ISR since the last drain, and the
// 2-bit A/B state they were measured against. Both written only from the
// ISR; s_encoderStepAccum is also read+cleared from controlsUpdate() under
// s_encoderMux, which is what makes that read/clear atomic with respect to
// the ISR (this is a single-core part, but an ISR still preempts loop()).
static volatile int32_t s_encoderStepAccum = 0;
static volatile uint8_t s_encoderPrevState = 0;
static portMUX_TYPE s_encoderMux = portMUX_INITIALIZER_UNLOCKED;

// Fractional detent progress carried between controlsUpdate() calls — this
// encoder's Gray code yields ENCODER_STEPS_PER_DETENT raw edges per
// mechanical click (see pins.h), so a single edge isn't a whole patch step.
static int32_t s_detentAccum = 0;

static bool s_lastSwPressed = false;
static uint32_t s_lastSwToggleMs = 0;

static int readOversampled(int pin) {
  long sum = 0;
  for (int sampleIndex = 0; sampleIndex < ADC_OVERSAMPLE_COUNT; sampleIndex++) {
    sum += analogRead(pin);
  }
  return (int)(sum / ADC_OVERSAMPLE_COUNT);
}

// Minimal ISR: read both pins, score the transition, accumulate. No
// Serial/millis/allocation — this preempts even the max-priority audio
// task on this board's single core, so it has to be short and IRAM-resident.
static void IRAM_ATTR encoderISR() {
  const uint8_t newState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
  s_encoderStepAccum += quadratureStep(s_encoderPrevState, newState);
  s_encoderPrevState = newState;
}

// Drains the ISR's raw edge counter and advances s_patchIndex by whole
// detents (wrapping through kPatches). Polling would miss edges here —
// loop() only runs when the audio task yields, about once per 256-frame
// block (~5.8ms), slower than a fast spin's edge spacing — which is why
// rotation is interrupt-driven while the button below is polled.
static void updateEncoderRotation() {
  int32_t rawSteps;
  portENTER_CRITICAL(&s_encoderMux);
  rawSteps = s_encoderStepAccum;
  s_encoderStepAccum = 0;
  portEXIT_CRITICAL(&s_encoderMux);

  s_detentAccum += rawSteps;

  int patchDelta = 0;
  while (s_detentAccum >= ENCODER_STEPS_PER_DETENT) {
    patchDelta++;
    s_detentAccum -= ENCODER_STEPS_PER_DETENT;
  }
  while (s_detentAccum <= -ENCODER_STEPS_PER_DETENT) {
    patchDelta--;
    s_detentAccum += ENCODER_STEPS_PER_DETENT;
  }
  if (patchDelta == 0) {
    return;
  }
  int newIndex = ((int)s_patchIndex + patchDelta) % NUM_PATCHES;
  if (newIndex < 0) {
    newIndex += NUM_PATCHES;
  }
  s_patchIndex = (uint8_t)newIndex;
}

// A press is tens of ms long, comfortably caught at loop()'s ~5.8ms polling
// cadence, so unlike rotation this doesn't need the ISR's speed (or its
// risk) — time-based debounce is enough.
static void updateEncoderButton() {
  const bool pressed = (digitalRead(PIN_ENCODER_SW) == LOW);
  const uint32_t now = millis();
  if (pressed && !s_lastSwPressed && (now - s_lastSwToggleMs) >= ENCODER_BUTTON_DEBOUNCE_MS) {
    s_distortionEnabled = !s_distortionEnabled;
    s_lastSwToggleMs = now;
  }
  s_lastSwPressed = pressed;
}

// Maps the smoothed pitch pot onto s_rootHz + s_voiceFreq[] for the given
// patch's pitch mode. patchIndex is passed in (not read from a global) so a
// patch change is always visible here, even the same call it happens in.
static void updateVoiceFreqs(float pitchNorm, uint8_t patchIndex) {
  const Patch &patch = kPatches[patchIndex];
  const float potHz = mapPitchHz(pitchNorm, FREQ_MIN_HZ, FREQ_MAX_HZ);

  if ((int)patchIndex != s_lastPatchIndexForFreq) {
    // A new patch needs recomputing even if the pot hasn't moved, and
    // unison-detune's ratios are per-patch (each table sets its own
    // detuneCents), so they're rebuilt here rather than once at startup.
    if (patch.pitchMode == PITCH_UNISON_DETUNE) {
      for (int voiceIndex = 0; voiceIndex < patch.voiceCount; voiceIndex++) {
        s_detuneRatio[voiceIndex] = powf(2.0f, patch.voices[voiceIndex].detuneCents / (float)kCentsPerOctave);
      }
    }
    s_lastPatchIndexForFreq = (int)patchIndex;
    s_lastRootDegree = INT32_MIN; // force the diatonic branch below to run
    s_lastRootHz = -1.0f;         // force the unison-detune branch to run
  }

  switch (patch.pitchMode) {
    case PITCH_DIATONIC: {
      const int rootDegree = snapMidiToScaleDegree(freqToMidi(potHz), patch.scale);
      if (rootDegree == s_lastRootDegree) {
        return;
      }
      s_lastRootDegree = rootDegree;
      s_rootHz = scaleDegreeToFreq(rootDegree, patch.scale);
      for (int voiceIndex = 0; voiceIndex < patch.voiceCount; voiceIndex++) {
        // Offsetting in scale degrees (not semitones) is what makes the
        // chord's quality follow the root's position in the key.
        s_voiceFreq[voiceIndex] = scaleDegreeToFreq(rootDegree + patch.voices[voiceIndex].scaleDegree, patch.scale);
      }
      break;
    }
    case PITCH_UNISON_DETUNE: {
      if (potHz == s_lastRootHz) {
        return;
      }
      s_lastRootHz = potHz;
      s_rootHz = potHz;
      for (int voiceIndex = 0; voiceIndex < patch.voiceCount; voiceIndex++) {
        s_voiceFreq[voiceIndex] = potHz * s_detuneRatio[voiceIndex];
      }
      break;
    }
  }
}

void controlsBegin() {
  analogReadResolution(ADC_RESOLUTION_BITS);

  analogSetPinAttenuation(PIN_POT_VOL1, ADC_11db);
  s_volLastRaw = analogRead(PIN_POT_VOL1);
  s_volFiltered = s_volLastRaw;

  analogSetPinAttenuation(PIN_POT_PITCH1, ADC_11db);
  s_pitchLastRaw = analogRead(PIN_POT_PITCH1);
  s_pitchFiltered = s_pitchLastRaw;

  s_rootHz = FREQ_MIN_HZ;
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++) {
    s_voiceFreq[voiceIndex] = FREQ_MIN_HZ;
  }

  pinMode(PIN_AUDIO_SWITCH, INPUT_PULLUP);

  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
  s_encoderPrevState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoderISR, CHANGE);
  s_lastSwPressed = (digitalRead(PIN_ENCODER_SW) == LOW);

  g_paramsMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(g_paramsMutex, portMAX_DELAY);
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++) {
    g_oscParams.freqHz[voiceIndex] = FREQ_MIN_HZ;
  }
  g_oscParams.rootHz = FREQ_MIN_HZ;
  g_oscParams.volume = 0.0f;
  g_oscParams.audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);
  g_oscParams.patchIndex = s_patchIndex;
  g_oscParams.distortionEnabled = s_distortionEnabled;
  xSemaphoreGive(g_paramsMutex);
}

void controlsUpdate() {
  int volRaw = readOversampled(PIN_POT_VOL1);
  float volFiltered = smoothValue(volRaw, &s_volFiltered, &s_volLastRaw,
                                  ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);
  float volume = constrain(volFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);

  int pitchRaw = readOversampled(PIN_POT_PITCH1);
  float pitchFiltered = smoothValue(pitchRaw, &s_pitchFiltered, &s_pitchLastRaw,
                                    ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);
  float pitchNorm = constrain(pitchFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);

  updateEncoderRotation();
  updateEncoderButton();
  updateVoiceFreqs(pitchNorm, s_patchIndex);

  bool audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);

  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    // Everything published together in one lock/unlock: patchIndex and
    // freqHz[] must never be observed out of step with each other (see
    // OscParams's comment in controls.h).
    const uint8_t voiceCount = kPatches[s_patchIndex].voiceCount;
    for (int voiceIndex = 0; voiceIndex < voiceCount; voiceIndex++) {
      g_oscParams.freqHz[voiceIndex] = s_voiceFreq[voiceIndex];
    }
    g_oscParams.rootHz = s_rootHz;
    g_oscParams.volume = volume;
    g_oscParams.audioOn = audioOn;
    g_oscParams.patchIndex = s_patchIndex;
    g_oscParams.distortionEnabled = s_distortionEnabled;
    xSemaphoreGive(g_paramsMutex);
  }
}
