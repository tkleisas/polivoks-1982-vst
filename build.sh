#!/usr/bin/env bash
# Build the MPC ARM plugin and the desktop VST2 from a pinned MPC Plugin SDK.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_REPO="${SDK_REPO:-https://github.com/inmusic-software/mpc-plugin-builder-sdk.git}"
SDK_COMMIT="${SDK_COMMIT:-febd2ca5bd9eb16c4a412837196e145566bc981b}"
REPO_URL="${REPO_URL:-https://github.com/tkleisas/polivoks-1982-vst}"
PYTHON="${PYTHON:-python3}"

if [ -n "${SDK_DIR:-}" ]; then
    case "$SDK_DIR" in
        /*) ;;
        *) SDK_DIR="$ROOT/$SDK_DIR" ;;
    esac
else
    SDK_DIR="$ROOT/.sdk"
fi

echo "== Polivoks 1982 build =="
echo "   repo: $ROOT"
echo "   SDK:  $SDK_DIR @ $SDK_COMMIT"

if [ ! -d "$SDK_DIR/.git" ]; then
    echo "-- cloning MPC Plugin SDK"
    git clone "$SDK_REPO" "$SDK_DIR"
fi
git -C "$SDK_DIR" fetch --quiet origin "$SDK_COMMIT" || true
git -C "$SDK_DIR" checkout --quiet "$SDK_COMMIT"

rm -rf "$SDK_DIR/plugins/Polivoks 1982"
mkdir -p "$SDK_DIR/plugins"
cp -a "$ROOT/plugin/Polivoks 1982" "$SDK_DIR/plugins/"

ZIG_ARGS=()
if [ -n "${ZIG:-}" ]; then ZIG_ARGS=(--zig "$ZIG"); fi

echo "-- building MPC (native + ARM32 + ARM64)"
"$PYTHON" "$SDK_DIR/tools/build.py" "$SDK_DIR/plugins/Polivoks 1982" --target all "${ZIG_ARGS[@]}"

echo "-- building desktop VST2"
"$PYTHON" "$SDK_DIR/tools/build.py" "$SDK_DIR/plugins/Polivoks 1982" --target desktop "${ZIG_ARGS[@]}"

echo "-- running host simulator test"
HOST_SIM="$SDK_DIR/.sdk-tools/host_sim"
NATIVE=""
for ext in so dylib; do
    f="$SDK_DIR/plugins/Polivoks 1982/build/native/polivoks-1982.$ext"
    if [ -f "$f" ]; then NATIVE="$f"; break; fi
done
if [ -z "$NATIVE" ]; then
    echo "error: native test library not found" >&2
    exit 1
fi
if [ ! -x "$HOST_SIM" ] && [ ! -f "$HOST_SIM" ]; then
    echo "error: host_sim not found at $HOST_SIM" >&2
    exit 1
fi
mkdir -p "$ROOT/dist"
TEST_WAV="$ROOT/dist/.host-sim-test.wav"
"$HOST_SIM" "$NATIVE" test --seconds 3 --notes 48,60,67 --wav "$TEST_WAV"
rm -f "$TEST_WAV"

echo "-- packaging release assets"
"$PYTHON" "$ROOT/tools/make_release.py" \
    --sdk "$SDK_DIR" \
    --out "$ROOT/dist" \
    --repo "$REPO_URL" \
    --desktop-name linux-vst2

echo
echo "Done. Release assets:"
ls -lh "$ROOT/dist"/*.zip
