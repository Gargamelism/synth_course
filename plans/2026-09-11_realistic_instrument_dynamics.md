# Realistic instrument dynamics: envelopes, brightness, vibrato, formants, noise, room

## Context

`harmonic_spreads.h` now gives each instrument patch a sourced *steady-state* overtone
balance, and each has an audition patch (GTR, PNO, CLR, FLT, TPT, TIB, HTP). They still sound
like organ stops, because every voice is a fixed wavetable looped forever at fixed brightness.
The ear identifies instruments mostly by how the sound *changes*: the attack, the decay, the
brightness falling as a pluck dies, breath noise, vibrato. None of that exists in the signal
path today: `audioTask()` (`audio_task.cpp`) reads pot-derived frequencies and a master volume
once per 256-frame block, runs one table lookup per voice per sample, mixes, optionally
distorts, and writes to I2S.

This plan adds those ingredients in phases, cheapest and most audible first, under two hard
constraints of the ESP32-C3 build:

- **CPU**: 160 MHz / 44.1 kHz = 3628 cycles per frame. 32 voices ≈ 416 cycles; with pre-mix
  distortion ≈ 26% of the budget (CLAUDE.md). Everything added per sample must be integer
  math (no FPU); float is fine once per block.
- **RAM**: see "Memory budget" below. The wavetables already take most of the heap.

## Memory budget (the constraint that shapes the plan)

From the current `arduino-cli compile` for `esp32c3`:

| item | bytes |
|---|---|
| dynamic memory ceiling reported by the core | 327,680 |
| static globals (`.data`+`.bss`, includes 2 KB `g_sineTable`, 1 KB SSD1306 buffer) | 19,432 |
| left for heap + stacks before runtime allocations | 308,248 |
| runtime, before wavetables: FreeRTOS, USB-CDC console, I2S DMA buffers, loop task stack (8 KB), audio task stack (4 KB) | est. 40–60 K |
| **wavetables today**: 11 referenced spreads × 9 mip levels × 2,048 B | **202,752** |
| estimated free heap after `setup()` | ~50–65 K |

The `setup()` free-heap print is the ground truth for the two estimates; read it on the next
upload before starting Phase 1. Everything in Phases 1–5 needs **no** heap; Phase 7 (reverb)
needs ~10 KB, and the optional forte/piano table pairs in Phase 2 would double wavetable RAM
and do **not** fit. So Phase 0 trims the pyramid first.

### Phase 0 — allocate only the mip levels a spread can reach (saves ~100 KB) ✅ implemented (commit 540a998)

`initWavetables()` (`oscillator.cpp`) builds all 9 levels (80 Hz → Nyquist) for every spread
any patch references. But the pitch pot tops out at `FREQ_MAX_HZ` = 1000 Hz, and a voice only
climbs above that by its scale-degree or cents offset:

| patch | highest voice offset | highest voice Hz at full pot | mip level needed |
|---|---|---|---|
| GTR … HTP, SOLO | degree 0 | ~1,060 (root snaps up to half a tone above 1000) | 3 |
| METL | 0 cents | ~1,060 | 3 |
| HARM | degree 13 = +23 semitones | ~4,000 | 5 |

Levels 6–8 are unreachable by any patch; levels 4–5 only by HARM's three spreads. Build per
spread only up to `maxLevel[spread]`, computed at the top of `initWavetables()` from
`kPatches`:

```
maxHz = FREQ_MAX_HZ * 2^((scaleDegreeToSemitone(degree) + 1.5) / 12)   // PITCH_DIATONIC; 1.5 st = snap + vibrato margin
maxHz = FREQ_MAX_HZ * 2^((detuneCents + 150) / 1200)                    // PITCH_UNISON_DETUNE
maxLevel = mipLevelForFreq(maxHz)
```

