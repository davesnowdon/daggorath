#!/usr/bin/env bash
# build-loader.sh - assemble the big-program loader and package it with the
# raw game image (GAME.BIN) so the 34.5 KB game loads across segments.
#
#   ./build-loader.sh          # assumes daggorat.com is already built
# Produces: loader.com, game.bin  (both in this dir).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
SJASM="${SJASM:-/home/dns/retro-computing/spectrum-next/dev/toolchains/sjasmplus/sjasmplus}"

[ -f daggorat.com ] || { echo "daggorat.com missing - run make first" >&2; exit 1; }

# 1. raw game image = .com body (strip the 16-byte EXOS type-5 header)
python3 - <<'PY'
d = open("daggorat.com","rb").read()
body = d[16:]
open("game.bin","wb").write(body)
p0, p1 = 0x3F00, 0x4000
p2 = len(body) - p0 - p1
assert p2 > 0, f"game body {len(body)} too small"
open("sizes.inc","w").write(f"GAME_P2LEN EQU 0x{p2:04X}   ; page-2 remainder ({p2} bytes)\n")
print(f"game.bin {len(body)} bytes: p0=0x{p0:04X} p1=0x{p1:04X} p2=0x{p2:04X}")
PY

# 2. assemble the loader (raw = header+body starting at ORG 0x00F0)
"$SJASM" loader.s --raw=loader.com --nologo
ls -l loader.com game.bin
python3 - <<'PY'
d=open("loader.com","rb").read()
print("loader header:", " ".join(f"{b:02X}" for b in d[:16]), " body len field:",
      hex(d[2] | (d[3]<<8)), "actual body:", hex(len(d)-16))
PY
