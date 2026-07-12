#!/bin/bash
# Build & run the golden-data generators against the C++ port's source.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
PORT_SRC=$REPO/../../native-port/DungeonsOfDaggorath/src
GOLD=$REPO/tests/golden
BUILD=$REPO/tests/golden/.build
mkdir -p "$GOLD" "$BUILD"

SDL_CFLAGS=$(sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2)
# SDL2_mixer may live in ~/.local (built from source earlier)
MIX_CFLAGS="-I$HOME/.local/include -I$HOME/.local/include/SDL2"

echo "=== golden: RNG streams (from the port's own RNG class) ==="
g++ -O2 -o "$BUILD/golden_rng" "$HERE/golden_rng.cpp" \
    -I"$PORT_SRC" $SDL_CFLAGS $MIX_CFLAGS
"$BUILD/golden_rng" "$GOLD"

echo "goldens up to date in $GOLD"
