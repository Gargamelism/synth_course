#include "controls.h"
#include "controls_math.h"
#include "notes.h"
#include "scale.h"
#include <math.h>

OscParams g_oscParams;
SemaphoreHandle_t g_paramsMutex;

// One master volume pot and one pitch pot serve every voice, so neither
// scales with NUM_VOICES any more.
static float s_volFiltered;
static float s_pitchFiltered;
static int s_volLastRaw;
static int s_pitchLastRaw;

// Voice frequencies are recomputed only when the pot actually moves to a new
// root — the diatonic path costs a powf per voice, which is fine in loop()
// but pointless to repeat while the pot sits still.
static float s_voiceFreq[NUM_VOICES];
static float s_rootHz;

#if defined(VOICE_PITCH_DIATONIC)
static int s_lastRootDegree = INT32_MIN;
#else
// Unison detune: each voice's offset is a fixed ratio, so it is computed
// once at startup and the pot sweep is a multiply.
static float s_detuneRatio[NUM_VOICES];
static float s_lastRootHz = -1.0f;
#endif

static int readOversampled(int pin) {
  long sum = 0;
  for (int sampleIndex = 0; sampleIndex < ADC_OVERSAMPLE_COUNT; sampleIndex++) {
    sum += analogRead(pin);
  }
  return (int)(sum / ADC_OVERSAMPLE_COUNT);
}

// Maps the smoothed pitch pot onto s_rootHz + s_voiceFreq[] for the pitch
// mode selected in voices.h.
static void updateVoiceFreqs(float pitchNorm) {
  const float potHz = mapPitchHz(pitchNorm, FREQ_MIN_HZ, FREQ_MAX_HZ);

#if defined(VOICE_PITCH_DIATONIC)
  const int rootDegree = snapMidiToScaleDegree(freqToMidi(potHz));
  if (rootDegree == s_lastRootDegree) {
    return;
  }
  s_lastRootDegree = rootDegree;
  s_rootHz = scaleDegreeToFreq(rootDegree);
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    // Offsetting in scale degrees (not semitones) is what makes the chord's
    // quality follow the root's position in the key.
    s_voiceFreq[voiceIndex] = scaleDegreeToFreq(rootDegree + kVoices[voiceIndex].scaleDegree);
  }
#else
  if (potHz == s_lastRootHz) {
    return;
  }
  s_lastRootHz = potHz;
  s_rootHz = potHz;
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    s_voiceFreq[voiceIndex] = potHz * s_detuneRatio[voiceIndex];
  }
#endif
}

void controlsBegin() {
  analogReadResolution(ADC_RESOLUTION_BITS);

  analogSetPinAttenuation(PIN_POT_VOL1, ADC_11db);
  s_volLastRaw = analogRead(PIN_POT_VOL1);
  s_volFiltered = s_volLastRaw;

  analogSetPinAttenuation(PIN_POT_PITCH1, ADC_11db);
  s_pitchLastRaw = analogRead(PIN_POT_PITCH1);
  s_pitchFiltered = s_pitchLastRaw;

#if defined(VOICE_PITCH_UNISON_DETUNE)
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    s_detuneRatio[voiceIndex] = powf(2.0f, kVoices[voiceIndex].detuneCents / 1200.0f);
  }
#endif

  s_rootHz = FREQ_MIN_HZ;
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    s_voiceFreq[voiceIndex] = FREQ_MIN_HZ;
  }

  pinMode(PIN_AUDIO_SWITCH, INPUT_PULLUP);

  g_paramsMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(g_paramsMutex, portMAX_DELAY);
  for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
    g_oscParams.freqHz[voiceIndex] = FREQ_MIN_HZ;
  }
  g_oscParams.rootHz = FREQ_MIN_HZ;
  g_oscParams.volume = 0.0f;
  g_oscParams.audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);
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
  updateVoiceFreqs(pitchNorm);

  bool audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);

  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    for (int voiceIndex = 0; voiceIndex < NUM_VOICES; voiceIndex++) {
      g_oscParams.freqHz[voiceIndex] = s_voiceFreq[voiceIndex];
    }
    g_oscParams.rootHz = s_rootHz;
    g_oscParams.volume = volume;
    g_oscParams.audioOn = audioOn;
    xSemaphoreGive(g_paramsMutex);
  }
}
