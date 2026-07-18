# next-lib.sh - shared ZEsarUX/TBBlue boot for the next-* harnesses.
# Sourced by tests/next-scene, next-sound, next-save run.sh scripts.
#
# next_boot <rootdir> [extra zesarux args...]
#   Boots ZEsarUX headless-ish (private Xephyr) as a Spectrum Next,
#   smartloads <rootdir>/daggorath.nex, maps esxDOS file I/O onto
#   <rootdir> (--esxdos-root-dir: the .sfx blob and daggorath.sav live
#   there as HOST files), and enables the ZRCP remote protocol on port
#   10000.  Returns once ZRCP answers.
#
# next_stop
#   Kills the emulator + Xephyr.

NEXT_EMU="${NEXT_EMU:-$HOME/retro-computing/spectrum-next/emulators/ZEsarUX/zesarux/src/zesarux}"
XDISP="${XDISP:-:2}"
HOSTDISP="${HOSTDISP:-:1}"
NEXT_TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZRCP="$NEXT_TESTS_DIR/next-scene/zrcp.py"

NEXT_EMU_PID=""
NEXT_XEPHYR_PID=""

next_boot() {
    local root="$1"
    shift
    next_stop
    sleep 1
    DISPLAY="$HOSTDISP" Xephyr "$XDISP" -screen 900x700 -ac \
        >"$root/xephyr.log" 2>&1 &
    NEXT_XEPHYR_PID=$!
    sleep 2
    DISPLAY="$XDISP" "$NEXT_EMU" --noconfigfile --nowelcomemessage \
        --nosplash --machine tbblue --realvideo \
        --enable-esxdos-handler --esxdos-root-dir "$root" \
        --enable-remoteprotocol "$@" \
        "$root/daggorath.nex" >"$root/zesarux.log" 2>&1 &
    NEXT_EMU_PID=$!
    local i
    for i in $(seq 1 30); do
        if python3 "$ZRCP" cmd get-version >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    echo "next_boot: ZRCP never answered on port 10000" >&2
    return 1
}

# Kill OUR tracked children (TERM first, then KILL) rather than every
# matching process on the box; the bracket-pattern pkill remains only
# as a stray sweep for crashed earlier runs, and can't match an
# ancestor shell's argv.
next_stop() {
    local p
    for p in $NEXT_EMU_PID $NEXT_XEPHYR_PID; do
        kill -TERM "$p" 2>/dev/null || true
    done
    sleep 0.5
    for p in $NEXT_EMU_PID $NEXT_XEPHYR_PID; do
        kill -9 "$p" 2>/dev/null || true
    done
    NEXT_EMU_PID=""
    NEXT_XEPHYR_PID=""
    pkill -9 -f 'zesaru[x]' 2>/dev/null || true
    pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
}

trap next_stop EXIT
