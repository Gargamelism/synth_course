# synth_course

3-oscillator sine synth on the ESP32-C3 SuperMini board.

## Tech stack

- **Platform**: ESP32-C3 SuperMini (single-core, RISC-V, QFN32) — audio generation runs as a max-priority task pinned to Core 0; since the C3 has only one core, `loop() `(controls + display) shares that same core and only runs when FreeRTOS preempts the audio task. GPIO8 is this board's onboard LED and GPIO9 is its BOOT button — both left free of other duties. GPIO11-17 are reserved for internal flash and GPIO18/19 are the dedicated USB-Serial/JTAG pins, so all other assignments avoid those too.
- **Framework**: Arduino for ESP32 (`synth_course.ino` sketch entry point, `setup()`/`loop()`).
- **Language**: C++ (Arduino-style).
- **Audio out**: `ESP_I2S.h` (`I2SClass`) → external PCM5102 I2S DAC.
- **Display**: `Adafruit_SSD1306` + `Adafruit_GFX` over I2C (`Wire.h`) → 128x64 SSD1306 OLED.
- **Input**: Analog potentiometers (3x volume, 3x pitch) read via the C3's 6 ADC-capable pins — 5 on ADC1 (GPIO0-4) plus GPIO5 on ADC2 (safe since this project never enables WiFi).
- **Liveness indicator**: the board's onboard LED (GPIO8, active-low) blinks at ~1Hz from `loop()`, independent of the OLED, so a frozen main loop is visible even when the synth is fully silent.

See `pins.h` for wiring/pin assignments and the header comment in `synth_course.ino` for the file-by-file breakdown.

## Limitations

- **Never flash/upload firmware to the connected board without the user's explicit go-ahead in that turn.** Compiling (`arduino-cli compile`) is fine on its own, but `arduino-cli upload`, `esptool ... write-flash`, or any other command that writes to the board's flash requires the user to confirm first, every time — a prior approval doesn't carry over to a later upload. This is enforced via `permissions.ask` in `.claude/settings.json`.
