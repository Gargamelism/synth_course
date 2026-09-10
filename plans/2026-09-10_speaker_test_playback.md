# Repo reorg + standalone player sketch: play an embedded audio clip

## Context

The user wants to check the physical speaker/MAX98357A amp's ability and safe volume level using a real audio clip (not a synthesized tone), via a program separate from the main synth firmware. Confirmed with the user:
- **Test material**: an uploaded WAV or MP3 file, not auto-cycled tones or a sweep.
- **Level control**: reuse the existing master volume pot (`PIN_POT_VOL1` in `pins.h`) to sweep amplitude live while the clip plays, so the user can find the max clean level by ear.
- **Repo shape**: each Arduino program gets its own base folder — `./synth` (the existing synth firmware) and `./player` (the new clip-player) — rather than adding the new sketch next to the existing files at repo root.

This must be a **separate Arduino sketch** from the synth: a folder can only hold one `.ino` (and its `.ino` must match the folder name), so it can't live alongside the synth's sources. It also should not depend on the synth's headers: it's a standalone diagnostic tool, and pulling in the polyphonic oscillator/mixer would confound "is the speaker distorting" with "is the wavetable distorting."

**MP3 decision**: decoding MP3 on-device needs a real decoder library (flash/RAM footprint, extra dependency) for no benefit over converting once on a computer. Plan is WAV-only on the device; MP3 input gets converted to WAV as a documented one-line `ffmpeg` step before running the header-generator script. This keeps the sketch itself trivial.

## Repo reorg

Today `synth_course.ino` and all its sources sit at the repo root. Arduino requires a sketch's main `.ino` to share its folder's name, so moving it under `synth/` means renaming it too:

```
git mv synth_course.ino synth/synth.ino
git mv audio_task.cpp audio_task.h controls.cpp controls.h controls_math.cpp controls_math.h \
       display.cpp display.h distortion.cpp distortion.h notes.h oscillator.cpp oscillator.h \
       pins.h scale.cpp scale.h voices.h synth/
git mv test synth/test
```

`test/run.sh` resolves sources as `../<name>.cpp` relative to `test/` — moving `test/` as a unit under `synth/` keeps that relative path correct with no script changes.

`scripts/harmonics_demo.py` (a Python mirror of `oscillator.cpp`, not itself compiled/included by any sketch) and `web_sim/` (a browser port, already decoupled) don't need to move — nothing breaks if they stay at repo root, so leave them to avoid unrelated churn.

Docs that name `synth_course.ino` or describe the root layout need updating to match: `README.md`'s Setup section (`Open synth_course.ino` → `Open synth/synth.ino`) and `CLAUDE.md`'s file-by-file breakdown reference. `.claude/settings.json`'s `Bash(arduino-cli upload*)`/`Bash(esptool*)` ask-rules are plain prefix matches, unaffected by the path change.

## File layout

```
synth/                (existing firmware, moved + renamed — see reorg above)
  synth.ino
  audio_task.cpp/h, controls.cpp/h, controls_math.cpp/h, display.cpp/h,
  distortion.cpp/h, notes.h, oscillator.cpp/h, pins.h, scale.cpp/h, voices.h
  test/                (moved as a unit, unchanged internally)
player/                (new)
  player.ino           sketch entry point: setup()/loop()
  wav_to_header.py      WAV -> clip.h converter
  clip.h               generated — PROGMEM PCM16 sample array + metadata (gitignored, see below)
```

`clip.h` is generated from the user's own audio file and will vary per person/run — add `player/clip.h` to `.gitignore` rather than committing someone's test clip to the repo. The sketch's top comment documents the generation step in place of a committed sample.

## Audio pipeline

1. **Input prep** (user, one-time, outside the repo): convert whatever file they have to 16-bit PCM mono WAV, e.g.
   ```
   ffmpeg -i input.mp3 -ac 1 -ar 22050 -sample_fmt s16 clip.wav
   ```
   22050 Hz mono keeps a several-second clip well under typical ESP32-C3 flash budgets (4MB modules, ~1.3–3MB usable for the sketch depending on partition scheme) while sounding fine for a speaker-quality check.

2. **`player/wav_to_header.py`**: reads the WAV via Python's stdlib `wave` module (no new dependency), asserts it's 16-bit PCM mono (fails with a clear message pointing at the `ffmpeg` command above otherwise — no on-device resampling/format conversion), and writes `player/clip.h`:
   ```c
   #pragma once
   #include <stdint.h>
   #include <pgmspace.h>

   const uint32_t kClipSampleRateHz = <from WAV header>;
   const uint32_t kClipSampleCount  = <n>;
   const int16_t kClipSamples[] PROGMEM = { ... };
   ```
   Script prints the resulting `clip.h` file size so the user can sanity-check it against flash before compiling.

## `player.ino`

- Duplicates just the handful of constants it needs from `synth/pins.h` locally (`PIN_I2S_BCK`/`PIN_I2S_LRCK`/`PIN_I2S_DOUT`, `PIN_POT_VOL1`, `ADC_RESOLUTION_BITS`) rather than including it — keeps this sketch fully self-contained and openable on its own in the Arduino IDE, in its own folder. A comment notes these must be kept in sync with `synth/pins.h` by hand if the wiring ever changes.
- `#include "clip.h"`.
- `setup()`: `Serial.begin(115200)`; brief delay for USB CDC enumeration (same board quirk as the main sketch); `I2SClass::begin(I2S_MODE_STD, kClipSampleRateHz, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)`; print clip length/sample rate.
- `loop()`, no separate FreeRTOS task needed (no polyphony to protect here, unlike the main firmware):
  1. Read `PIN_POT_VOL1` (oversample + light EMA smoothing, same shape as `controls.cpp`'s pot handling) → gain fraction 0.0–1.0.
  2. Build one block of interleaved stereo `int16_t` samples from the next chunk of `kClipSamples`, each scaled by the gain (integer Q15 multiply, same pattern as `mixOscillators`/`audio_task.cpp`), duplicated to L/R.
  3. Blocking `i2s.write(...)` — paces the loop like the main firmware's audio task.
  4. Wrap the clip index back to 0 at the end for continuous looping, so the user can keep adjusting the pot without replaying manually.
  5. Throttled `Serial.print` of the current volume percentage (e.g. every ~500ms) as objective level feedback while turning the pot.

## Build/upload

- Compiles as its own sketch: `arduino-cli compile --fqbn esp32:esp32:esp32c3 player/`.
- The synth continues to compile the same way from its new location: `arduino-cli compile --fqbn esp32:esp32:esp32c3 synth/`.
- Per project rules, uploading to the board needs the user's explicit go-ahead each time — compiling is fine on its own.

## Verification

1. Generate `clip.h` from a short test WAV, compile, confirm no errors/warnings.
2. On hardware (after explicit upload go-ahead): confirm the clip loops continuously, the pot sweeps level from silent to full-scale, and Serial reports match the pot position.
3. Listen across the pot's range for speaker rattle/distortion to find the practical max clean level — the actual point of this tool.
