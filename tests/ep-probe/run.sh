#!/usr/bin/env bash
# run.sh - build + run the Dave-chip hardware probe in ep128emu.
#
# Produces:
#   probe-screen.png     measured numbers (rates, stretch, segment safety)
#   snap_virgin.ep128s   snapshot taken INSIDE the probe's no-Dave-writes
#                        window -> parse_dave.py prints EXOS's A7h/B4h
#                        convention (probe unknown f)
#   snap_post.ep128s     post-probe state (parser sanity: expect A7h=0x00
#                        and B4h enables snd+1Hz, exactly what the probe
#                        restores)
#
# The probe waits ~10 s (1Hz flip-flop polled passively) after printing
# "SNAP NOW" before its first Dave write; the Ctrl+F9 quick snapshot below
# lands inside that window.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"

# z88dk env - SEPARATE exports (a one-line export expands $Z88DK before
# assignment and silently loses zcc from PATH; cost a session in Phase 3)
export Z88DK="$HOME/retro-computing/spectrum-next/dev/toolchains/z88dk"
export ZCCCFG="$Z88DK/lib/config"
export PATH="$Z88DK/bin:$PATH"

echo "== 1. build probe.com + PROBEDAT.BIN"
python3 - <<'EOF'
open('probedat.bin', 'wb').write(bytes(((i & 0xFF) ^ 0xA7) for i in range(4096)))
EOF
rm -f probe.com
zcc +enterprise -compiler=sdcc -subtype=com probe.c probe_asm.asm \
    -o probe -create-app
[ -f probe.com ] || { echo "run.sh: probe.com not built" >&2; exit 1; }

echo "== 2. boot + load (runcom.sh)"
QS="$HOME/.ep128emu/qs_ep128.dat"
rm -f "$QS" snap_virgin.ep128s snap_post.ep128s probe-screen.png
rm -rf run
OUTDIR="$(pwd)/run" "$DEVTOOLS/runcom.sh" probe.com probedat.bin \
    > run-boot.log 2>&1
echo "   loaded (probe now inside its snapshot window)"

echo "== 3. virgin-state snapshot (EXOS Dave convention)"
sleep 2
DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
    >/dev/null 2>&1 || true
sleep 0.5
cp "$QS" snap_virgin.ep128s
rm -f "$QS"

echo "== 4. wait for the measurements, then capture"
sleep 40
DISPLAY="$XDISP" import -window root probe-screen.png
DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
    >/dev/null 2>&1 || true
sleep 0.5
cp "$QS" snap_post.ep128s || true
pkill -9 -f ep128emu 2>/dev/null || true
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true

echo "== 5. EXOS Dave convention (virgin snapshot)"
python3 parse_dave.py snap_virgin.ep128s
echo "== 6. post-probe state (parser sanity: expect A7h=0x00, snd+1Hz enabled)"
python3 parse_dave.py snap_post.ep128s
echo "== read probe-screen.png for the measured numbers"
