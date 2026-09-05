#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "controls.h"
#include "notes.h"
#include "distortion.h"
#include "oscillator.h"

// The SSD1306 buffer is 128x64; this board only shows a 72x40 window at
// (OLED_X_OFFSET, OLED_Y_OFFSET), so every draw below is shifted by that.
static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static const char *k_Title = "SI SY";

// Compile-time codes for the DISTORTION_*/HARMONIC_SPREAD_* macro picked in
// distortion.h/oscillator.h, shown on-screen so the active build is visible
// without cracking open those headers.
#if defined(DISTORTION_HARD_CLIP)
static const int k_DistortionCode = 1;
#elif defined(DISTORTION_SOFT_CLIP)
static const int k_DistortionCode = 2;
#elif defined(DISTORTION_FOLDBACK)
static const int k_DistortionCode = 3;
#elif defined(DISTORTION_BITCRUSH)
static const int k_DistortionCode = 4;
#else
static const int k_DistortionCode = 0; // passthrough
#endif

#if defined(HARMONIC_SPREAD_NATURAL)
static const int k_HarmonicsCode = 1;
#elif defined(HARMONIC_SPREAD_OCTAVE)
static const int k_HarmonicsCode = 2;
#elif defined(HARMONIC_SPREAD_ODD)
static const int k_HarmonicsCode = 3;
#elif defined(HARMONIC_SPREAD_EQUAL)
static const int k_HarmonicsCode = 4;
#endif

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

static void drawTitle() {
  s_display.setCursor(OLED_X_OFFSET, OLED_Y_OFFSET);
  s_display.print(k_Title);
}

static void drawOscillatorRows(const float freq[NUM_OSCILLATORS], const float vol[NUM_OSCILLATORS]) {
  char noteBuf[NOTE_NAME_BUF_SIZE];
  const int barX = OLED_X_OFFSET + OLED_VOL_BAR_X_PX;
  const int barW = OLED_VISIBLE_WIDTH - OLED_VOL_BAR_X_PX;
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    int rowY = OLED_Y_OFFSET + OLED_OSC_ROW_TOP_PX + oscIndex * OLED_OSC_ROW_SPACING_PX;
    freqToNoteName(freq[oscIndex], noteBuf);

    s_display.setCursor(OLED_X_OFFSET, rowY);
    s_display.printf("%-3s", noteBuf);

    // Volume as a proportional bar filling the rest of the row.
    s_display.drawRect(barX, rowY, barW, OLED_TEXT_HEIGHT_PX, SSD1306_WHITE);
    int fillW = (int)(vol[oscIndex] * (barW - 2) + 0.5f);
    if (fillW > 0) {
      s_display.fillRect(barX + 1, rowY + 1, fillW, OLED_TEXT_HEIGHT_PX - 2, SSD1306_WHITE);
    }
  }
}

static void drawSettingsRow() {
  // One row below the last oscillator row, so it never collides even if
  // NUM_OSCILLATORS changes.
  int rowY = OLED_Y_OFFSET + OLED_OSC_ROW_TOP_PX + NUM_OSCILLATORS * OLED_OSC_ROW_SPACING_PX;
  s_display.setCursor(OLED_X_OFFSET, rowY);
  s_display.printf("D%d/H%d", k_DistortionCode, k_HarmonicsCode);
}

static void drawFlatline() {
  // EKG-style flatline across the visible window's vertical center.
  int lineY = OLED_Y_OFFSET + OLED_VISIBLE_HEIGHT / 2;
  s_display.drawLine(OLED_X_OFFSET, lineY,
                     OLED_X_OFFSET + OLED_VISIBLE_WIDTH - 1, lineY, SSD1306_WHITE);
}

bool displayBegin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!s_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    return false;
  }
  s_display.clearDisplay();
  s_display.setTextColor(SSD1306_WHITE);
  s_display.setTextSize(1);
  drawTitle();
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
  drawTitle();

  if (anyOscillatorActive(vol)) {
    drawOscillatorRows(freq, vol);
  } else {
    drawFlatline();
  }
  drawSettingsRow();

  s_display.display();
}
