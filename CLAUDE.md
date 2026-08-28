# synth_course

3-oscillator sine synth on the ABRobot ESP32-C3 0.42" OLED dev board.

## Tech stack

- **Platform**: ABRobot ESP32-C3 0.42" OLED dev board (single-core RISC-V ESP32-C3, QFN32) — audio generation runs as a max-priority task pinned to Core 0; since the C3 has only one core, `loop()` (controls + display) shares that same core and only runs when FreeRTOS preempts the audio task. This board has a **built-in SSD1306 OLED hardwired to GPIO5 (SDA) / GPIO6 (SCL)**, GPIO18/19 soldered directly to the USB socket, and GPIO20/21 wired as UART0. GPIO8 is the onboard LED, GPIO9 the BOOT button, GPIO11-17 the internal flash. That leaves GPIO0-4, 7, 10 for general use — plus GPIO20/21, which this project reclaims by **requiring "USB CDC On Boot" enabled** in the Arduino IDE (console moves to native USB; `PIN_I2S_BCK` then lives on GPIO20).
- **Framework**: Arduino for ESP32 (`synth_course.ino` sketch entry point, `setup()`/`loop()`).
- **Language**: C++ (Arduino-style).
- **Audio out**: `ESP_I2S.h` (`I2SClass`) → external PCM5102 I2S DAC.
- **Display**: `Adafruit_SSD1306` + `Adafruit_GFX` over I2C (`Wire.h`) → the board's built-in SSD1306. The controller has a 128x64 buffer but the 0.42" glass only shows a 72x40 window at offset (30,12); `display.cpp` shifts every draw by `OLED_X_OFFSET`/`OLED_Y_OFFSET` since Adafruit_SSD1306 has no offset support.
- **Input**: Analog potentiometers — 3x volume + 2x pitch — read via 5 of the C3's 6 ADC pins (ADC1: GPIO0-4). The 6th, GPIO5 (the only ADC2 pin), is the OLED's SDA on this board, so oscillator 3 has no pitch pot and runs at a fixed `OSC3_FIXED_FREQ_HZ` (220 Hz / A3); its volume pot still works.
- **Liveness indicator**: the board's onboard LED (GPIO8, active-low) blinks at ~1Hz from `loop()`, independent of the OLED, so a frozen main loop is visible even when the synth is fully silent.

See `pins.h` for wiring/pin assignments and the header comment in `synth_course.ino` for the file-by-file breakdown.

## Limitations

- **Never flash/upload firmware to the connected board without the user's explicit go-ahead in that turn.** Compiling (`arduino-cli compile`) is fine on its own, but `arduino-cli upload`, `esptool ... write-flash`, or any other command that writes to the board's flash requires the user to confirm first, every time — a prior approval doesn't carry over to a later upload. This is enforced via `permissions.ask` in `.claude/settings.json`.
