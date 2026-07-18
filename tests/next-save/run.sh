#!/usr/bin/env bash
# run.sh - Spectrum Next save/load round trip via esxDOS.
#
# ZEsarUX's --esxdos-root-dir maps esxDOS file I/O onto a host
# directory, so daggorath.sav is a plain host file - no image
# extraction needed (unlike tests/ep-save's FAT12 walker).
#
# Phase A (save): boot, ENTER into a real game, wait for the start
# scene (draw-buffer convergence, same check as next-scene), ZSAVE,
# then verify the host file EQUALS the in-RAM blob (SAVBUF address from
# the map, read via ZRCP read-memory) byte for byte.
#
# Phase B (load): fresh boot with phase A's save seeded, ZLOAD until
# SAVBUF equals the seeded file - proving esxDOS read it back and the
# core applied it.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../next-lib.sh
ZRCPQ() { python3 ../next-scene/zrcp.py "$@"; }

echo "== 1. build"
make -C "$PORT/spectrum-next" >/dev/null
SAVBUF=$(grep -E '^_SAVBUF\b' "$PORT/spectrum-next/daggorath.map" \
         | head -1 | sed -E 's/.*\$([0-9a-fA-F]+).*/0x\1/')
echo "   SAVBUF at $SAVBUF"

boot_to_prompt() {   # $1 = run dir
    next_boot "$1"
    sleep 6
    ZRCPQ keys '\n' 100
    local try
    for try in $(seq 1 24); do
        sleep 5
        ZRCPQ mem 16384 6144 "$1/ula.bin" >/dev/null
        if python3 ../next-scene/ula_diff.py \
            ../next-scene/desk-start.pgm "$1/ula.bin" >/dev/null 2>&1; then
            return 0
        fi
    done
    echo "boot_to_prompt: start scene never converged" >&2
    return 1
}

echo "== 2. phase A: ZSAVE"
rm -rf runA
mkdir runA
cp "$PORT/spectrum-next/daggorath.nex" "$PORT/spectrum-next/daggorath.sfx" runA/
boot_to_prompt "$(pwd)/runA"
SAVED=""
for try in 1 2 3 4; do
    ZRCPQ keys 'zsave x\n' 80
    sleep 3
    if [ -s runA/daggorath.sav ]; then
        SAVED=yes
        echo "   daggorath.sav written (try $try, $(stat -c%s runA/daggorath.sav) bytes)"
        break
    fi
done
[ -n "$SAVED" ] || { next_stop; echo "NEXT-SAVE FAILED (no save file appeared)" >&2; exit 1; }
# file = 8-byte envelope header + payload
LEN=$(( $(stat -c%s runA/daggorath.sav) - 8 ))
ZRCPQ mem "$SAVBUF" "$LEN" runA/savbuf.bin >/dev/null
next_stop
head -c 2 runA/daggorath.sav | grep -q '^DS$' \
    || { echo "NEXT-SAVE FAILED (no DS envelope magic)" >&2; exit 1; }
tail -c +9 runA/daggorath.sav | cmp - runA/savbuf.bin \
    || { echo "NEXT-SAVE FAILED (payload != SAVBUF)" >&2; exit 1; }
echo "   save payload == SAVBUF ($LEN bytes, envelope present)"

echo "== 3. phase B: ZLOAD from a seeded save"
rm -rf runB
mkdir runB
cp "$PORT/spectrum-next/daggorath.nex" "$PORT/spectrum-next/daggorath.sfx" runB/
cp runA/daggorath.sav runB/
boot_to_prompt "$(pwd)/runB"
LOADED=""
for try in 1 2 3 4; do
    ZRCPQ keys 'zload x\n' 80
    sleep 3
    ZRCPQ mem "$SAVBUF" "$LEN" runB/savbuf.bin >/dev/null
    if tail -c +9 runB/daggorath.sav | cmp -s - runB/savbuf.bin; then
        LOADED=yes
        echo "   SAVBUF matches the seeded save payload (try $try)"
        break
    fi
done
next_stop
[ -n "$LOADED" ] || { echo "NEXT-SAVE FAILED (load never matched)" >&2; exit 1; }
echo "NEXT-SAVE PASS (ZSAVE file == blob; ZLOAD round-trips byte-exact)"
