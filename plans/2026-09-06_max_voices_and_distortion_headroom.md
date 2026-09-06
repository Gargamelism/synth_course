# Polyphonic voices with per-voice harmonics, and a distortion/headroom fix

## Context

Today the synth is a **single** oscillator (`pins.h:60` `#define NUM_OSCILLATORS 1`) summing 22 harmonics
per sample in a runtime loop (`oscillator.cpp` `Oscillator::nextSample()`), with one global harmonic spread
chosen by `#ifdef HARMONIC_SPREAD_*` in `oscillator.h`. We want many voices, each free to use a *different*
harmonic spread.

Measured before writing this plan (compiled `oscillator.cpp` / `distortion.cpp` with the project's own
toolchain, `~/Library/Arduino15/packages/esp32/tools/esp-rv32/2601`, `-Os -march=rv32imc_zicsr_zifencei`,
and counted the emitted instructions):

- The harmonic loop is 11 instructions ≈ **14 cycles per harmonic**, so ~330 cycles per voice per frame.
  Against the 160 MHz ÷ 44.1 kHz = **3628 cycle/frame** budget that caps the current design at ~10 voices,
  and past ~90% duty the max-priority audio task starves IDLE0 → `Task watchdog got triggered: IDLE0`.
- `applyDistortion` runs once per *output* frame, so every flavor costs under 1.5% of the budget.
  It is not what limits voice count. But `-Os` does emit a multi-cycle hardware `div a0,a0,a4` for the
  `/3` in `DISTORTION_SOFT_CLIP` (`distortion.cpp`).
- `SINE_TABLE_AMPLITUDE 9000` (`pins.h`) — its own comment says it "keeps 3-osc full-volume sum inside
  int16_t range". At 4+ voices `mixOscillators` hard-clips. This is **unintended** distortion and it blocks
  adding voices at all.

The fix that unlocks everything: the harmonic sum is a fixed periodic function of phase, so it can be
**precomputed into a composite wavetable** at startup. A voice then costs one lookup (~13 cycles instead of
~330), and — crucially for this request — a voice's harmonic spread becomes just *which table it points at*,
so per-voice spreads are free.

### Decisions confirmed with the user

- `MAX_VOICES = 32`.
- Pitch mode 1 is **diatonic**: key fixed at compile time; the pot's root snaps to the key; each voice adds
  a compile-time *scale-degree* offset, so chord quality emerges from the root's position
  (C→C E G major, D→D F A minor, B→B D F diminished). Quality is never stored.
