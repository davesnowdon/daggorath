#!/usr/bin/env bash
# run.sh - automated fixed-scene pixel diff: MEGA65 vs desktop.
#
# The README's manual procedure, scripted: boot xemu headless
# (../mega65-lib.sh: open-roms, virtual SD, uartmon), inject the PRG,
# RETURN during the title fade, then poll the live bitmap ($18000,
# 8000 bytes via monitor M dumps) until it matches the desktop golden
# (m65_diff.py vs ../next-scene/desk-start.pgm, heart cells masked).
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../mega65-lib.sh

echo "== 1. build"
make -C "$PORT/mega65" daggorath.prg DAGGOR65.SFX >/dev/null

echo "== 2. boot (xemu headless + uartmon) and inject"
rm -rf run
mkdir -p run/sd
cp "$PORT/mega65/DAGGOR65.SFX" run/sd/
m65_boot "$(pwd)/run"

echo "== 3. RETURN past the fade, poll the bitmap until it converges"
sleep 3
m65 cmd @key:1 >/dev/null
PASS=""
for try in $(seq 1 24); do
    sleep 5
    m65 dump 18000 1F40 run/m65-scene.bin >/dev/null
    if python3 m65_diff.py ../next-scene/desk-start.pgm \
        run/m65-scene.bin > run/diff.txt 2>&1; then
        PASS=yes
        break
    fi
done
m65_stop
cat run/diff.txt
[ -n "$PASS" ] || { echo "MEGA65-SCENE FAILED (never converged)" >&2; exit 1; }
echo "MEGA65-SCENE PASS"
