#!/usr/bin/env bash
# Build the desktop VST3/standalone for the host OS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${BUILD_DIR:-$ROOT/vst3/build}"

CMAKE_ARGS=(-S "$ROOT/vst3" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release)
if [ -n "${JUCE_DIR:-}" ]; then
    CMAKE_ARGS+=("-DJUCE_DIR=$JUCE_DIR")
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo
echo "VST3 bundle: $BUILD/Polivoks1982_artefacts/Release/VST3/Polivoks 1982.vst3"
echo "Standalone:  $BUILD/Polivoks1982_artefacts/Release/Standalone/Polivoks 1982"
