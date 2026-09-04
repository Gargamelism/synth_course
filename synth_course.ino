// Single-oscillator sine synth: one volume + one pitch potentiometer,
// audio out over I2S to an external DAC, live status on the board's
// built-in 0.42" OLED.
// See pins.h for wiring, oscillator/audio_task for the DSP + Core-0 audio
// path, controls for the loop() pot reading, display for the OLED.

#include "pins.h"
#include "oscillator.h"
#include "controls.h"
#include "audio_task.h"
#include "display.h"

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  // This board needs "USB CDC On Boot" enabled (Arduino IDE -> Tools): it
  // routes the console over native USB and frees GPIO20/21 for I2S. The USB
  // port takes a moment to enumerate after reset — wait a beat so the
  // diagnostics below actually reach the Serial Monitor.
  delay(200);

  initSineTable();
  initHarmonicWeights();
  controlsBegin();
  pinMode(PIN_STATUS_LED, OUTPUT);

  if (!displayBegin()) {
    Serial.println("SSD1306 not found at OLED_I2C_ADDR - check wiring/address");
  }

  audioTaskBegin();

  Serial.println("3-osc synth running");
}

void loop() {
  static uint32_t lastDisplayMs = 0;

  controlsUpdate();

  // Liveness blink — toggles at ~1Hz so a frozen loop() is visually
  // obvious even while the synth is fully silent. Active-low LED.
  bool blinkOn = (millis() / BLINK_HALF_PERIOD_MS) % 2 == 0;
  digitalWrite(PIN_STATUS_LED, blinkOn ? LOW : HIGH);

  uint32_t now = millis();
  if (now - lastDisplayMs >= DISPLAY_REFRESH_MS) {
    lastDisplayMs = now;
    displayUpdate();
  }
}
