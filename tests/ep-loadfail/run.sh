#!/usr/bin/env bash
# run.sh - EP loader negative paths: a broken install must halt loudly.
#
# The loader's only failure signal is the raw EXOS status byte written to
# the border (port 81h) before it hangs - the game itself always runs with
# a black border, so "border != 0 after the load command" IS the assertion.
# Border colour comes from the NICK_STATE chunk of a quick snapshot, not a
# screenshot, so the check is exact and windowing-independent.
#
# Case 1: GAME.BIN missing entirely       -> EXOS open fails (old path)
# Case 2: GAME.BIN truncated to 20000 B   -> 2nd read-block hits EOF (the
#         status check added after the review; a partial image must not
#         be jumped into)
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"
QS="$HOME/.ep128emu/qs_ep128.dat"

echo "== 1. build"
make -C "$PORT/enterprise" loader.com game.bin >/dev/null

# boot with the given disk contents, then poll for a non-zero border;
# xtype lines are lossy while the machine is busy, so retype the load
# command until the border changes (or give up)
expect_error_border() {   # $@ = files for the disk (loader.com first)
    pkill -9 -f ep128emu 2>/dev/null || true
    pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
    sleep 1
    rm -rf run
    LOAD_WAIT=6 OUTDIR="$(pwd)/run" "$DEVTOOLS/runcom.sh" "$@" \
        > run-boot.log 2>&1
    local border=0
    for try in 1 2 3; do
        rm -f "$QS"
        DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
            >/dev/null 2>&1 || true
        sleep 0.6
        border=$(python3 nick_border.py "$QS")
        [ "$border" != "0" ] && break
        DISPLAY="$XDISP" "$DEVTOOLS/xtype" "ep128emu" \
            'load "a:LOADER.COM"\n' 80
        sleep 6
    done
    pkill -9 -f ep128emu 2>/dev/null || true
    pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
    echo "$border"
}

# exact expected borders (= raw EXOS status codes; measured + stable in
# the emulator): distinct codes prove the two DIFFERENT error paths
# fired, not merely "some failure happened"
EXPECT_MISSING=207     # 0xCF: EXDOS file-not-found on the open
EXPECT_TRUNC=228       # 0xE4: .EOF from the second read-block

echo "== 2. case 1: GAME.BIN missing"
B1=$(expect_error_border "$PORT/enterprise/loader.com")
[ "$B1" = "$EXPECT_MISSING" ] || { echo "EP-LOADFAIL FAILED (missing GAME.BIN: border $B1, expected $EXPECT_MISSING)" >&2; exit 1; }
echo "   halted with border colour $B1 (EXOS open status, as expected)"

echo "== 3. case 2: GAME.BIN truncated"
head -c 20000 "$PORT/enterprise/game.bin" > GAME.BIN
B2=$(expect_error_border "$PORT/enterprise/loader.com" "$(pwd)/GAME.BIN")
rm -f GAME.BIN
[ "$B2" = "$EXPECT_TRUNC" ] || { echo "EP-LOADFAIL FAILED (truncated GAME.BIN: border $B2, expected $EXPECT_TRUNC)" >&2; exit 1; }
echo "   halted with border colour $B2 (.EOF read status, as expected)"

echo "EP-LOADFAIL PASS (missing => border $B1, truncated => border $B2)"
