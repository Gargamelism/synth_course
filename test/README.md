# Host-side unit tests

Tests for the hardware-independent DSP/control math (`oscillator.cpp`,
`controls_math.cpp`). No Arduino/ESP32 toolchain involved — just the
system `g++`, so they run in seconds on the dev machine.

```
./test/run.sh
```

`run.sh` discovers every `test_*.cpp` file automatically and compiles it
against the matching `../<name>.cpp` (e.g. `test_oscillator.cpp` pairs with
`../oscillator.cpp`) if one exists. To add a test, just drop in a new
`test_<name>.cpp` — no registration needed.

What's covered: the sine table, the phase-accumulator oscillator, mix/clip
behavior in `mixOscillators`, the ADC smoothing (EMA + hysteresis) in
`smoothValue`, and the exponential pitch-mapping curve in `mapPitchHz`.

What's *not* covered here, and can't be from the host: I2S output, Core-0/
Core-1 timing, ADC hardware behavior, and OLED rendering. Those still need
the manual on-target checklist in `plans/2026-08-19_initial_synth.md`.
