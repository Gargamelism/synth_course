// 3-oscillator sine synth: 3 volumes + 3 pitches via potentiometers,
// audio out over I2S to an external DAC, live status on a 0.96" OLED.
// See pins.h for wiring, oscillator/audio_task for the DSP + Core-0 audio
// path, controls for the Core-1 pot reading, display for the OLED.

#include "pins.h"
#include "oscillator.h"
#include "controls.h"
#include "audio_task.h"
#include "display.h"

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  initSineTable();
  controlsBegin();

  if (!displayBegin()) {
    Serial.println("SSD1306 not found at OLED_I2C_ADDR - check wiring/address");
  }

  audioTaskBegin();

  Serial.println("3-osc synth running");
}

void loop() {
  static uint32_t lastDisplayMs = 0;

  controlsUpdate();

  uint32_t now = millis();
  if (now - lastDisplayMs >= DISPLAY_REFRESH_MS) {
    lastDisplayMs = now;
    displayUpdate();
  }
}
