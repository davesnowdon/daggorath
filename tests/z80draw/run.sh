#!/usr/bin/env bash
# run.sh - build and run the z80 draw identity test.
#
#   1. generate the deterministic corpus (corpus_gen.c, seed 0xC0DE1234)
#   2. compile the REAL spectrum-next/draw_z80n.asm into a zsdcc test
#      wrapper (z88dk +test target, org 0x8000) and execute it under
#      z88dk-ticks -mz80n; collect the 64K RAM dump on exit
#   3. rasterize the same corpus with the normative core/draw_ref.c on
#      the host (gcc), pack to the ULA layout, compare byte-for-byte
#   4. repeat step 2/3 with a zsdcc-compiled C DDA (cref_z80.c) for a
#      cycle-count baseline and an independent harness cross-check
#
# Prints "Z80DRAW IDENTICAL" on success (asm side), else a diff summary.
set -euo pipefail
cd "$(dirname "$0")"

Z88DK="${Z88DK:-$HOME/retro-computing/spectrum-next/dev/toolchains/z88dk}"
export Z88DK
export ZCCCFG="$Z88DK/lib/config"
export PATH="$Z88DK/bin:$PATH"

CORE=../../core
ASM=../../spectrum-next/draw_z80n.asm

echo "== 1. corpus"
gcc -std=c99 -O2 -Wall -Wextra -o corpus_gen corpus_gen.c
./corpus_gen > corpus_data.h

echo "== 2. host reference (gcc + core/draw_ref.c)"
gcc -std=c99 -O2 -Wall -Wextra -I"$CORE" -o host_check \
    host_check.c "$CORE/draw_ref.c"

build_and_run () {
    # $1 = tag, remaining args = source of the routine under test
    local tag="$1"; shift
    zcc +test -clib=z80n -compiler=sdcc -SO2 --max-allocs-per-node20000 \
        -pragma-define:CRT_ORG_CODE=0x8000 \
        wrapper.c done.asm "$@" -o "wrapper_$tag" -m
    local done_addr
    done_addr=$(grep -E '^_test_done\b' "wrapper_$tag.map" \
                | head -1 | sed -E 's/.*\$([0-9a-fA-F]+).*/\1/')
    if [ -z "$done_addr" ]; then
        echo "run.sh: _test_done not found in wrapper_$tag.map" >&2
        exit 2
    fi
    # ticks v0.14c quirks (all verified empirically):
    #   - segfaults on input files without an extension -> use .bin
    #   - default run cap is 1e8 cycles (a default -counter); raise it
    #     with -counter, NOT -w: -w runs never write the -output dump
    #     and always exit nonzero
    #   - the RAM dump is only written on -end/-counter exits, not on
    #     the CMD_EXIT trap -> the wrapper parks on _test_done
    # A stale dump would silently pass: delete it first, and host_check
    # verifies the wrapper's completion sentinel besides the pixels.
    cp "wrapper_$tag" "wrapper_$tag.bin"
    rm -f "ram_$tag.bin"
    timeout 300 z88dk-ticks -mz80n -l 0x8000 -pc 0x8000 -end "0x$done_addr" \
        -counter 2000000000 -output "ram_$tag.bin" "wrapper_$tag.bin" \
        | tail -1 > "cycles_$tag.txt"
    if [ ! -s "cycles_$tag.txt" ] || [ ! -f "ram_$tag.bin" ]; then
        echo "run.sh: ticks produced no output for $tag" >&2
        exit 2
    fi
    echo "   ticks($tag): $(cat "cycles_$tag.txt") cycles"
}

echo "== 3. z80n asm under z88dk-ticks"
build_and_run asm "$ASM"

echo "== 4. verdict (asm vs draw_ref)"
./host_check ram_asm.bin

echo "== 5. zsdcc C baseline (cycle comparison + harness cross-check)"
build_and_run cref cref_z80.c
if ./host_check ram_cref.bin > /dev/null; then
    echo "   C baseline also identical (harness cross-check OK)"
else
    echo "   WARNING: C baseline mismatch - harness suspect" >&2
fi

ASM_CYC=$(cat cycles_asm.txt)
CREF_CYC=$(cat cycles_cref.txt)
if [ -n "$ASM_CYC" ] && [ -n "$CREF_CYC" ] && [ "$ASM_CYC" -gt 0 ]; then
    echo "   speedup (whole corpus incl. wrapper): \
$(( CREF_CYC / ASM_CYC )).$(( (CREF_CYC * 100 / ASM_CYC) % 100 ))x \
($CREF_CYC vs $ASM_CYC cycles)"
fi
