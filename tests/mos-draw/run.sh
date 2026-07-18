#!/usr/bin/env bash
# run.sh - MEGA65 rasterizer corpus identity under mos-sim.
#
# The MEGA65 backend carries its own C copy of the DDA (cell-
# interleaved plot) which until now only had integration coverage
# (mega65-scene).  This gives it what the Z80 backends have: the REAL
# shipping compile unit (mega65/plat_mega65.c, #included whole by
# wrapper.c with a stub hardware header) built by llvm-mos, run on the
# simulated 6502, over the same corpus as tests/z80draw-ep - byte-
# compared against the normative core/draw_ref.c on the host.
#
# Prints "MOS-DRAW IDENTICAL" on success.
set -euo pipefail
cd "$(dirname "$0")"

LLVM_MOS=${LLVM_MOS:-$HOME/retro-computing/mega65/dev/llvm-mos}
CORE=../../core

echo "== 1. corpus (shared with z80draw-ep)"
gcc -O1 -o corpus_gen ../z80draw-ep/corpus_gen_ep.c
./corpus_gen > corpus_data.h

echo "== 2. the real mega65 rasterizer under mos-sim"
"$LLVM_MOS/bin/mos-sim-clang" -Os -Istub -I"$CORE" -o mos_draw wrapper.c
"$LLVM_MOS/bin/mos-sim" mos_draw > fb_hex.txt

echo "== 3. verdict (vs the normative core/draw_ref.c)"
gcc -std=c99 -O2 -Wall -Wextra -I"$CORE" -o host_check \
    host_check_m65.c "$CORE/draw_ref.c"
./host_check fb_hex.txt
