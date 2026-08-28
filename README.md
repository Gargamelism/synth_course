# synth_course

3-oscillator sine synth on the ABRobot ESP32-C3 0.42" OLED board: 3 volumes + 2 pitches via potentiometers (oscillator 3 runs at a fixed pitch), audio out over I2S to an external DAC, live status on the built-in 0.42" OLED, and a blinking onboard LED as a liveness indicator.

## Hardware

- ABRobot ESP32-C3 0.42" OLED dev board — an ESP32-C3 with a **built-in** SSD1306 OLED (72x40 visible, I2C on GPIO5/6) and an onboard LED on GPIO8 used as a liveness indicator
- PCM5102 I2S DAC breakout
- 5x potentiometers (3 volume, 2 pitch)

> **Why 5 pots, not 6:** the ESP32-C3 has only 6 ADC-capable pins and this board's built-in OLED permanently uses one of them (GPIO5). That leaves 5 for pots, so oscillator 3 has no pitch knob — it plays a fixed 220 Hz (`OSC3_FIXED_FREQ_HZ`).

See `pins.h` for wiring/pin assignments.

## System requirements

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x (or `arduino-cli`)
- **ESP32 board support**: install via Boards Manager → search "esp32" → install the Espressif "esp32" package (this project uses the Arduino-ESP32 core v3.x, which provides `ESP_I2S.h`)
- **Board selection**: pick "ESP32C3 Dev Module" (FQBN `esp32:esp32:esp32c3`) under Tools → Board
- **Tools settings**: set **USB CDC On Boot → "Enabled"**. This board wires GPIO20/21 as UART0, and the firmware puts the I2S bit-clock on GPIO20 — so the serial console must move to native USB or boot diagnostics are lost
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
