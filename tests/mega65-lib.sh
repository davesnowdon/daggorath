# mega65-lib.sh - shared xemu boot for the mega65-* harnesses.
# Sourced by tests/mega65-scene, mega65-save, mega65-sound run.sh.
#
# m65_boot <rundir>
#   Boots xemu headless with open-roms, a virtual SD fed from
#   <rundir>/sd (put DAGGOR65.SFX + DAGGOR65.SAV there - the FAT is
#   in-memory, host files are read at boot but writes do NOT come
#   back), and the uartmon socket at <rundir>/mon.sock.  Accepts the
#   first-boot on-boarding screen, then injects daggorath.prg via
#   m65mon.py (open-roms can't -prg autoload).
#
# m65 <args...>   m65mon.py against the booted socket
# m65_stop        kill xemu

M65_EMU="${M65_EMU:-$HOME/retro-computing/mega65/emulators/xemu/build/bin/xmega65.native}"
M65_ROM="${M65_ROM:-$HOME/retro-computing/mega65/dev/open-roms/bin/mega65.rom}"
M65_TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
M65MON="$M65_TESTS_DIR/../mega65/m65mon.py"
M65_SOCK=""

m65() {
    python3 "$M65MON" "$M65_SOCK" "$@"
}

M65_PID=""

m65_boot() {
    local rundir="$1"
    M65_SOCK="$rundir/mon.sock"
    m65_stop
    sleep 1
    "$M65_EMU" -headless -besure -hicked 0 -rom "$M65_ROM" \
        -uartmon "$M65_SOCK" -sdimg "$rundir/sd" -virtsd \
        >"$rundir/xemu.log" 2>&1 &
    M65_PID=$!
    local i
    for i in $(seq 1 30); do
        [ -S "$M65_SOCK" ] && break
        sleep 1
    done
    [ -S "$M65_SOCK" ] || { echo "m65_boot: no uartmon socket" >&2; return 1; }
    sleep 3
    m65 cmd @key:1 >/dev/null       # accept the on-boarding screen
    sleep 2
    m65 inject "$M65_TESTS_DIR/../mega65/daggorath.prg"
}

# Kill OUR tracked child (TERM first, then KILL); the bracket-pattern
# pkill remains only as a stray sweep for crashed earlier runs.
m65_stop() {
    if [ -n "$M65_PID" ]; then
        kill -TERM "$M65_PID" 2>/dev/null || true
        sleep 0.5
        kill -9 "$M65_PID" 2>/dev/null || true
        M65_PID=""
    fi
    pkill -9 -f 'xmega65.nativ[e]' 2>/dev/null || true
}

trap m65_stop EXIT
