# synth_course

Polyphonic wavetable synth on the ABRobot ESP32-C3 0.42" OLED board: a master volume pot and a pitch pot drive every voice in the `kVoices` table (`voices.h`), audio out over I2S to an external DAC, live status on the built-in 0.42" OLED, and a blinking onboard LED as a liveness indicator.

## Hardware

- ABRobot ESP32-C3 0.42" OLED dev board — an ESP32-C3 with a **built-in** SSD1306 OLED (72x40 visible, I2C on GPIO5/6) and an onboard LED on GPIO8 used as a liveness indicator
- PCM5102 I2S DAC breakout
- 2x potentiometers (master volume, pitch)

> **Two pots for any number of voices:** every voice is scaled by the one volume pot and pitched relative to the one pitch pot — as a diatonic chord whose quality follows the root, or as a detuned unison (`VOICE_PITCH_*` in `voices.h`). Add voices by adding rows to `kVoices`, not knobs.

See `pins.h` for wiring/pin assignments.

## System requirements

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x (or `arduino-cli`)
- **ESP32 board support**: install via Boards Manager → search "esp32" → install the Espressif "esp32" package (this project uses the Arduino-ESP32 core v3.x, which provides `ESP_I2S.h`)
- **Board selection**: pick "ESP32C3 Dev Module" (FQBN `esp32:esp32:esp32c3`) under Tools → Board
- **Tools settings**: set **USB CDC On Boot → "Enabled"**. This board wires GPIO20/21 as UART0, and the firmware puts the encoder switch on GPIO20 and the audio gate switch on GPIO21 — so the serial console must move to native USB or boot diagnostics are lost
- **Libraries** (install via Library Manager):
  - `Adafruit SSD1306`
  - `Adafruit GFX Library`
  - (Wire and ESP_I2S ship with the ESP32 board package — no separate install needed)

## Setup

1. Install the Arduino IDE and add the ESP32 board package (File → Preferences → Additional Board Manager URLs → `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
2. Install the two Adafruit libraries listed above via Sketch → Include Library → Manage Libraries.
3. Open `synth/synth.ino`, select your board and serial port under Tools, then Upload.
4. Open the Serial Monitor at 115200 baud to see startup logs.

## Wiring reference

See the pin definitions and constants in `synth/pins.h`.

## Speaker/amp test

`player/` is a separate, standalone sketch that loops a short audio clip of your own choosing over I2S, with the master volume pot controlling level live — useful for checking the physical speaker's max clean volume and response with real material. See `player/player.ino`'s top comment for the WAV-conversion + `wav_to_header.py` steps.
