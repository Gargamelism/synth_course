# ESP32 3-Oscillator Sine Synth with OLED Status Display

## Context

`/Users/gargamel/dev/synth_course` is currently empty (only a `.claude/settings.local.json` with pre-approved permissions for `arduino-cli`, `esptool`, `pip3`, `brew`). The goal is to build, from scratch, working ESP32 firmware for a small hardware synthesizer: 3 sine oscillators, each with its own volume and pitch controlled by a physical potentiometer, mixed and sent to an external I2S DAC for real audio output, with a 0.96" I2C OLED (SSD1306, 128x64) showing each oscillator's live pitch and volume.

Confirmed with the user: ESP32 dev board, external I2S DAC module (PCM5102-style breakout) rather than the ESP32's built-in DAC, 6 potentiometers (3 volume + 3 pitch) for control, sine-only oscillators, and `arduino-cli` (not PlatformIO/Arduino IDE) as the toolchain — since that's what the project's existing permissions already anticipate.

Verified on this machine before writing this plan: `arduino-cli` 1.5.1 is already installed via Homebrew. `arduino-cli core list` shows only `arduino:avr` — the ESP32 core is **not yet installed**. `arduino-cli board list` shows no ESP32 connected (only virtual Bluetooth/debug ports) — nothing is plugged in yet. So this is a genuine from-scratch setup, not just firmware on top of an already-working toolchain.

## Part A — Toolchain Setup & Verification (do this first, before writing any firmware)

1. `arduino-cli version` — already OK (1.5.1).
2. Register the ESP32 board index and install the core:
   ```
   arduino-cli config init   # only if ~/Library/Arduino15/arduino-cli.yaml doesn't already exist
   arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32
   ```
3. Install OLED libraries (I2S needs no separate library — `ESP_I2S.h` ships inside the `esp32:esp32` core):
   ```
   arduino-cli lib update-index
   arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"
   ```
4. **Verification checklist** — run each and confirm before moving to Part B/C:
   | Check | Command | Expected |
   |---|---|---|
   | CLI present | `arduino-cli version` | version string |
   | ESP32 core installed | `arduino-cli core list` | `esp32:esp32` row, v3.x |
   | FQBN valid | `arduino-cli board listall esp32` | `esp32:esp32:esp32` ("ESP32 Dev Module") listed |
   | I2S API present | `find ~/Library/Arduino15/packages/esp32 -name ESP_I2S.h` | path found |
   | OLED libs installed | `arduino-cli lib list` | Adafruit SSD1306, GFX, BusIO (auto dep) |
   | Board enumerates | plug in ESP32, `arduino-cli board list` | new `/dev/cu.*` port, FQBN guess `esp32:esp32:esp32` |

   If the board doesn't enumerate when plugged in, it's almost always a missing USB-UART driver (CP2102 → Silicon Labs VCP driver; CH340/CH9102 → WCH driver) — install and re-check.

## Part B — Wiring

**I2S DAC (PCM5102 breakout):** BCK→GPIO26, LRCK/WSEL→GPIO25, DIN→GPIO27, SCK→GND (use internal oscillator), VIN→3V3, GND→GND (FMT/XSMT/DEMP are normally pre-strapped correctly on the breakout — verify against its silkscreen).

**OLED (SSD1306 I2C):** SDA→GPIO21, SCL→GPIO22, VCC→3V3, GND→GND. Default address `0x3C` (fallback `0x3D` if `display.begin()` fails).

**6 potentiometers** (10k linear, outer legs to 3V3/GND, wiper to ADC pin — all ADC1-only pins, so no WiFi/ADC2 conflict, and none are strapping/UART0 pins):
| Control | GPIO |
|---|---|
| Osc1 Volume | 32 |
| Osc2 Volume | 33 |
| Osc3 Volume | 34 |
| Osc1 Pitch | 35 |
| Osc2 Pitch | 36 (VP) |
| Osc3 Pitch | 39 (VN) |

Note: on generic ESP32-DevKitC/WROOM-32 boards GPIO32/33 are free (no populated RTC crystal) — visually confirm on the specific board before wiring. All parts run off the ESP32's 3V3 rail; share a common ground across ESP32/DAC/OLED/pots.

