#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "controls.h"
#include "notes.h"

static Adafruit_SSD1306 s_display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static const char *k_Title = "3-OSC SYNTH";

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

  if (xSemaphoreTake(g_paramsMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    return; // skip this refresh rather than block
  }
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    freq[oscIndex] = g_oscParams.freqHz[oscIndex];
    vol[oscIndex] = g_oscParams.volume[oscIndex];
  }
  xSemaphoreGive(g_paramsMutex);

  s_display.clearDisplay();
  s_display.setCursor(0, 0);
  s_display.println(k_Title);

  char noteBuf[NOTE_NAME_BUF_SIZE];
  for (int oscIndex = 0; oscIndex < NUM_OSCILLATORS; oscIndex++) {
    freqToNoteName(freq[oscIndex], noteBuf);
    s_display.setCursor(0, OLED_ROW_HEIGHT_PX + oscIndex * OLED_ROW_HEIGHT_PX);
    s_display.printf("O%d %4dHz %-3s V:%3d%%",
                      oscIndex + 1, (int)freq[oscIndex], noteBuf, (int)(vol[oscIndex] * 100.0f));
  }

  s_display.display();
}
