#!/usr/bin/env bash
# Host-side unit tests for the hardware-independent DSP/control math.
# Discovers every test/test_*.cpp, compiles it against the matching
# ../<name>.cpp (test_oscillator.cpp -> ../oscillator.cpp) if one exists,
# and runs it with the system g++/clang — no ESP32 toolchain needed, and no
# manifest to update when a new test file is added. See test/README.md.
set -euo pipefail
cd "$(dirname "$0")"

build_dir="build"
mkdir -p "$build_dir"

status=0

for test_file in test_*.cpp; do
  name="${test_file%.cpp}"
  src_file="../${name#test_}.cpp"
  sources=("$test_file")
  [ -f "$src_file" ] && sources+=("$src_file")

  if ! g++ -std=c++17 -Wall -Wextra -O1 -I.. -o "$build_dir/$name" "${sources[@]}"; then
    echo "BUILD FAILED: $name"
    status=1
    continue
  fi
  if ! "./$build_dir/$name"; then
    status=1
  fi
done

exit $status