## Part C — Firmware

### Files (new sketch at repo root, since arduino-cli requires the sketch folder name == `.ino` name, and the folder is already `synth_course`)
- `synth_course.ino` — `setup()`/`loop()` orchestration only
- `pins.h` — all GPIO assignments + audio constants in one place
- `oscillator.h/.cpp` — sine lookup table, phase-accumulator oscillator, mixing
- `audio_task.h/.cpp` — I2S init + FreeRTOS Core-0 audio task
- `controls.h/.cpp` — ADC read/smoothing/mapping, shared param struct + mutex
- `display.h/.cpp` — SSD1306 init + status render
- `notes.h` — freq→MIDI note-name lookup table for the OLED

### Architecture
- **Core 0**: dedicated FreeRTOS task (`xTaskCreatePinnedToCore`, priority `configMAX_PRIORITIES-1`) does only: compute next sample block for all 3 oscillators → mix → `I2S.write()`. The blocking `I2S.write()` call itself paces real-time audio — no `delay()` needed.
- **Core 1**: standard Arduino `loop()` reads the 6 pots (oversampled + EMA-smoothed + hysteresis to kill ADC jitter), maps them to freq/volume, updates a shared `OscParams{float freqHz[3]; float volume[3];}` struct guarded by a mutex (audio task takes it with a 0-timeout `xSemaphoreTake` once per block, so it's never stalled by the control loop), and refreshes the OLED every ~100ms.
- **Oscillator**: 256-entry `int16_t` sine table (amplitude ±9000, generated once via `sinf()` in `setup()`), 32-bit phase accumulator per oscillator, `phaseInc = freqHz * (2^32 / 44100)`, table index = top 8 bits of the accumulator (standard DDS). Amplitude cap at ±9000 means 3 oscillators summed at full volume (27000) stay inside `int16_t` range without a runtime clamp, though a defensive `constrain()` is added anyway.
- **ADC→pitch mapping**: exponential across ~80–1000 Hz (`freqHz = 80 * (1000/80)^adcNorm`) — musically natural, not linear. **ADC→volume mapping**: linear 0.0–1.0.
- **OLED layout** (Adafruit_SSD1306 + Adafruit_GFX — chosen over U8g2 since this only needs plain text, and Adafruit's API/footprint is the simplest fit):
  ```
  3-OSC SYNTH
  O1 440Hz A4   V:78%
  O2 220Hz A3   V:12%
  O3 880Hz A5   V:100%
  ```
- Sample rate 44100 Hz, 16-bit stereo, mono oscillators duplicated to L/R (no panning needed).

### Staged bring-up (de-risk by isolating one unknown at a time)
1. Blink/Serial sanity check.
2. OLED-only test (`display.begin()` + text) — confirms I2C wiring.
3. I2S-only test (fixed 440Hz tone) — confirms DAC wiring before adding oscillator/control complexity.
4. ADC-only test (Serial-print all 6 raw pot values) — confirms wiring/pin mapping and noise floor.
5. Integrate into the final `synth_course.ino`.

## Part D — Build, Flash, Verify

```
arduino-cli compile --fqbn esp32:esp32:esp32 /Users/gargamel/dev/synth_course
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 /Users/gargamel/dev/synth_course
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
```
(replace the port with whatever `arduino-cli board list` reports once the board is plugged in)

### End-to-end verification
| Check | How |
|---|---|
| Compiles cleanly | `arduino-cli compile` exits 0 |
| Uploads successfully | `arduino-cli upload` reports success, no esptool sync errors |
| OLED boots & updates live | Status layout appears; turning each pot changes its Hz/note/% within ~100ms |
| Audio present, clean | Each oscillator produces a clean sine tone, no dropouts/pops at max combined volume |
| Pitch/volume tracks pots | Each pot audibly sweeps its oscillator's pitch (~80–1000Hz) or fades its volume independently of the other two |
| No audio glitching under load | Wiggling all 6 pots while audio plays causes no clicks/stutters (confirms the Core0/Core1 split works) |
| Serial debug sane | Monitor shows stable freq/vol readings, jitter-free when a pot is untouched |
