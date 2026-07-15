#!/usr/bin/env bash
# run.sh - EP fixed-scene golden: byte-exact framebuffer identity between
# the Enterprise build (running in ep128emu) and the desktop reference.
#
# Both sides run the ATTRACT DEMO, which is jiffy-deterministic end to end
# (fixed maze, scripted commands, RNG from fixed seeds - no real-time
# input): at any given jiffy both render the same frame.  The desktop side
# dumps present-boundary frames for every 2nd jiffy of a wide demo window
# (--headless --turbo --screenshot: virtual clock, deterministic); the EP
# side boots the real loader+game in ep128emu and takes several Ctrl+F9
# quick snapshots across the same window on the wall clock (60 jiffies/s).
# compare_scene.py then extracts each snapshot's FRONT framebuffer (via
# the LPT LD1 pointer - always a complete presented frame) and requires it
# to match SOME desktop golden byte-for-byte: any rasterizer, layout,
# paging or double-buffer divergence makes it match nothing.
#
# The matched jiffies double as the Phase-3 timing stopwatch: their spacing
# against the snapshot wall-clock spacing measures the EP's jiffy rate
# (the isr_ep.asm 50->60 Hz accumulator; expect ~60/s).
#
# Window: jiffies 2400-3400 of the demo - the lit room + spider encounter,
# the most draw-heavy stretch of the attract loop.
set -euo pipefail
cd "$(dirname "$0")"

PORT=$(cd ../.. && pwd)
DEVTOOLS="$HOME/retro-computing/elan-enterprise/dev-tools"
XDISP="${XDISP:-:2}"
# Wide window: the EP's game clock is wall-true (ISR) while its scheduler
# reschedules relative to "now", so slow frames make the EP demo drift a
# few seconds behind the desktop's virtual-clock replay by mid-demo.  The
# EP frame still equals SOME desktop frame exactly - just at an earlier
# desktop jiffy - so goldens span generously around the snapshot window.
# (The game enters ~17s before runcom.sh returns: its final waits run
# after the loader has already started the game.)
J_LO=2000
J_HI=4400
J_STEP=2
SNAP_FIRST=26         # seconds after boot-harness completion for 1st snap
SNAP_N=8
SNAP_GAP=2.2          # seconds between snapshots

echo "== 1. desktop goldens (jiffies $J_LO..$J_HI step $J_STEP)"
make -C "$PORT/desktop" >/dev/null
mkdir -p golden-frames
rm -f golden-frames/g*.pgm
for (( j = J_LO; j <= J_HI; j += J_STEP )); do
    "$PORT/desktop/daggorath" --headless \
        --screenshot "$j" "golden-frames/g$j.pgm" >/dev/null 2>&1
done
echo "   $(ls golden-frames/g*.pgm | wc -l) frames dumped"

echo "== 2. EP build + boot (ep128emu on $XDISP)"
make -C "$PORT/enterprise" loader.com game.bin >/dev/null
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
pkill -9 -f ep128emu 2>/dev/null || true
sleep 1
rm -rf run
OUTDIR="$(pwd)/run" "$DEVTOOLS/runcom.sh" \
    "$PORT/enterprise/loader.com" "$PORT/enterprise/game.bin" \
    > run-boot.log 2>&1
T0=$(date +%s.%N)
echo "   booted (game entering title fade)"

echo "== 3. quick snapshots across the demo window"
# ep128emu quick save (Ctrl+F9) with no configured name writes
# qs_ep128.dat under its home directory.
QS_CANDIDATES=("$HOME/.ep128emu/qs_ep128.dat" "$HOME/qs_ep128.dat")
rm -f "${QS_CANDIDATES[@]}" snap_*.ep128s snap_times.txt
sleep "$SNAP_FIRST"
for (( i = 0; i < SNAP_N; ++i )); do
    DISPLAY="$XDISP" "$DEVTOOLS/xhotkey" "ep128emu" Control_L F9 \
        >/dev/null 2>&1 || true
    sleep 0.4
    for q in "${QS_CANDIDATES[@]}"; do
        if [ -f "$q" ]; then
            cp "$q" "snap_$i.ep128s"
            rm -f "$q"
            echo "$i $(date +%s.%N)" >> snap_times.txt
            break
        fi
    done
    [ -f "snap_$i.ep128s" ] || echo "   snapshot $i: no file appeared" >&2
    sleep "$SNAP_GAP"
done
pkill -9 -f ep128emu 2>/dev/null || true
pkill -9 -f "Xephyr $XDISP" 2>/dev/null || true
N_SNAPS=$(ls snap_*.ep128s 2>/dev/null | wc -l)
echo "   $N_SNAPS snapshots captured"
if [ "$N_SNAPS" -lt 2 ]; then
    echo "run.sh: not enough snapshots (Ctrl+F9 path broken?)" >&2
    exit 2
fi

echo "== 4. verdict (byte-exact framebuffer vs desktop goldens)"
# _isr_jiffies' Z80 address (from the game map) lets compare_scene.py read
# the EP's OWN jiffy counter out of each snapshot for the stopwatch.
JADDR=$(grep -E '^_isr_jiffies\b' "$PORT/enterprise/daggorat.map" \
        | head -1 | sed -E 's/.*\$([0-9a-fA-F]+).*/0x\1/')
python3 compare_scene.py snap_*.ep128s --goldens golden-frames \
    ${JADDR:+--jiffy-addr "$JADDR"} | tee verdict.txt
grep -q 'EP-SCENE IDENTICAL' verdict.txt

echo "== 5. jiffy-rate stopwatch (EP jiffy counter vs wall clock)"
python3 - <<'EOF'
times = {}
for line in open('snap_times.txt'):
    i, t = line.split()
    times[int(i)] = float(t)
pairs = []
for line in open('verdict.txt'):
    if line.startswith('EP_JIFFIES'):
        for tok in line.split()[1:]:
            snap, j = tok.split(':')
            pairs.append((int(snap), int(j)))
pairs.sort()
if len(pairs) >= 2:
    (s0, j0), (s1, j1) = pairs[0], pairs[-1]
    dt = times[s1] - times[s0]
    dj = (j1 - j0) & 0xFFFF          # jiffy_t is uint16, may wrap
    rate = dj / dt if dt > 0 else 0
    print(f'   {dj} jiffies over {dt:.2f}s wall = {rate:.1f} jiffies/s '
          f'(target 60; PAL-frame ISR accumulator)')
    if not (54 <= rate <= 66):
        print('   WARNING: rate outside 54..66 - check the ISR accumulator')
        raise SystemExit(1)
else:
    print('   (fewer than 2 EP-jiffy readings - stopwatch skipped)')
EOF
echo "EP-SCENE PASS"