Result: 7 audition spreads + OCTAVES_WITH_ODDS at 4 levels (8 KB each), VIOLA/ODD/NATURAL at
6 levels (12 KB each): **100 KB instead of 198 KB**. `Oscillator::setFrequency()` already keeps
its previous table when the wanted level is null, so an out-of-range voice degrades to
slight aliasing, never a null read.

Make the budget an invariant, not a hope: add `WAVETABLE_RAM_BUDGET_BYTES` (e.g. 128 KB) to
`pins.h`, have `initWavetables()` compute the total it is about to allocate and print it with
the free heap in `setup()`, and add a host test in `test_oscillator.cpp` that recomputes the
same total from `kPatches` and `CHECK`s it against the budget. Adding a patch that blows the
budget then fails `run.sh`, not the board.

Optional further trim if Phase 2's table pairs are ever wanted: halve the table size per
level above 2 (1024, 1024, 1024, 512, 256 …) — level L keeps ~22/2^L harmonics so the lookup
error stays constant. Needs a per-table index shift in `Oscillator` (one extra field, same
per-sample cost). Brings the full 11-spread set to ~80 KB. Rejected: building tables into
flash with a generator script — 32 voices streaming 2 KB tables would exceed the C3's 16 KB
cache and stall the audio task on SPI-flash misses.

### Rules for every later phase

- New fixed-size DSP state (envelopes, filter, noise, reverb lines) is `static` at file scope,
  never `malloc` and never on the audio task's 4 KB stack. It then shows up in the compile's
  "Global variables" line and fails at link time if it does not fit.
- Per-voice state is sized `MAX_VOICES` (32) like `s_osc[]`.

## Phase 1 — note-on and amplitude envelope (largest gain; prerequisite for 2, 3, 5)

A plucked or struck sound cannot exist on a drone: something has to say "a note began".

**Trigger.** `PIN_AUDIO_SWITCH` (GPIO21) is already read every `controlsUpdate()` and published
as `OscParams::audioOn`, which today hard-mutes the mix. Reinterpret it as the gate: rising
edge = note on, falling edge = release. A second trigger source, per patch: re-trigger when the
pitch pot's snapped root degree changes (`snapMidiToScaleDegree`, `scale.cpp`, already produces
it), so sweeping the pot "re-plucks" each new note for guitar/piano. Publish a `uint8_t
noteOnCount` (increment per trigger) plus `bool gate` in `OscParams` — a counter, not an edge
flag, so a press shorter than one 5.8 ms block is never lost; the audio task compares it to
the last count it saw.

**Envelope.** New `envelope.h` / `envelope.cpp`, Arduino-free so `test/run.sh` picks it up:

```
struct EnvelopeConfig { uint16_t attackMs, decayMs, releaseMs; uint8_t sustainPercent; };
class Envelope { noteOn(); noteOff(); int16_t nextBlockGainQ15(int frames); bool idle(); }
```

State machine in float, advanced once per block (attack linear, decay/release exponential
toward the target — a multiply per block). The audio loop ramps from the previous block's gain
to the new one per sample (Q15 delta accumulated in an `int32_t`, one add and one shift per
frame) so there is no zipper noise. Applied once to the mixed sample, after distortion:
`mixed = (mixed * gainQ15) >> 15`. `masterQ15` keeps the volume pot; the `audioOn ? … : 0`
mute goes away because the envelope's release does the job.

**Per-patch values** — a new `EnvelopeConfig amp` field on `Patch` (`voices.h`):

| patch | A ms | D ms | S % | R ms | note |
|---|---|---|---|---|---|
| GTR | 3 | 700 | 0 | 150 | plucked: no sustain, long decay |
| PNO | 2 | 1200 | 0 | 120 | struck |
| CLR | 40 | 80 | 85 | 120 | reed speaks a little late |
| FLT | 60 | 120 | 80 | 150 | soft chiff-less start (chiff is Phase 5) |
| TPT | 25 | 60 | 90 | 100 | brass bite |
| TIB, HTP | 5 | 0 | 100 | 20 | organ: on/off |
| SOLO, HARM, METL | 5 | 0 | 100 | 20 | today's behaviour, click-free |

