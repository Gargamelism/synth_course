#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "controls.h"
#include "notes.h"

static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static const char *k_Title = "3-OSC SYNTH";

static bool snapshotOscParams(float freq[NUM_OSCILLATORS], float vol[NUM_OSCILLATORS]) {
  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    return false; // skip this refresh rather than block
  }
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    freq[oscIndex] = g_oscParams.freqHz[oscIndex];
    vol[oscIndex] = g_oscParams.volume[oscIndex];
  }
  xSemaphoreGive(g_paramsMutex);
  return true;
}

static bool anyOscillatorActive(const float vol[NUM_OSCILLATORS]) {
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    if (vol[oscIndex] > 0.0f) {
      return true;
    }
  }
  return false;
}

static void drawOscillatorRows(const float freq[NUM_OSCILLATORS], const float vol[NUM_OSCILLATORS]) {
  char noteBuf[NOTE_NAME_BUF_SIZE];
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    freqToNoteName(freq[oscIndex], noteBuf);
    s_display.setCursor(0, OLED_ROW_HEIGHT_PX + oscIndex * OLED_ROW_HEIGHT_PX);
    s_display.printf("O%d %4dHz %-3s V:%3d%%",
                      oscIndex + 1, (int)freq[oscIndex], noteBuf, (int)(vol[oscIndex] * 100.0f));
  }
}

static void drawFlatline() {
  // EKG-style flatline across the oscillator rows' vertical center.
  int lineY = OLED_ROW_HEIGHT_PX + (NUM_OSCILLATORS * OLED_ROW_HEIGHT_PX) / 2;
  s_display.drawLine(0, lineY, OLED_WIDTH - 1, lineY, SSD1306_WHITE);
}

bool displayBegin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!s_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    return false;
  }
  s_display.clearDisplay();
  s_display.setTextColor(SSD1306_WHITE);
  s_display.setTextSize(1);
  s_display.setCursor(0, 0);
  s_display.println(k_Title);
  s_display.display();
  return true;
}

void displayUpdate() {
  float freq[NUM_OSCILLATORS];
  float vol[NUM_OSCILLATORS];
  if (!snapshotOscParams(freq, vol)) {
    return;
  }

  s_display.clearDisplay();
  s_display.setCursor(0, 0);
  s_display.println(k_Title);

  if (anyOscillatorActive(vol)) {
    drawOscillatorRows(freq, vol);
  } else {
    drawFlatline();
  }

  s_display.display();
}
