#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "controls.h"
#include "voices.h"
#include "notes.h"
#include "oscillator.h"

// The SSD1306 buffer is 128x64; this board only shows a 72x40 window at
// (OLED_X_OFFSET, OLED_Y_OFFSET), so every draw below is shifted by that.
static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static const char *k_Title = "SI SY";

static const char *pitchModeCode(PitchMode mode) {
  switch (mode) {
    case PITCH_DIATONIC: return "DIA";
    case PITCH_UNISON_DETUNE: return "UNI";
    default: return "?";
  }
}

// The active patch (kPatches[patchIndex], voices.h) plus the master
// volume/root note are all the mutex holds long enough to copy.
static bool snapshotStatus(float *rootHz, float *volume, uint8_t *patchIndex,
                           bool *distortionEnabled) {
  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    return false; // skip this refresh rather than block
  }
  *rootHz = g_oscParams.rootHz;
  *volume = g_oscParams.volume;
  *patchIndex = g_oscParams.patchIndex;
  *distortionEnabled = g_oscParams.distortionEnabled;
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

// Row 1: how many voices are sounding and how they are pitched relative to the root
static void drawVoiceRow(uint8_t patchIndex) {
  const Patch &patch = kPatches[patchIndex];
  s_display.setCursor(OLED_X_OFFSET, rowY(1));
  s_display.printf("V%d %s", patch.voiceCount, pitchModeCode(patch.pitchMode));
}

// Row 2: the active patch's label, its voice 0 harmonic spread ("*" if the
// other voices use different ones), and whether its matched distortion is
// currently on (the encoder's click toggles this; "D" means on).
static void drawSettingsRow(uint8_t patchIndex, bool distortionEnabled) {
  const Patch &patch = kPatches[patchIndex];
  const int spreadCode = (int)patch.voices[0].spread + 1; // 1-based (natural = 1 .. viola = 5)
  const char *spreadSuffix = patchSpreadsMixed(patch) ? "*" : "";
  s_display.setCursor(OLED_X_OFFSET, rowY(2));
  s_display.printf("%s S%d%s%s", patch.label, spreadCode, spreadSuffix,
                    distortionEnabled ? "D" : "");
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
  uint8_t patchIndex;
  bool distortionEnabled;
  if (!snapshotStatus(&rootHz, &volume, &patchIndex, &distortionEnabled)) {
    return;
  }

  s_display.clearDisplay();
  
  drawTitle();
  drawRootRow(rootHz, volume);
  drawVoiceRow(patchIndex);
  drawSettingsRow(patchIndex, distortionEnabled);

  s_display.display();
}
