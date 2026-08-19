---
description: Review the current branch for embedded C++ best practices, with special focus on never obstructing real-time audio generation on Core 0
argument-hint: [base-branch]
allowed-tools: Bash(git diff:*), Bash(git log:*), Bash(git status:*), Bash(git branch:*), Read, Grep, Glob, ReportFindings
---

Review the current branch's changes for this ESP32 3-oscillator synth. Base branch to diff against: `$ARGUMENTS` (default to `main` if not given).

## 1. Determine scope
- Run `git status` and `git branch --show-current`.
- If the base branch/ref exists and differs from HEAD, use `git diff <base>...HEAD` (and `git log <base>..HEAD --oneline`) to scope the review to changed lines.
- If there's no meaningful diff (e.g. no commits yet, or already on the base branch), fall back to reviewing the full source tree: discover it with `Glob` (`*.ino`, `*.cpp`, `*.h`) rather than a fixed file list, so the review stays correct as files are added, renamed, or removed.

## 2. Real-time audio safety (highest priority — audio_task.cpp and oscillator.cpp hot paths)
Flag anything in the Core-0 audio task or per-sample oscillator code that could stall or delay audio output:
- Any blocking call other than the existing paced `s_i2s.write(...)` (e.g. `delay()`, blocking mutex/semaphore takes with nonzero/indefinite timeout, blocking I2C/SPI/Serial calls, `analogRead`).
- Heap allocation (`new`, `malloc`, `String` concatenation, STL containers that allocate) anywhere in `audioTask`, `Oscillator::nextSample`, or `mixOscillators`.
- New cross-core shared state that isn't protected by the established mutex pattern, or that changes the audio side's mutex timeout away from non-blocking (0-tick) takes.
- Added float math, branching, or per-sample work in the hot path beyond what's already there — call out the cost, don't just require pure-integer if the existing code already mixes float.
- Unbounded loops, recursion, or anything whose runtime isn't bounded and small relative to one audio block period (256 frames at the configured sample rate).

## 3. Cross-core concurrency correctness
- Any new global/static shared between Core 0 (audio) and Core 1 (controls/display) must go through a mutex or atomic, following the existing `g_paramsMutex` pattern (non-blocking take on the audio side, short-timeout take with skip-on-miss on the UI side).
- Watch for read/write races on anything *not* write-once-then-read-only (like `g_sineTable`, which is safe because it's only written once in `setup()`).

## 4. General embedded C++ practices
- No dynamic allocation, no exceptions/RTTI usage (unsupported/expensive on this target).
- Const-correctness; fixed-width types (`int16_t`, `uint32_t`, etc.) where size/overflow matters.
- No magic numbers — pins, sample rate, buffer sizes, thresholds belong in `pins.h`/`notes.h` alongside existing constants.
- Naming matches project convention: `g_` for cross-file globals, `s_` for file-local statics, camelCase functions.
- Header guards / `#pragma once` present on new headers; no missing includes relying on transitive includes.
- Integer overflow, signed/unsigned mismatches, and array bounds on any new indexing logic.

## 5. Report findings
Use the `ReportFindings` tool. Rank findings most-severe first, with anything that risks an audible glitch (blocking, allocation, or unbounded work in the Core-0 hot path, or an unprotected cross-core race) always ranked above general style/practice findings. If nothing survives review, call it with an empty findings array.