- Pitch mode 2 is **unison detune**. Only these two modes.
- Per-voice harmonic spread comes from a **compile-time table in a header**.
- Volume: the single pot is **master**; each voice has a compile-time relative level in that same table.
- Aliasing is handled with **mipmapped wavetables** — one table per spread per octave, band-limited.
- Display: **summary rows** (root note + master bar / voice count / settings). Per-voice rows don't scale.
- Distortion fixes: mixer headroom, the soft-clip divide, and distortion's position relative to the mix.
  (Leaving the "all `DISTORTION_*` commented out" state alone — that's a deliberate build switch.)

---

## Part A — Wavetable core (`oscillator.h` / `oscillator.cpp`)

The load-bearing change. Everything else depends on it.

**Spreads become runtime-selectable.** Delete the `#ifdef HARMONIC_SPREAD_*` selector from `oscillator.h`.
The five `harmonicWeightTerm()` bodies already in `oscillator.cpp` stay, renamed per spread
(`weightNatural`, `weightOctave`, `weightOdd`, `weightEqual`, `weightViola`) and dispatched by a
`enum HarmonicSpread { SPREAD_NATURAL, …, SPREAD_COUNT }`. Dispatch happens only at table-build time, so
its cost is irrelevant.

**Mip pyramid.** 9 octave levels cover 80 Hz → Nyquist (`log2(22050/80) ≈ 8.1`). Level *L* is built with
only the harmonics whose frequency stays under Nyquist at that level's top frequency:

| level | base Hz | harmonics kept |
|---|---|---|
| 0 | 80 | 22 |
| 3 | 640 | 22 |
| 4 | 1280 | 17 |
| 5 | 2560 | 8 |
| 6 | 5120 | 4 |
| 7 | 10240 | 2 |
| 8 | 20480 | 1 |

```cpp
const int WAVETABLE_SIZE = 1024;          // 10-bit phase, vs today's 8-bit sine table
const int WAVETABLE_MIP_LEVELS = 9;
extern const int16_t *g_wavetable[SPREAD_COUNT][WAVETABLE_MIP_LEVELS];  // null = not built
void initWavetables();                     // allocates ONLY the spreads kVoices actually references
int  mipLevelForFreq(float freqHz);
```

Reuse the existing normalization idea from `initHarmonicWeights()` verbatim: sum the terms, then scale so
they total `VOLUME_Q15_ONE`, which bounds each table's peak to `SINE_TABLE_AMPLITUDE`. Keep
`initSineTable()`/`g_sineTable` — `initWavetables()` builds from it.

`Oscillator` gains a `const int16_t *table_` and a `spread_`; `setFrequency()` picks the mip level
alongside the phase increment (runs once per block per voice — ~4 cycles/frame amortized). `nextSample()`
collapses to index, load, advance phase.

**RAM:** 1024 entries × 2 B × 9 levels = 18 KB **per spread in use**. Two spreads ≈ 37 KB, all five ≈ 92 KB,
against ~320 KB usable. Lazy allocation in `initWavetables()` keeps the common case cheap; print
`ESP.getFreeHeap()` in `setup()` so regressions are visible.

**Known quality trade:** today each harmonic gets `(phase*n)>>24`, i.e. *finer* phase resolution than a
shared index would give. `WAVETABLE_SIZE 1024` (vs the 256-entry sine table) more than repays that.

**Cost after:** ~13 cycles/voice → 32 voices ≈ **13% CPU**, versus 107% (underrun) with the current loop
at just 11 voices.

## Part B — Voice table (`voices.h`, new)

One row per voice, all its settings together — the single place a user edits to change the instrument.

```cpp
struct VoiceConfig {
  int8_t  scaleDegree;  // VOICE_PITCH_DIATONIC: offset in SCALE DEGREES from the pot's root
  int16_t detuneCents;  // VOICE_PITCH_UNISON_DETUNE
  uint8_t spread;       // HarmonicSpread — this is the "different harmonics per voice" knob
  uint8_t level;        // relative weight, normalized at startup (see Part D)
};
static const VoiceConfig kVoices[] = { … };

#define MAX_VOICES 32
#define NUM_VOICES (sizeof(kVoices) / sizeof(kVoices[0]))
static_assert(NUM_VOICES <= MAX_VOICES, "kVoices exceeds MAX_VOICES");
```

Pick the mode the same way `distortion.h` already does — exactly one `#define`, only that path compiled:

```cpp
#define VOICE_PITCH_DIATONIC
// #define VOICE_PITCH_UNISON_DETUNE
```

**Rename `NUM_OSCILLATORS` → `NUM_VOICES`** across `pins.h`, `controls.h/.cpp`, `audio_task.cpp`,
`display.cpp`. Mechanical, but do it in its own commit so the substantive diffs stay readable.
`NUM_OSCILLATORS` and `OSC3_FIXED_FREQ_HZ` disappear from `pins.h`.

## Part C — Diatonic scale math (`scale.h` / `scale.cpp`, new)

Split out as its own translation unit for the same reason `controls_math.cpp` was: `test/run.sh` auto-pairs
`test_scale.cpp` → `../scale.cpp`, so this gets host unit tests for free. **Must stay Arduino-free.**

```cpp
#define SCALE_KEY_ROOT_MIDI 60   // C4
#define SCALE_MAJOR              // or SCALE_MINOR
static const int8_t kScaleSemitones[] = {0,2,4,5,7,9,11};  // major
const int SCALE_DEGREES_PER_OCTAVE = 7;

int   scaleDegreeToSemitone(int degree);   // floor-divides so negative degrees work
int   snapMidiToScaleDegree(float midi);   // pot's note -> nearest degree in key
float scaleDegreeToFreq(int degree);
```

`notes.h` already computes MIDI from Hz inside `freqToNoteName` (`12*log2f(f/440)+69`). **Extract that into
`freqToMidi()` / `midiToFreq()` in `notes.h` and have both `freqToNoteName` and `scale.cpp` call them** —
no second copy of the formula.

Voice frequencies are computed in `controlsUpdate()` (in `loop()`, off the audio path — float and `log2f`
are fine there) and written into `g_oscParams.freqHz[]` under the existing mutex. `controls.cpp`'s existing
`mapPitchHz` still maps the pot; its output is then snapped to the key.

`controls.cpp`'s `k_VolPins`/`k_PitchPins` static_asserts must be relaxed — one master volume pot and one
pitch pot now serve all voices, so they no longer scale with voice count.

## Part D — Distortion and headroom (`distortion.h` / `distortion.cpp`, `pins.h`)

Three fixes, matching the file's existing "one `#define`, only that path compiled" convention.

**1. Headroom — the real bug.** Normalize `kVoices[].level` to sum to `VOLUME_Q15_ONE` at startup, reusing
the exact pattern `initHarmonicWeights()` already uses. The full-volume mix then peaks at
`SINE_TABLE_AMPLITUDE` *regardless of voice count*, so clipping in `mixOscillators` becomes impossible
rather than merely unlikely. That frees `SINE_TABLE_AMPLITUDE` to rise from 9000 to **32000**, a ~11 dB SNR
gain. Its `pins.h` comment ("keeps 3-osc full-volume sum inside int16_t range") must be rewritten — it will
no longer be true or relevant.

*Trade-off worth stating up front:* normalization is worst-case (all voices in phase). A wide chord at 32
voices will therefore sound quieter than 32 voices in unison. That is the price of guaranteeing no
unintended clipping, which is what this fix is for.

**2. Distortion stage becomes selectable.**

```cpp
#define DISTORTION_STAGE_PRE_MIX    // per voice, before mixing — default
// #define DISTORTION_STAGE_POST_MIX  // on the sum — today's behaviour
```

Pre-mix is the fix the user asked for: each voice is distorted at full scale, so `DISTORTION_DRIVE_GAIN`
means what it says, and voices don't intermodulate through the shared clipper. `audio_task.cpp` moves the
`applyDistortion` call inside the per-voice loop under that `#ifdef`. Cost at 32 voices: soft clip
~15 cycles × 32 ≈ 480 cycles ≈ 13% CPU — affordable *only* because of Part A. Total stays ≈ 26%.

**3. Soft-clip divide.** Replace `x3 / kCubicDivisor` with a Q16 reciprocal multiply
(`(x3 * 21846) >> 16`, error < 1e-4): ~3 cycles instead of ~20+. Preserves `y = x - x³/3` exactly enough
that the comment's "monotonic and self-bounded, no final clamp needed" claim still holds — changing the
divisor to a power of two would not.

## Part E — Display (`display.cpp`)

Replace `drawOscillatorRows` with three summary rows. Delete `anyOscillatorActive`'s per-voice array scan in
favour of a master-volume check; keep `drawFlatline` for the silent case.

```
row 0 (y=0)   "SI SY"                 (existing drawTitle)
row 1 (y=12)  root note + master volume bar   (reuses freqToNoteName + the existing drawRect/fillRect bar)
row 2 (y=21)  "V32 DIA"  active voice count + pitch mode
row 3 (y=30)  "D2/S5*"  settings                (existing drawSettingsRow)
```

Rows are 12 chars wide (72 px / 6 px at text size 1), so both rows have slack.

This also fixes a latent bug found while planning: `drawSettingsRow` computes
`rowY = … + NUM_OSCILLATORS * OLED_OSC_ROW_SPACING_PX`, which at `NUM_OSCILLATORS 3` lands at y=39 and is
clipped off the 40 px window. Fixed rows remove the dependency entirely.

`k_HarmonicsCode`'s `#if/#elif` chain (`display.cpp:30-40`) loses its macros when Part A removes the global
spread selector. Replace it with the runtime spread code of voice 0, printed as **`S%d`** — the harmonics
field was always the spread code, so it is renamed `H` → `S` rather than duplicated:

```cpp
s_display.printf("D%d/S%d%s", k_DistortionCode, (int)kVoices[0].spread + 1,
                 k_SpreadsMixed ? "*" : "");
```

`+ 1` keeps today's 1-based codes (natural=1 … viola=5) so the on-screen numbers don't shift meaning.

**Voices beyond voice 0.** `S%d` alone would report voice 0 and silently misrepresent the rest, so two
fields cover what the other voices are doing. A full per-voice list is not an option — 32 rows don't fit
40 px — and the per-voice degrees/detunes/levels stay where they are edited, in `voices.h`.

*Mixed-spread marker (`*`).* Appended when any voice uses a spread other than voice 0's. Derived from
`kVoices` at compile time, so it costs one char on screen and nothing at runtime:

```cpp
static constexpr bool spreadsMixed() {
  for (size_t i = 1; i < NUM_VOICES; i++) {
    if (kVoices[i].spread != kVoices[0].spread) return true;
  }
  return false;
}
static constexpr bool k_SpreadsMixed = spreadsMixed();
```

*Pitch mode on row 2.* The single biggest thing about the extra voices — whether they are a chord or a
detuned unison — comes straight from the `VOICE_PITCH_*` `#define` in Part B, printed after the count:

```cpp
#if defined(VOICE_PITCH_DIATONIC)
static const char *k_PitchModeCode = "DIA";
#elif defined(VOICE_PITCH_UNISON_DETUNE)
static const char *k_PitchModeCode = "UNI";
#endif
…
s_display.printf("V%d %s", (int)NUM_VOICES, k_PitchModeCode);
```

No `#else`, same as the distortion chain: a missing or doubled mode macro must fail the build. `V32 DIA`
is 7 of the 12 chars.

The old chain had **no `#else`**, so a missing macro failed the build rather than silently misreporting.
Preserve that property with the `static_assert` on `SPREAD_COUNT` from Part A plus a range check on
`kVoices[0].spread`, so an out-of-range spread is a compile error, not a wrong digit.

`snapshotOscParams` still copies `NUM_VOICES` floats under the mutex — at 32 voices that's 256 bytes on the
`loop()` stack, which is fine, but it only needs voice 0's frequency and the master volume now. Narrow it.

## Out of scope (stated, not silently skipped)

- **`web_sim/`** already lags the firmware badly — no harmonics, no distortion, no Q15 volume, no audio
  switch, and it hardcodes 3 oscillators. Resyncing it is a real project, not a rider on this one.
- **`scripts/harmonics_demo.py`** duplicates `harmonicWeightTerm` and predates the viola spread
  (it would `raise ValueError("viola")`). One-line fix, but separate.

---

## Verification

Host tests first — `test/run.sh` auto-discovers `test_*.cpp` and links `../<name>.cpp`, so new files need no
registration. `oscillator.cpp` and `scale.cpp` must stay free of Arduino headers for this to work.

| Check | How | Expected |
|---|---|---|
| **Existing oracle updated** | `test/test_oscillator.cpp` re-implements `nextSample()`'s harmonic loop verbatim as its expected value — it **will fail** until rewritten against the wavetable model. Rewrite it, don't delete it. | passes |
| Band-limiting | new `test_oscillator.cpp` case: for each spread and mip level, assert every non-zero harmonic weight is under Nyquist for that level's top frequency | no aliasing partials |
| Headroom invariant | sweep a full period of every built table | `|sample| <= SINE_TABLE_AMPLITUDE` (extends the check already at the end of `test_oscillator.cpp`) |
| No mixer clipping | `mixOscillators` over 32 voices at normalized levels and full master | never returns `INT16_MAX`/`INT16_MIN` |
| Mip selection | `mipLevelForFreq` at 80 / 640 / 1280 / 21000 Hz | 0 / 3 / 4 / 8 |
| **Diatonic chord** | new `test_scale.cpp`, using the confirmed table as the fixture: C→C E G, D→D F A, E→E G B, F→F A C, G→G B D, A→A C E, B→B D F | exact match |
| Negative//wrapping degrees | `scaleDegreeToSemitone(-1)` and `(7)` | `-1` (B below) and `12` |
| `notes.h` refactor | existing `test_notes.cpp` (A4/A3/A5, C4, A#4, the 452.9 Hz boundary, `FREQ_MIN/MAX` → D#2/B5) | still passes unchanged |
| Soft-clip reciprocal | new `test_distortion.cpp` (auto-links `../distortion.cpp`): compare against `x - x*x*x/3` in float | within 1 LSB |
| Compiles | `arduino-cli compile` | clean, and check the reported RAM against the 18 KB/spread estimate |
| No `div` remains | `riscv32-esp-elf-g++ -Os -S distortion.cpp -DDISTORTION_SOFT_CLIP`, grep the output | no `div` instruction |

On-target checks (need a flash — **ask before uploading**, per `CLAUDE.md` and `.claude/settings.json`'s
`permissions.ask`):

| Check | Expected |
|---|---|
| Free heap printed in `setup()` | comfortably above the wavetable allocation |
| Audio-task stack | `uxTaskGetStackHighWaterMark` — the 4096 B stack now also holds `float freq[32]` + two `int16_t[32]`; confirm margin or raise it |
| Liveness LED still blinks at ~1 Hz, OLED still refreshes | `loop()` is not starved — the ~26% duty prediction holds |
| No `Task watchdog got triggered: IDLE0` over several minutes at 32 voices | audio task is well under budget |
| Sweep the pitch pot through the full range | chord quality changes with the root; no aliasing whine on high voices; no crackle at full master volume |
