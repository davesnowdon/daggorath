#!/usr/bin/env bash
# run.sh - Spectrum Next sound verification (zxnDMA -> Covox).
#
# Boot the .nex in ZEsarUX with --aofile capture (headerless RAW:
# unsigned 8-bit MONO at 15600 Hz - the sndfile-less build can't write
# WAV), let the title fade + attract demo play their sample events, and
# verify the capture cross-correlates with the shipped 7350 Hz masters
# using the shared analyzer (tests/ep-sound/analyze_wav.py).
#
# --skip-seconds drops the NextZXOS boot beeps/key clicks: they
# correlate > 0.5 with the short percussive masters, and without the
# skip a completely mute game would still PASS on boot noise alone.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../next-lib.sh
CAPTURE_S="${CAPTURE_S:-75}"

echo "== 1. build"
make -C "$PORT/spectrum-next" >/dev/null

echo "== 2. boot with RAW audio capture"
rm -rf run
mkdir run
cp "$PORT/spectrum-next/daggorath.nex" "$PORT/spectrum-next/daggorath.sfx" run/
next_boot "$(pwd)/run" --ao null --aofile "$(pwd)/run/out.raw"
echo "   booted; capturing $CAPTURE_S s of title fade + demo"
sleep "$CAPTURE_S"
next_stop
[ -s run/out.raw ] || { echo "run.sh: no audio captured" >&2; exit 2; }
ls -l run/out.raw

echo "== 3. verdict (event cross-correlation vs sample masters)"
python3 ../ep-sound/analyze_wav.py run/out.raw "$PORT/assets/raw-7350" \
    --input-rate 15600 --skip-seconds 12 --label NEXT-SOUND \
    | tee run/verdict.txt
grep -q 'NEXT-SOUND PASS' run/verdict.txt
echo "NEXT-SOUND DONE"
