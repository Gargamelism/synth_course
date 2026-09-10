# Host-side unit tests

Tests for the hardware-independent DSP/control math (`oscillator.cpp`,
`scale.cpp`, `controls_math.cpp`, `distortion.cpp`). No Arduino/ESP32 toolchain involved — just the
system `g++`, so they run in seconds on the dev machine.

```
./test/run.sh
```

`run.sh` discovers every `test_*.cpp` file automatically and compiles it
against the matching `../<name>.cpp` (e.g. `test_oscillator.cpp` pairs with
`../oscillator.cpp`) if one exists. To add a test, just drop in a new
`test_<name>.cpp` — no registration needed.

What's covered: the sine table, the composite wavetables (band-limiting per
mip level, mip selection, the peak-amplitude invariant), the wavetable
oscillator, voice-level normalization and mix/clip behavior in
`mixOscillators`, the diatonic scale math in `scale.cpp`, the soft-clip
curve's Q16 reciprocal, the ADC smoothing (EMA + hysteresis) in
`smoothValue`, and the exponential pitch-mapping curve in `mapPitchHz`.

`test_softclip.cpp` is named for what it tests rather than a source file:
`distortion.cpp` compiles only the flavor its macro selects, so the test
picks the flavor and `#include`s the translation unit directly (which also
stops `run.sh` auto-linking a second, passthrough copy of it).

What's *not* covered here, and can't be from the host: I2S output, Core-0/
Core-1 timing, ADC hardware behavior, and OLED rendering. Those still need
the manual on-target checklist in `plans/2026-08-19_initial_synth.md`.
