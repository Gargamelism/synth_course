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

# A handful of host-testable .cpp files call into another one (e.g.
# oscillator.cpp -> scale.cpp's scaleDegreeToSemitone) — listed here rather
# than auto-linking every ../*.cpp, since most sources (audio_task.cpp,
# controls.cpp, display.cpp) pull in Arduino-only headers and can't build on
# the host at all. A plain function, not an associative array, since macOS's
# bundled bash (3.2) has no associative arrays.
extra_sources_for() {
  case "$1" in
    oscillator) echo "scale.cpp" ;;
  esac
}

for test_file in test_*.cpp; do
  name="${test_file%.cpp}"
  core_name="${name#test_}"
  src_file="../${core_name}.cpp"
  sources=("$test_file")
  [ -f "$src_file" ] && sources+=("$src_file")
  for extra in $(extra_sources_for "$core_name"); do
    sources+=("../$extra")
  done

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
