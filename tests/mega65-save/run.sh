#!/usr/bin/env bash
# run.sh - MEGA65 save/load round trip through hyppo's SD file traps.
#
# xemu's -virtsd FAT is IN-MEMORY: host files seed it at boot, but
# writes never come back to the host, so the save FILE can't be
# diffed directly.  Instead:
#
# Phase A (round trip): boot with the zeroed 4K slot seeded, reach the
# start scene, ZSAVE, dump SAVBUF (address+size from the map), POISON
# SAVBUF with 0xAA via the monitor, ZLOAD, dump again - every byte must
# come back from the in-memory file (any unfetched byte stays 0xAA).
#
# Phase B (cross-boot load): fresh boot with phase A's SAVBUF dump
# seeded as DAGGOR65.SAV (padded to the 4K slot - exactly what
# write512 produces), ZLOAD, SAVBUF must equal the seed.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../mega65-lib.sh

echo "== 1. build"
make -C "$PORT/mega65" daggorath.prg DAGGOR65.SFX DAGGOR65.SAV >/dev/null
read -r SAVBUF SAVLEN <<<"$(awk '/ SAVBUF$/ {print $1, $3}' \
    "$PORT/mega65/daggorath.map")"
echo "   SAVBUF at 0x$SAVBUF, 0x$SAVLEN bytes"

boot_to_prompt() {   # $1 = run dir (with sd/ populated)
    m65_boot "$1"
    sleep 3
    m65 cmd @key:1 >/dev/null
    local try
    for try in $(seq 1 24); do
        sleep 5
        m65 dump 18000 1F40 "$1/scene.bin" >/dev/null
        if python3 ../mega65-scene/m65_diff.py \
            ../next-scene/desk-start.pgm "$1/scene.bin" >/dev/null 2>&1; then
            return 0
        fi
    done
    echo "boot_to_prompt: start scene never converged" >&2
    return 1
}

echo "== 2. phase A: ZSAVE, poison SAVBUF, ZLOAD"
rm -rf runA
mkdir -p runA/sd
cp "$PORT/mega65/DAGGOR65.SFX" "$PORT/mega65/DAGGOR65.SAV" runA/sd/
boot_to_prompt "$(pwd)/runA"
m65 cmd '@type:zsave x\r' >/dev/null
sleep 3
m65 dump "$SAVBUF" "$SAVLEN" runA/savbuf_a.bin >/dev/null
m65 fill "$SAVBUF" "$SAVLEN" AA
m65 dump "$SAVBUF" "$SAVLEN" runA/poisoned.bin >/dev/null
python3 -c "
d = open('runA/poisoned.bin','rb').read()
assert d == b'\xaa' * len(d), 'poison fill did not take'
"
m65 cmd '@type:zload x\r' >/dev/null
sleep 3
m65 dump "$SAVBUF" "$SAVLEN" runA/savbuf_b.bin >/dev/null
m65_stop
cmp runA/savbuf_a.bin runA/savbuf_b.bin \
    || { echo "MEGA65-SAVE FAILED (round trip differs)" >&2; exit 1; }
echo "   ZSAVE -> poison -> ZLOAD round-trips byte-exact ($((16#$SAVLEN)) bytes)"

echo "== 3. phase B: ZLOAD from a seeded file on a fresh boot"
rm -rf runB
mkdir -p runB/sd
cp "$PORT/mega65/DAGGOR65.SFX" runB/sd/
python3 -c "
d = open('runA/savbuf_a.bin','rb').read()
open('runB/sd/DAGGOR65.SAV','wb').write(d + b'\x00' * (4096 - len(d)))
"
boot_to_prompt "$(pwd)/runB"
m65 cmd '@type:zload x\r' >/dev/null
sleep 3
m65 dump "$SAVBUF" "$SAVLEN" runB/savbuf.bin >/dev/null
m65_stop
cmp runA/savbuf_a.bin runB/savbuf.bin \
    || { echo "MEGA65-SAVE FAILED (seeded load differs)" >&2; exit 1; }
echo "MEGA65-SAVE PASS (poisoned round trip + cross-boot seeded load byte-exact)"
