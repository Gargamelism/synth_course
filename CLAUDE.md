# synth_course

3-oscillator sine synth on ESP32-C3.

## Tech stack

- **Platform**: ESP32-C3 (single-core, RISC-V, QFN32) — audio generation runs as a max-priority task pinned to Core 0; since the C3 has only one core, `loop() `(controls + display) shares that same core and only runs when FreeRTOS preempts the audio task. Pin usage stays within GPIO0-10 to avoid the C3's dedicated USB-Serial/JTAG pins (GPIO18/19) and its internal-flash-reserved GPIO11-17.
- **Framework**: Arduino for ESP32 (`synth_course.ino` sketch entry point, `setup()`/`loop()`).
- **Language**: C++ (Arduino-style).
- **Audio out**: `ESP_I2S.h` (`I2SClass`) → external PCM5102 I2S DAC.
- **Display**: `Adafruit_SSD1306` + `Adafruit_GFX` over I2C (`Wire.h`) → 128x64 SSD1306 OLED.
- **Input**: Analog potentiometers (3x volume, 3x pitch) read via the C3's 6 ADC-capable pins — 5 on ADC1 (GPIO0-4) plus GPIO5 on ADC2 (safe since this project never enables WiFi).

See `pins.h` for wiring/pin assignments and the header comment in `synth_course.ino` for the file-by-file breakdown.
