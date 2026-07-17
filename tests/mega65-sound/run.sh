#!/usr/bin/env bash
# run.sh - MEGA65 Audio-DMA sound checks (memory/register level).
#
# xemu has no audio-capture option, so unlike ep-sound/next-sound this
# cannot cross-correlate a recording.  It proves the three things that
# break silently instead:
#   1. snd_ok == 1        (blob found, placed, fully streamed in)
#   2. placed bytes match (first 256 bytes of sample 0 at $40000 -
#      place_samples() first-fits from the $40000 region - equal the
#      blob's first 256 bytes)
#   3. the title-fade buzz ARMS channel 0 (ctrl bit7 polled during the
#      fade) with the exact 7350 Hz phase step the backend computes,
#      at a non-zero volume
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
. ../mega65-lib.sh

echo "== 1. build"
make -C "$PORT/mega65" daggorath.prg DAGGOR65.SFX >/dev/null
SNDOK=$(awk '/ snd_ok$/ {print $1}' "$PORT/mega65/daggorath.map")
echo "   snd_ok at 0x$SNDOK"

echo "== 2. boot and inject"
rm -rf run
mkdir -p run/sd
cp "$PORT/mega65/DAGGOR65.SFX" run/sd/
m65_boot "$(pwd)/run"

echo "== 3. poll channel 0 through the title fade"
ARMED=""
for try in $(seq 1 60); do
    sleep 1
    m65 dump FFD3720 10 run/ch0.bin >/dev/null
    CTRL=$(python3 -c "print(open('run/ch0.bin','rb').read()[0])")
    if [ "$CTRL" -ge 128 ]; then
        ARMED=yes
        break
    fi
done
[ -n "$ARMED" ] || { m65_stop; echo "MEGA65-SOUND FAILED (channel 0 never armed)" >&2; exit 1; }

echo "== 4. checks"
m65 dump "$SNDOK" 1 run/snd_ok.bin >/dev/null
m65 dump 40000 100 run/placed.bin >/dev/null
m65_stop
python3 - "$PORT/mega65/DAGGOR65.SFX" <<'EOF'
import sys

blob = open(sys.argv[1], 'rb').read()
snd_ok = open('run/snd_ok.bin', 'rb').read()[0]
assert snd_ok == 1, f'snd_ok = {snd_ok}, blob not loaded/placed'
print('   snd_ok == 1 (blob streamed and placed)')

placed = open('run/placed.bin', 'rb').read()
nd = sum(a != b for a, b in zip(placed, blob[:256]))
assert nd == 0, f'{nd}/256 placed bytes differ at $40000'
print('   sample 0 bytes at $40000 == blob (256/256)')

ch0 = open('run/ch0.bin', 'rb').read()
ctrl = ch0[0]
freq = ch0[4] | (ch0[5] << 8) | (ch0[6] << 16)
vol = ch0[9]
# SFX_STEP from sound_mega65.c: rate/40.5MHz * 2^24, rounded
step = ((7350 << 24) + 20250000) // 40500000
assert freq == step, f'CH0_FREQ {freq} != expected step {step}'
assert vol != 0, 'CH0_VOL is zero'
print(f'   channel armed: ctrl=0x{ctrl:02X} freq={freq} (= step) vol={vol}')
EOF
echo "MEGA65-SOUND PASS (blob placed byte-exact; channel armed at 7350 Hz)"