Memory: one `Envelope` (~24 B) — the patch's voices share it since they trigger together.
CPU: ~3 cycles/frame. Test: `test_envelope.cpp` — gain hits Q15 one after `attackMs`, settles
at sustain, releases to 0, and a re-trigger mid-release restarts from the current level.

## Phase 2 — brightness that decays with time and rises with loudness

A pluck loses its upper harmonics within a few hundred ms; brass gets brighter the louder it
plays (UNSW brass page: over a crescendo the fundamental rises 8 dB, the 9th harmonic 45 dB).

**Global one-pole low-pass on the mix** (`tone.h` / `tone.cpp`, host-testable):
`y += ((x - y) * kQ15) >> 15`, two integer ops per frame, no memory. `kQ15` is recomputed per
block from a cutoff in Hz (`k = 1 - exp(-2π fc / fs)`, float once per block):

```
cutoffHz = cutoffMinHz + (cutoffMaxHz - cutoffMinHz) * filterEnvGain * (0.3 + 0.7 * volumePot)
```

`filterEnv` is a second `Envelope` with its own per-patch `EnvelopeConfig` (fast decay for
GTR/PNO: A 1 / D 300 / S 20; slow rise for TPT: A 80 / S 100). Per-patch `cutoffMinHz`,
`cutoffMaxHz` fields on `Patch`. The `0.3 + 0.7·volume` term gives the loudness-brightness link
with the existing pot. The filter sits after the envelope and before the I2S write.

Rejected for now: per-voice crossfade between a forte and a piano wavetable per spread. It is
the more faithful model (the UNSW pages give the soft spectra) but it doubles table RAM
(Phase 0 + the half-size pyramid would bring it to ~160 KB, still too tight beside a reverb)
and costs a second lookup plus a lerp per voice per sample.

## Phase 3 — detuned strings and vibrato (mostly table edits)

- **Piano chorus**: a real piano has three strings per note, slightly out of tune; the beating
  is a large part of "piano". Change `kVoicesPiano` to three voices in `PITCH_UNISON_DETUNE`
  at −2, 0, +2 cents (`detuneCents`, `voices.h`; the mode exists, METL uses it). Same spread, so
  zero extra RAM; +2 voices of CPU.
- **Vibrato**: per-patch `{rateHz, depthCents, onsetMs}`. In `audioTask()`, once per block:
  `mult = 2^(depth(t) · sin(2π·rate·t) / 1200)` with `depth(t)` ramping from 0 over the
  200–400 ms after note-on (delayed onset is what reads as "played" rather than "modulated"),
  applied to every `freq[]` before `setFrequency()`. Float, ~200 cycles per 256-frame block —
  nothing. Block-rate pitch steps at 6 Hz vibrato are 29 steps per cycle and the phase
  accumulator stays continuous, so no clicks. Strings/winds 5–6 Hz, 15–30 cents; organ 0.
- **Flute tremolo**: amplitude wobble at the same rate, folded into the block's envelope target
  so Phase 1's per-sample ramp smooths it.

## Phase 4 — formants fixed in Hz, not harmonic number (zero runtime cost)

Every instrument model in `harmonic_spreads.h` pins its resonance to harmonic *numbers*, so the
trumpet's "formant" slides with pitch. The mip pyramid already builds one table per octave,
so give the weight function the harmonic's real frequency:

- `buildTable(table, spread, harmonicCount, levelCentreHz)` with
  `levelCentreHz = WAVETABLE_MIP_BASE_HZ · 2^(level + 0.5)`; `harmonicWeightTerm(spread, h,
  harmonicHz)` with `harmonicHz = (h+1) · levelCentreHz`.
