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
python3 verify_savbuf.py snap_save.ep128s saved.bin "$SAVBUF"

echo "== 2b. phase A2: second ZSAVE (opposite flip parity through the dance)"
# LOOK forces a full redraw -> one plat_present -> the draw/front buffers
# swap, so this save enters the EXOS dance with the other LPT/LD1 parity.
DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" 'l\n' 80
sleep 3
SAVED2=""
for try in 1 2 3 4; do
    DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" 'zsave x\n' 80
    sleep 5
    snapshot snap_save2.ep128s
    if python3 fat12.py run/disk.img DAGGOR.SAV saved2.bin 2>/dev/null \
       && python3 verify_savbuf.py snap_save2.ep128s saved2.bin "$SAVBUF"; then
        SAVED2=yes
        echo "   second save (overwrite) round-trips (try $try)"
        break
    fi
done
[ -n "$SAVED2" ] || { echo "EP-SAVE FAILED (second save never matched)" >&2; exit 1; }

echo "== 3. phase B: ZLOAD from a seeded save"
cp saved2.bin DAGGOR.SAV
boot "$(pwd)/DAGGOR.SAV"
LOADED=""
for try in 1 2 3 4; do
    DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" 'zload x\n' 80
    sleep 5
    snapshot snap_load.ep128s
    if python3 verify_savbuf.py snap_load.ep128s saved2.bin "$SAVBUF"; then
        LOADED=yes
        echo "   SAVBUF matches the seeded save (try $try)"
        break
    fi
done
pkill -9 -f ep128emu 2>/dev/null || true
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
[ -n "$LOADED" ] || { echo "EP-SAVE FAILED (load never matched)" >&2; exit 1; }
echo "EP-SAVE PASS (ZSAVE file == blob; ZLOAD round-trips byte-exact)"
