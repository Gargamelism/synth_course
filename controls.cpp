#include "controls.h"
#include <math.h>

OscParams g_oscParams;
SemaphoreHandle_t g_paramsMutex;

static const int k_VolPins[] = {PIN_POT_VOL1, PIN_POT_VOL2, PIN_POT_VOL3};
static const int k_PitchPins[] = {PIN_POT_PITCH1, PIN_POT_PITCH2, PIN_POT_PITCH3};
static_assert(sizeof(k_VolPins) / sizeof(k_VolPins[0]) == NUM_OSCILLATORS,
              "k_VolPins entries must match NUM_OSCILLATORS — add/remove PIN_POT_VOLn in pins.h");
static_assert(sizeof(k_PitchPins) / sizeof(k_PitchPins[0]) == NUM_OSCILLATORS,
              "k_PitchPins entries must match NUM_OSCILLATORS — add/remove PIN_POT_PITCHn in pins.h");

static float s_volFiltered[NUM_OSCILLATORS];
static float s_pitchFiltered[NUM_OSCILLATORS];
static int s_volLastRaw[NUM_OSCILLATORS];
static int s_pitchLastRaw[NUM_OSCILLATORS];

static int readOversampled(int pin) {
  long sum = 0;
  for (int sampleIndex = 0; sampleIndex < ADC_OVERSAMPLE_COUNT; sampleIndex++) {
    sum += analogRead(pin);
  }
  return (int)(sum / ADC_OVERSAMPLE_COUNT);
}

// Smooths `raw` into `*filtered`/`*lastRaw` with an EMA + hysteresis gate,
// returns the value to use this cycle (filtered, but only updated if the
// change exceeds the hysteresis threshold — kills ADC/pot-wiper jitter).
static float smooth(int raw, float *filtered, int *lastRaw) {
  if (abs(raw - *lastRaw) >= ADC_HYSTERESIS_COUNTS) {
    *filtered = (*filtered) * (1.0f - ADC_EMA_ALPHA) + raw * ADC_EMA_ALPHA;
    *lastRaw = raw;
  }
  return *filtered;
}

void controlsBegin() {
  analogReadResolution(ADC_RESOLUTION_BITS);
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    analogSetPinAttenuation(k_VolPins[oscIndex], ADC_11db);
    analogSetPinAttenuation(k_PitchPins[oscIndex], ADC_11db);
    int volRaw = analogRead(k_VolPins[oscIndex]);
    int pitchRaw = analogRead(k_PitchPins[oscIndex]);
    s_volFiltered[oscIndex] = volRaw;
    s_pitchFiltered[oscIndex] = pitchRaw;
    s_volLastRaw[oscIndex] = volRaw;
    s_pitchLastRaw[oscIndex] = pitchRaw;
  }

  g_paramsMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(g_paramsMutex, portMAX_DELAY);
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    g_oscParams.freqHz[oscIndex] = FREQ_MIN_HZ;
    g_oscParams.volume[oscIndex] = 0.0f;
  }
  xSemaphoreGive(g_paramsMutex);
}

void controlsUpdate() {
  float freq[NUM_OSCILLATORS];
  float vol[NUM_OSCILLATORS];

  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    int volRaw = readOversampled(k_VolPins[oscIndex]);
    int pitchRaw = readOversampled(k_PitchPins[oscIndex]);

    float volFiltered = smooth(volRaw, &s_volFiltered[oscIndex], &s_volLastRaw[oscIndex]);
    float pitchFiltered = smooth(pitchRaw, &s_pitchFiltered[oscIndex], &s_pitchLastRaw[oscIndex]);

    vol[oscIndex] = constrain(volFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);

    float pitchNorm = constrain(pitchFiltered / (float)ADC_MAX_COUNT, 0.0f, 1.0f);
    freq[oscIndex] = FREQ_MIN_HZ * powf(FREQ_MAX_HZ / FREQ_MIN_HZ, pitchNorm);
  }

  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
      g_oscParams.freqHz[oscIndex] = freq[oscIndex];
      g_oscParams.volume[oscIndex] = vol[oscIndex];
    }
    xSemaphoreGive(g_paramsMutex);
  }
}
