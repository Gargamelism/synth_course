#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "controls.h"
#include "voices.h"
#include "notes.h"
#include "distortion.h"
#include "oscillator.h"

// The SSD1306 buffer is 128x64; this board only shows a 72x40 window at
// (OLED_X_OFFSET, OLED_Y_OFFSET), so every draw below is shifted by that.
static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static const char *k_Title = "SI SY";

// Compile-time codes for the build switches picked in distortion.h and
// voices.h, shown on-screen so the active build is visible without cracking
// open those headers.
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

// No #else on purpose: an unknown pitch mode must fail the build rather
// than quietly display the wrong thing.
#if defined(VOICE_PITCH_DIATONIC)
static const char *k_PitchModeCode = "DIA";
#elif defined(VOICE_PITCH_UNISON_DETUNE)
static const char *k_PitchModeCode = "UNI";
#endif

// Voice 0's spread, 1-based to keep the codes the old H field used
// (natural = 1 .. viola = 5), plus a "*" when the other voices don't all
// share it. Both are compile-time constants folded out of kVoices.
static const int k_SpreadCode = (int)kVoices[0].spread + 1;
static const char *k_SpreadSuffix = voiceSpreadsMixed() ? "*" : "";

// Only the root note and the master volume are on screen, so the display
// holds the mutex just long enough to copy those two.
static bool snapshotStatus(float *rootHz, float *volume) {
  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    return false; // skip this refresh rather than block
  }
  *rootHz = g_oscParams.rootHz;
  *volume = g_oscParams.volume;
  xSemaphoreGive(g_paramsMutex);
  return true;
}

static int rowY(int row) {
  return OLED_Y_OFFSET + OLED_STATUS_ROW_TOP_PX + row * OLED_STATUS_ROW_SPACING_PX;
}

static void drawTitle() {
  s_display.setCursor(OLED_X_OFFSET, OLED_Y_OFFSET);
  s_display.print(k_Title);
}

// Row 0: the root note the pitch pot is on, plus the master volume as a
// proportional bar filling the rest of the row.
static void drawRootRow(float rootHz, float volume) {
  char noteBuf[NOTE_NAME_BUF_SIZE];
  freqToNoteName(rootHz, noteBuf);

  const int y = rowY(0);
  s_display.setCursor(OLED_X_OFFSET, y);
  s_display.printf("%-3s", noteBuf);

  const int barX = OLED_X_OFFSET + OLED_VOL_BAR_X_PX;
  const int barW = OLED_VISIBLE_WIDTH - OLED_VOL_BAR_X_PX;
  s_display.drawRect(barX, y, barW, OLED_TEXT_HEIGHT_PX, SSD1306_WHITE);
  int fillW = (int)(volume * (barW - 2) + 0.5f);
  if (fillW > 0) {
    s_display.fillRect(barX + 1, y + 1, fillW, OLED_TEXT_HEIGHT_PX - 2, SSD1306_WHITE);
  }
}

// Row 1: how many voices are sounding and how they are pitched relative to
// the root — the summary that replaces the old per-oscillator rows, which
// stopped fitting the 40 px window well before MAX_VOICES.
static void drawVoiceRow() {
  s_display.setCursor(OLED_X_OFFSET, rowY(1));
  s_display.printf("V%d %s", NUM_VOICES, k_PitchModeCode);
}

// Row 2: the build switches — distortion flavor and voice 0's harmonic
// spread ("*" if the other voices use different ones).
static void drawSettingsRow() {
  s_display.setCursor(OLED_X_OFFSET, rowY(2));
  s_display.printf("D%d/S%d%s", k_DistortionCode, k_SpreadCode, k_SpreadSuffix);
}

static void drawFlatline() {
  // EKG-style flatline in place of the root/volume row while the synth is
  // silent.
  int lineY = rowY(0) + OLED_TEXT_HEIGHT_PX / 2;
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
  float rootHz;
  float volume;
  if (!snapshotStatus(&rootHz, &volume)) {
    return;
  }

  s_display.clearDisplay();
  drawTitle();

  if (volume > 0.0f) {
    drawRootRow(rootHz, volume);
  } else {
    drawFlatline();
  }
  drawVoiceRow();
  drawSettingsRow();

  s_display.display();
}
