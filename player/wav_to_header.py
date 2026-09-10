#!/usr/bin/env python3
"""Converts a 16-bit PCM mono WAV file into clip.h for the player sketch to
embed and loop over I2S.

Input must already be 16-bit PCM mono — this script does no resampling or
format conversion. Convert first with, e.g.:
    ffmpeg -i input.mp3 -ac 1 -ar 22050 -sample_fmt s16 clip.wav

Usage: python3 wav_to_header.py clip.wav [-o output_path.h] [-s START_SECONDS]

If the clip (from --start onward) is longer than fits in flash, it's cut down
to the allowed length automatically — use --start to pick which part of a
longer file to embed (e.g. skip past a quiet intro).
"""
import argparse
import struct
import sys
import wave
from pathlib import Path

SAMPLES_PER_LINE = 16

# player.ino measures ~326000 bytes of flash before any clip data (see its
# compile output). The ESP32-C3 board here has a 1310720-byte app partition;
# staying comfortably under the remainder leaves headroom for the linker's
# other rodata. At 2 bytes/sample this caps a clip to ~20s at 22050 Hz mono
# (or proportionally longer at a lower sample rate).
MAX_SAMPLES = 450_000


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("wav_path", type=Path, help="16-bit PCM mono WAV file")
    parser.add_argument("-o", "--out", type=Path, default=None,
                         help="output header path (default: clip.h next to this script)")
    parser.add_argument("-s", "--start", type=float, default=0.0,
                         help="seconds to skip from the start of the file before cutting (default: 0)")
    args = parser.parse_args()

    out_path = args.out or Path(__file__).parent / "clip.h"

    with wave.open(str(args.wav_path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            sys.exit(
                f"{args.wav_path} is {wav.getnchannels()}ch / {wav.getsampwidth() * 8}-bit"
                " — need 16-bit mono PCM. Convert first, e.g.:\n"
                "  ffmpeg -i input.mp3 -ac 1 -ar 22050 -sample_fmt s16 clip.wav"
            )
        sample_rate = wav.getframerate()
        total_frames = wav.getnframes()

        start_frame = int(args.start * sample_rate)
        if start_frame >= total_frames:
            sys.exit(
                f"--start {args.start:.1f}s is at/past the end of {args.wav_path} "
                f"({total_frames / sample_rate:.1f}s long)"
            )
        wav.setpos(start_frame)

        take_frames = min(total_frames - start_frame, MAX_SAMPLES)
        frames = wav.readframes(take_frames)

    samples = struct.unpack(f"<{len(frames) // 2}h", frames)

    remaining_frames = total_frames - start_frame
    if remaining_frames > MAX_SAMPLES:
        max_seconds = MAX_SAMPLES / sample_rate
        print(
            f"note: {args.wav_path} has {remaining_frames / sample_rate:.1f}s available from "
            f"--start {args.start:.1f}s — cut down to the ~{max_seconds:.1f}s that fits flash "
            f"at {sample_rate} Hz. Pass --start to embed a different part instead."
        )

    lines = [
        ", ".join(str(s) for s in samples[i:i + SAMPLES_PER_LINE])
        for i in range(0, len(samples), SAMPLES_PER_LINE)
    ]

    out_path.write_text(
        "#pragma once\n"
        "#include <stdint.h>\n\n"
        f"const uint32_t kClipSampleRateHz = {sample_rate};\n"
        f"const uint32_t kClipSampleCount = {len(samples)};\n"
        "const int16_t kClipSamples[] = {\n  "
        + ",\n  ".join(lines)
        + "\n};\n"
    )

    size_kb = out_path.stat().st_size / 1024
    duration_s = len(samples) / sample_rate
    print(f"wrote {out_path} — {len(samples)} samples @ {sample_rate} Hz "
          f"({duration_s:.1f}s, {size_kb:.0f} KB)")


if __name__ == "__main__":
    main()
