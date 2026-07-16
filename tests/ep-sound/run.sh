#!/usr/bin/env bash
# run.sh - Enterprise sound verification: boot the game in ep128emu with WAV
# capture (sound.file=), let the title fade + attract demo play their sample
# events, then verify the capture cross-correlates with the shipped 7350 Hz
# masters (analyze_wav.py).  Ships only DAGGOR1.SFX, so this also exercises
# the stock-profile loader path.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"
CAPTURE_S="${CAPTURE_S:-75}"     # demo dwell: title fade + first demo loop

echo "== 1. build game + loader + blobs"
make -C "$PORT/enterprise" loader.com game.bin DAGGOR1.SFX >/dev/null

echo "== 2. boot with WAV capture"
rm -f out.wav
rm -rf run
EMUOPTS="sound.file=$(pwd)/out.wav" OUTDIR="$(pwd)/run" \
    "$DEVTOOLS/runcom.sh" \
    "$PORT/enterprise/loader.com" "$PORT/enterprise/game.bin" \
    "$PORT/enterprise/DAGGOR1.SFX" \
    > run-boot.log 2>&1
echo "   booted; capturing $CAPTURE_S s of title fade + demo"
sleep "$CAPTURE_S"
# SIGTERM first: give the emulator a chance to finalize the WAV header
pkill -TERM -f ep128emu 2>/dev/null || true
sleep 2
pkill -9 -f ep128emu 2>/dev/null || true
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
[ -s out.wav ] || { echo "run.sh: no WAV captured" >&2; exit 2; }
ls -l out.wav

echo "== 3. verdict (event cross-correlation vs sample masters)"
python3 analyze_wav.py out.wav "$PORT/assets/raw-7350" | tee verdict.txt
grep -q 'EP-SOUND PASS' verdict.txt
echo "EP-SOUND DONE"
