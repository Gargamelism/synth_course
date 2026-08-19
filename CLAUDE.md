# synth_course

3-oscillator sine synth on ESP32.

## Tech stack

- **Platform**: ESP32, dual-core — audio generation runs as a task pinned to Core 0, `loop()` (controls + display) runs on Core 1.
- **Framework**: Arduino for ESP32 (`synth_course.ino` sketch entry point, `setup()`/`loop()`).
- **Language**: C++ (Arduino-style).
- **Audio out**: `ESP_I2S.h` (`I2SClass`) → external PCM5102 I2S DAC.
- **Display**: `Adafruit_SSD1306` + `Adafruit_GFX` over I2C (`Wire.h`) → 128x64 SSD1306 OLED.
- **Input**: Analog potentiometers (3x volume, 3x pitch) read via ESP32 ADC1 pins.

See `pins.h` for wiring/pin assignments and the header comment in `synth_course.ino` for the file-by-file breakdown.