- A `formant(hz, centreHz, widthOctaves)` helper (Gaussian in log-frequency) that the models
  multiply their source spectrum by. Sources, from the earlier research: trumpet 1.2–1.4 kHz,
  clarinet 1.5–1.7 kHz, flute ~800 Hz, oboe 1.4 kHz (HyperPhysics), violin-family body
  resonances near 275 Hz (air) and 450 Hz (top plate) (Fletcher & Rossing ch. 10). The
  trumpet table then simplifies to a gentle 1/√n source × the formant.
- `test_oscillator.cpp`'s all-spreads loop gains a frequency argument; `buildTable()` still
  normalises the weight sum, so headroom is unchanged and no RAM changes.

## Phase 5 — breath, bow and pick noise

A flute tone is roughly a third noise; bowed strings and reeds carry a little; a pick has a
20 ms burst. One noise generator on the mix path: 32-bit LCG (`x = x·1664525 + 1013904223`,
top 16 bits) through a one-pole low-pass, scaled by a per-patch `noisePercent` and by Phase 1's
envelope so it starts and stops with the note. ~12 cycles per frame, no memory. Values: FLT 12,
CLR 3, TPT 2, strings 2 (attack-only noise via the filter envelope is a later refinement).

## Phase 6 — random harmonic phases in the tables

`buildTable()` sums every harmonic at zero phase, which yields spiky waveforms: the worst
crest factor for headroom and the harshest input for the clipper. Giving each harmonic a fixed
pseudo-random start phase (`index = (i·(h+1) + phaseOffset[h]) & mask`; the unused
`#include <random>` in `oscillator.cpp` hints this was intended) leaves the steady timbre the
same to the ear, lowers the peak (the existing `≤ SINE_TABLE_AMPLITUDE` clamp and test still
hold, since peak ≤ sum of weights), and sounds less "synthy" through distortion. Two rules:
seed deterministically so builds and tests are reproducible, and use the **same** offset for a
given harmonic across all mip levels of a spread, otherwise a level switch mid-note clicks.

## Phase 7 — room

A short reverb adds more perceived realism than any spectral tweak once notes have envelopes.
Freeverb-style: 4 combs (1116, 1188, 1277, 1356 samples at 44.1 kHz) + 2 all-passes (556, 441),
`int16_t` static delay lines = **9,868 B**, ~80–120 cycles per frame, one wet-level per patch
(15–25 %). Sits last in the chain. Only start this after Phase 0 has freed the heap and the
`setup()` free-heap print confirms > 30 KB spare.

## Not achievable with single-cycle wavetables

- Piano inharmonicity (partial n at f·n·√(1+Bn²)): a wavetable is periodic by construction;
  per-partial oscillators would cost ~22× per voice. Phase 3's detune chorus is the stand-in.
- Bowed-string stick-slip and reed nonlinearity: physical modelling, a different engine.

## Suggested order and what each buys

0 ✅ (RAM trim + budget test) → 1 (note-on + ADSR) → 2 (brightness filter) → 3 (detune +
vibrato) → 4 (Hz formants) → 5 (noise) → 6 (phases) → 7 (reverb). After 1 and 2 the guitar
and piano patches stop being organ stops; 3 makes the piano a piano and the winds sound
played; 4–7 are polish.

## Verification

- Host: `bash synth/test/run.sh` — new `test_envelope.cpp`, `test_tone.cpp`, the wavetable
  budget check and the widened all-spreads check all pass (`test_scale.cpp`'s 22 failures are
  pre-existing and unrelated).
- Build: `arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc synth` after every
  phase; watch the "Global variables" line grow only by the static buffers listed above.
- Board (upload only on explicit go-ahead, each time): the `setup()` line prints wavetable
  bytes and free heap — record it after Phase 0 and after Phase 7. Listening: GTR with the
  gate switch, then sweep the pitch pot to hear re-triggering; TPT loud vs quiet for the
  brightness link; PNO for beating; FLT for breath.
