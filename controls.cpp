#include "controls.h"
#include "controls_math.h"
#include <math.h>

OscParams g_oscParams;
SemaphoreHandle_t g_paramsMutex;

static const int k_VolPins[] = {PIN_POT_VOL1};
static const int k_PitchPins[] = {PIN_POT_PITCH1};
static_assert(sizeof(k_VolPins) / sizeof(k_VolPins[0]) == NUM_OSCILLATORS,
              "k_VolPins entries must match NUM_OSCILLATORS — add/remove PIN_POT_VOLn in pins.h");
static_assert(sizeof(k_PitchPins) / sizeof(k_PitchPins[0]) == NUM_PITCH_POTS,
              "k_PitchPins entries must match NUM_PITCH_POTS — add/remove PIN_POT_PITCHn in pins.h");
static_assert(NUM_PITCH_POTS <= NUM_OSCILLATORS,
              "can't have more pitch pots than oscillators");

static float s_volFiltered[NUM_OSCILLATORS];
static float s_pitchFiltered[NUM_PITCH_POTS];
static int s_volLastRaw[NUM_OSCILLATORS];
static int s_pitchLastRaw[NUM_PITCH_POTS];

static int readOversampled(int pin) {
  long sum = 0;
  for (int sampleIndex = 0; sampleIndex < ADC_OVERSAMPLE_COUNT; sampleIndex++) {
    sum += analogRead(pin);
  }
  return (int)(sum / ADC_OVERSAMPLE_COUNT);
}

void controlsBegin() {
  analogReadResolution(ADC_RESOLUTION_BITS);
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    analogSetPinAttenuation(k_VolPins[oscIndex], ADC_11db);
    int volRaw = analogRead(k_VolPins[oscIndex]);
    s_volFiltered[oscIndex] = volRaw;
    s_volLastRaw[oscIndex] = volRaw;
  }
  for (int potIndex = 0; potIndex < NUM_PITCH_POTS; potIndex++) {
    analogSetPinAttenuation(k_PitchPins[potIndex], ADC_11db);
    int pitchRaw = analogRead(k_PitchPins[potIndex]);
    s_pitchFiltered[potIndex] = pitchRaw;
    s_pitchLastRaw[potIndex] = pitchRaw;
  }

  pinMode(PIN_AUDIO_SWITCH, INPUT_PULLUP);

  g_paramsMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(g_paramsMutex, portMAX_DELAY);
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    g_oscParams.freqHz[oscIndex] = FREQ_MIN_HZ;
    g_oscParams.volume[oscIndex] = 0.0f;
  }
  g_oscParams.audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);
  xSemaphoreGive(g_paramsMutex);
}

void controlsUpdate() {
  float freq[NUM_OSCILLATORS];
  float vol[NUM_OSCILLATORS];

  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    int volRaw = readOversampled(k_VolPins[oscIndex]);
    float volFiltered = smoothValue(volRaw, &s_volFiltered[oscIndex], &s_volLastRaw[oscIndex],
                                     ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);
    vol[oscIndex] = constrain(volFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);
  }

  for (int potIndex = 0; potIndex < NUM_PITCH_POTS; potIndex++) {
    int pitchRaw = readOversampled(k_PitchPins[potIndex]);
    float pitchFiltered = smoothValue(pitchRaw, &s_pitchFiltered[potIndex], &s_pitchLastRaw[potIndex],
                                       ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);
    float pitchNorm = constrain(pitchFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);
    freq[potIndex] = mapPitchHz(pitchNorm, FREQ_MIN_HZ, FREQ_MAX_HZ);
  }
  bool audioOn = (digitalRead(PIN_AUDIO_SWITCH) == LOW);

  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
      g_oscParams.freqHz[oscIndex] = freq[oscIndex];
      g_oscParams.volume[oscIndex] = vol[oscIndex];
    }
    g_oscParams.audioOn = audioOn;
    xSemaphoreGive(g_paramsMutex);
  }
}
