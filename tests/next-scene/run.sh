#!/usr/bin/env bash
# run.sh - automated fixed-scene pixel diff: Spectrum Next vs desktop.
#
# The README's manual ZRCP procedure, scripted: boot the .nex in
# ZEsarUX (next-lib.sh), press ENTER during the title fade (a keypress
# in the attract demo also starts a real game, so timing is forgiving),
# then poll the draw buffer (bank 7 low page at 0x4000, 6144 bytes -
# present() copies it verbatim to the visible bank 5) until it matches
# the desktop golden (ula_diff.py vs desk-start.pgm, heart cells
# masked).  Poll-until-converged because game phases take variable
# wall time under emulation.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../next-lib.sh

echo "== 1. build"
make -C "$PORT/spectrum-next" >/dev/null

echo "== 2. boot (ZEsarUX + ZRCP)"
rm -rf run
mkdir run
cp "$PORT/spectrum-next/daggorath.nex" "$PORT/spectrum-next/daggorath.sfx" run/
next_boot "$(pwd)/run"

echo "== 3. ENTER past the fade, poll the draw buffer until it converges"
sleep 6
python3 zrcp.py keys '\n' 100
PASS=""
for try in $(seq 1 24); do
    sleep 5
    python3 zrcp.py mem 16384 6144 run/next-ula.bin >/dev/null
    if python3 ula_diff.py desk-start.pgm run/next-ula.bin \
        > run/diff.txt 2>&1; then
        PASS=yes
        break
    fi
done
next_stop
cat run/diff.txt
[ -n "$PASS" ] || { echo "NEXT-SCENE FAILED (never converged)" >&2; exit 1; }
echo "NEXT-SCENE PASS"
