#!/usr/bin/env bash
# run.sh - EP save/load round trip through the EXOS dance.
#
# Phase A (save): boot the game, enter a real game, type ZSAVE until
# DAGGOR.SAV appears on the FAT12 disk (real-time keystrokes are lossy
# while the game is in PREPARE, so the file itself is the retry signal),
# then verify the file EQUALS the in-RAM save blob (SAVBUF, address from
# the map) byte for byte.
#
# Phase B (load): fresh boot with Phase A's save file SEEDED onto the
# disk (the real "resume an old game" case), type ZLOAD until the in-RAM
# blob equals the seeded file - proving the dance read it back and the
# core applied it.
#
# Uses ep128emu quick snapshots (Ctrl+F9) to read SAVBUF, and a local
# FAT12 walker to read the disk image.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"
QS="$HOME/.ep128emu/qs_ep128.dat"

echo "== 1. build"
make -C "$PORT/enterprise" loader.com game.bin DAGGOR1.SFX >/dev/null
SAVBUF=$(grep -E '^_SAVBUF\b' "$PORT/enterprise/daggorat.map" \
         | sed -E 's/.*\$([0-9a-fA-F]+).*/0x\1/')
echo "   SAVBUF at $SAVBUF"

snapshot() {    # -> $1 (file)
    rm -f "$QS"
    DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
        >/dev/null 2>&1 || true
    sleep 0.6
    cp "$QS" "$1"
}

boot() {        # $@ = extra disk files
    pkill -9 -f ep128emu 2>/dev/null || true
    pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
    sleep 1
    rm -rf run
    LOAD_WAIT=4 OUTDIR="$(pwd)/run" "$DEVTOOLS/runcom.sh" \
        "$PORT/enterprise/loader.com" "$PORT/enterprise/game.bin" \
        "$PORT/enterprise/DAGGOR1.SFX" "$@" > run-boot.log 2>&1
    sleep 40
    DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" ' ' 80   # demo -> PREPARE
    sleep 16
}

echo "== 2. phase A: ZSAVE"
boot
SAVED=""
for try in 1 2 3 4; do
    DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" 'zsave x\n' 80
    sleep 5
    if python3 fat12.py run/disk.img DAGGOR.SAV saved.bin 2>/dev/null; then
        SAVED=yes
        echo "   DAGGOR.SAV written (try $try)"
        break
    fi
done
[ -n "$SAVED" ] || { echo "EP-SAVE FAILED (no save file appeared)" >&2; exit 1; }
snapshot snap_save.ep128s
python3 - "$SAVBUF" <<'EOF'
import struct, sys
addr = int(sys.argv[1], 0)
data = open('snap_save.ep128s','rb').read()
pos = 16; segs = {}
while pos + 12 <= len(data):
    t, l = struct.unpack('>II', data[pos:pos+8])
    body = data[pos+8:pos+8+l]; pos += 12 + l
    if t == 0x45508002:
        p = 8
        while p + 2 + 16384 <= len(body):
            s = body[p]; p += 2; segs[s] = body[p:p+16384]; p += 16384
sav = open('saved.bin','rb').read()
seg = 0xFC + (addr >> 14)
buf = segs[seg][addr & 0x3FFF:(addr & 0x3FFF) + len(sav)]
nd = sum(a != b for a, b in zip(sav, buf))
print(f'   save file vs SAVBUF: {nd} differing bytes of {len(sav)}')
raise SystemExit(0 if nd == 0 else 1)
EOF

echo "== 3. phase B: ZLOAD from a seeded save"
cp saved.bin DAGGOR.SAV
boot "$(pwd)/DAGGOR.SAV"
LOADED=""
for try in 1 2 3 4; do
    DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" 'zload x\n' 80
    sleep 5
    snapshot snap_load.ep128s
    if python3 - "$SAVBUF" <<'EOF'
import struct, sys
addr = int(sys.argv[1], 0)
data = open('snap_load.ep128s','rb').read()
pos = 16; segs = {}
while pos + 12 <= len(data):
    t, l = struct.unpack('>II', data[pos:pos+8])
    body = data[pos+8:pos+8+l]; pos += 12 + l
    if t == 0x45508002:
        p = 8
        while p + 2 + 16384 <= len(body):
            s = body[p]; p += 2; segs[s] = body[p:p+16384]; p += 16384
sav = open('saved.bin','rb').read()
seg = 0xFC + (addr >> 14)
buf = segs[seg][addr & 0x3FFF:(addr & 0x3FFF) + len(sav)]
raise SystemExit(0 if bytes(buf) == sav else 1)
EOF
    then
        LOADED=yes
        echo "   SAVBUF matches the seeded save (try $try)"
        break
    fi
done
pkill -9 -f ep128emu 2>/dev/null || true
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
[ -n "$LOADED" ] || { echo "EP-SAVE FAILED (load never matched)" >&2; exit 1; }
echo "EP-SAVE PASS (ZSAVE file == blob; ZLOAD round-trips byte-exact)"
