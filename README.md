# synth_course

3-oscillator sine synth on ESP32: 3 volumes + 3 pitches via potentiometers, audio out over I2S to an external DAC, live status on a 0.96" OLED.

## Hardware

- ESP32 dev board
- PCM5102 I2S DAC breakout
- SSD1306 128x64 OLED (I2C)
- 6x potentiometers (3 volume, 3 pitch)

See `pins.h` for wiring/pin assignments.

## System requirements

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x (or `arduino-cli`)
- **ESP32 board support**: install via Boards Manager → search "esp32" → install the Espressif "esp32" package (this project uses the Arduino-ESP32 core v3.x, which provides `ESP_I2S.h`)
- **Board selection**: pick your specific ESP32 dev board model under Tools → Board
- **Libraries** (install via Library Manager):
  - `Adafruit SSD1306`
  - `Adafruit GFX Library`
  - (Wire and ESP_I2S ship with the ESP32 board package — no separate install needed)

## Setup

1. Install the Arduino IDE and add the ESP32 board package (File → Preferences → Additional Board Manager URLs → `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
2. Install the two Adafruit libraries listed above via Sketch → Include Library → Manage Libraries.
3. Open `synth_course.ino`, select your board and serial port under Tools, then Upload.
4. Open the Serial Monitor at 115200 baud to see startup logs.

## Wiring reference

See the pin definitions and constants in `pins.h`.
