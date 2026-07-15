#!/usr/bin/env bash
# run.sh - build and run the z80 RNG identity test.
#
#   1. compile the REAL core/rng.c (with -DDOD_RNG_ASM, exactly as the
#      shipping build does) + the REAL spectrum-next/rng_z80.asm into a
#      zsdcc test wrapper (z88dk +test target, org 0x8000) and execute
#      it under z88dk-ticks; collect the 64K RAM dump on exit
#   2. replay the identical seed corpus through the NORMATIVE C
#      rng_RANDOM with gcc and compare the dump byte-for-byte
#      (return values AND the SEED/carry state bytes)
#
# Prints "Z80RNG IDENTICAL" on success, else a diff summary.
# See tests/z80draw/run.sh for the z88dk-ticks quirks the invocation
# works around (extension required, -counter not -w, dump only written
# on -end exits).
set -euo pipefail
cd "$(dirname "$0")"

Z88DK="${Z88DK:-$HOME/retro-computing/spectrum-next/dev/toolchains/z88dk}"
export Z88DK
export ZCCCFG="$Z88DK/lib/config"
export PATH="$Z88DK/bin:$PATH"

CORE=../../core
ASM=../../spectrum-next/rng_z80.asm

echo "== 1. z80 asm rng under z88dk-ticks"
zcc +test -clib=z80n -compiler=sdcc -SO2 --max-allocs-per-node20000 \
    -pragma-define:CRT_ORG_CODE=0x8000 -DDOD_RNG_ASM -I"$CORE" \
    wrapper.c done.asm "$CORE/rng.c" "$ASM" -o wrapper_asm -m
done_addr=$(grep -E '^_test_done\b' wrapper_asm.map \
            | head -1 | sed -E 's/.*\$([0-9a-fA-F]+).*/\1/')
if [ -z "$done_addr" ]; then
    echo "run.sh: _test_done not found in wrapper_asm.map" >&2
    exit 2
fi
cp wrapper_asm wrapper_asm.bin
rm -f ram_asm.bin
timeout 300 z88dk-ticks -mz80n -l 0x8000 -pc 0x8000 -end "0x$done_addr" \
    -counter 2000000000 -output ram_asm.bin wrapper_asm.bin \
    | tail -1 > cycles_asm.txt
if [ ! -s cycles_asm.txt ] || [ ! -f ram_asm.bin ]; then
    echo "run.sh: ticks produced no output" >&2
    exit 2
fi
echo "   ticks: $(cat cycles_asm.txt) cycles"

echo "== 2. verdict (asm vs the normative C rng)"
gcc -std=c99 -O2 -Wall -Wextra -I"$CORE" -o host_check \
    host_check.c "$CORE/rng.c"
./host_check ram_asm.bin
