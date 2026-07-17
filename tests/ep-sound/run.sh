#!/usr/bin/env bash
# run.sh - Enterprise sound verification: boot the game in ep128emu with WAV
# capture (sound.file=), let the title fade + attract demo play their sample
# events, then verify the capture cross-correlates with the shipped 7350 Hz
# masters (analyze_wav.py).
#
# Two passes:
#   stock (128K, DAGGOR1.SFX only)  - the 3-bin profile-1 loader path
#   384K  (both blobs on the disk)  - >= 8 spare segments, so the loader
#                                     picks DAGGOR2.SFX (profile 2, all
#                                     full-rate); previously untested
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"
CAPTURE_S="${CAPTURE_S:-75}"     # demo dwell: title fade + first demo loop

echo "== 1. build game + loader + blobs"
make -C "$PORT/enterprise" loader.com game.bin DAGGOR1.SFX DAGGOR2.SFX \
    >/dev/null

capture_and_verify() {   # $1 = label, $2 = profile, $3 = EMUOPTS, $4.. = files
    local label="$1" profile="$2" extra="$3"
    shift 3
    echo "== boot ($label) with WAV capture"
    rm -f out.wav
    rm -rf run
    # 384K machines memory-test longer at boot (probe lesson)
    EMUOPTS="sound.file=$(pwd)/out.wav $extra" \
        BOOT_WAIT="${BOOT_WAIT:-20}" POSTRET_WAIT="${POSTRET_WAIT:-14}" \
        OUTDIR="$(pwd)/run" \
        "$DEVTOOLS/runcom.sh" "$PORT/enterprise/loader.com" "$@" \
        > "run-boot-$label.log" 2>&1
    echo "   booted; capturing $CAPTURE_S s of title fade + demo"
    sleep "$CAPTURE_S"
    # which blob did the loader pick?  handoff block byte 0 at FC:0x00F0
    rm -f "$HOME/.ep128emu/qs_ep128.dat"
    DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
        >/dev/null 2>&1 || true
    sleep 0.6
    got=$(python3 - "$HOME/.ep128emu/qs_ep128.dat" <<'EOF'
import struct, sys
data = open(sys.argv[1], 'rb').read()
pos = 16
while pos + 12 <= len(data):
    t, l = struct.unpack('>II', data[pos:pos+8])
    body = data[pos+8:pos+8+l]; pos += 12 + l
    if t == 0x45508002:
        p = 8
        while p + 2 + 16384 <= len(body):
            s = body[p]; p += 2
            if s == 0xFC:
                print(body[p + 0xF0]); raise SystemExit
            p += 16384
print(-1)
EOF
)
    # SIGTERM first: give the emulator a chance to finalize the WAV header
    pkill -TERM -f ep128emu 2>/dev/null || true
    sleep 2
    pkill -9 -f ep128emu 2>/dev/null || true
    pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
    [ "$got" = "$profile" ] || {
        echo "run.sh: loader picked profile $got, expected $profile ($label)" >&2
        exit 2
    }
    echo "   loader picked SFX profile $got (expected)"
    [ -s out.wav ] || { echo "run.sh: no WAV captured ($label)" >&2; exit 2; }
    ls -l out.wav
    echo "== verdict ($label): event cross-correlation vs sample masters"
    python3 analyze_wav.py out.wav "$PORT/assets/raw-7350" \
        | tee "verdict-$label.txt"
    grep -q 'EP-SOUND PASS' "verdict-$label.txt"
}

capture_and_verify stock 1 "" \
    "$PORT/enterprise/game.bin" "$PORT/enterprise/DAGGOR1.SFX"

capture_and_verify 384k 2 "memory.ram.size=384" \
    "$PORT/enterprise/game.bin" \
    "$PORT/enterprise/DAGGOR1.SFX" "$PORT/enterprise/DAGGOR2.SFX"

echo "EP-SOUND DONE (stock DAGGOR1 + 384K DAGGOR2 both pass)"
