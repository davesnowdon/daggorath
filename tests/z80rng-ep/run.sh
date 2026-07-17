#!/usr/bin/env bash
# run.sh - z80 RNG identity test under the ENTERPRISE toolchain flavour.
#
# Same harness as tests/z80rng (whose wrapper.c / done.asm / host_check.c
# are reused in place - no copies to drift), but:
#   - assembles the REAL enterprise/rng_z80.asm (byte-identical to the
#     Next's today; this test keeps that true)
#   - compiles WITHOUT -clib=z80n and executes under z88dk-ticks -mz80
#     (PLAIN Z80 - proves no Z80n opcodes creep in, the same guarantee
#     tests/z80draw-ep provides for the rasterizer)
#
# Prints "Z80RNG-EP IDENTICAL" on success, else a diff summary.
set -euo pipefail
cd "$(dirname "$0")"

Z88DK="${Z88DK:-$HOME/retro-computing/spectrum-next/dev/toolchains/z88dk}"
export Z88DK
export ZCCCFG="$Z88DK/lib/config"
export PATH="$Z88DK/bin:$PATH"

CORE=../../core
REF=../z80rng
ASM=../../enterprise/rng_z80.asm

echo "== 1. z80 asm rng under z88dk-ticks (plain -mz80)"
zcc +test -compiler=sdcc -SO2 --max-allocs-per-node20000 \
    -pragma-define:CRT_ORG_CODE=0x8000 -DDOD_RNG_ASM -I"$CORE" \
    "$REF/wrapper.c" "$REF/done.asm" "$CORE/rng.c" "$ASM" -o wrapper_asm -m
done_addr=$(grep -E '^_test_done\b' wrapper_asm.map \
            | head -1 | sed -E 's/.*\$([0-9a-fA-F]+).*/\1/')
if [ -z "$done_addr" ]; then
    echo "run.sh: _test_done not found in wrapper_asm.map" >&2
    exit 2
fi
cp wrapper_asm wrapper_asm.bin
rm -f ram_asm.bin
timeout 300 z88dk-ticks -mz80 -l 0x8000 -pc 0x8000 -end "0x$done_addr" \
    -counter 2000000000 -output ram_asm.bin wrapper_asm.bin \
    | tail -1 > cycles_asm.txt
if [ ! -s cycles_asm.txt ] || [ ! -f ram_asm.bin ]; then
    echo "run.sh: ticks produced no output" >&2
    exit 2
fi
echo "   ticks: $(cat cycles_asm.txt) cycles"

echo "== 2. verdict (asm vs the normative C rng)"
gcc -std=c99 -O2 -Wall -Wextra -I"$CORE" -o host_check \
    "$REF/host_check.c" "$CORE/rng.c"
./host_check ram_asm.bin | sed 's/Z80RNG/Z80RNG-EP/'
