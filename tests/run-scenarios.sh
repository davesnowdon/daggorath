#!/bin/sh
# Scenario regression harness: replay each scripted scenario through the
# desktop shim under the deterministic virtual clock (--headless implies
# --turbo) and diff the command-boundary state dumps against the stored
# goldens.  "$1 = --regen" rewrites the goldens instead of diffing
# (review the diff before committing regenerated goldens!).
set -e
cd "$(dirname "$0")"
BIN=../desktop/daggorath
GOLD=golden/scenarios
OUT="${TMPDIR:-/tmp}/dod-scenarios.$$"
mkdir -p "$OUT" "$GOLD"

make -C ../desktop >/dev/null

run() {
    name=$1; exit_after=$2; keys=$3
    set -- --headless --exit-after "$exit_after" \
        --dump-state "$OUT/$name.dump" --save-file "$OUT/$name.sav"
    [ -n "$keys" ] && set -- "$@" --replay "scenarios/$keys"
    # the game's own exit status GATES: a crash after the final dump
    # would otherwise pass on the golden diff alone
    if ! timeout 120 "$BIN" "$@" >/dev/null 2>&1; then
        echo "FAIL   $name (nonzero exit or timeout)"; FAILED=1; return 0
    fi
    if [ "$REGEN" = 1 ]; then
        cp "$OUT/$name.dump" "$GOLD/$name.golden"
        echo "regen  $name ($(grep -c '^===' "$GOLD/$name.golden") dumps)"
    else
        diff -u "$GOLD/$name.golden" "$OUT/$name.dump" > "$OUT/$name.diff" || {
            echo "FAIL   $name (see $OUT/$name.diff)"; FAILED=1; return 0;
        }
        echo "ok     $name ($(grep -c '^===' "$OUT/$name.dump") dumps)"
    fi
}

# behavioral save/load: phase a plays + ZSAVEs and continues; phase b
# is a FRESH process that ZLOADs the file and plays on (restored state
# must behave, not merely round-trip bytes); phase c replays the load
# against a checksum-corrupted copy - the envelope must reject it and
# the unloaded fresh game plays on.  All three phases golden-gated.
saveload_phase() {
    name=$1; keys=$2; sav=$3
    if ! timeout 120 "$BIN" --headless --exit-after 6000 \
        --dump-state "$OUT/$name.dump" --save-file "$sav" \
        --replay "scenarios/$keys" >/dev/null 2>&1; then
        echo "FAIL   $name (nonzero exit or timeout)"; FAILED=1; return 1
    fi
    if [ "$REGEN" = 1 ]; then
        cp "$OUT/$name.dump" "$GOLD/$name.golden"
        echo "regen  $name ($(grep -c '^===' "$GOLD/$name.golden") dumps)"
    else
        diff -u "$GOLD/$name.golden" "$OUT/$name.dump" > "$OUT/$name.diff" || {
            echo "FAIL   $name (see $OUT/$name.diff)"; FAILED=1; return 1;
        }
        echo "ok     $name ($(grep -c '^===' "$OUT/$name.dump") dumps)"
    fi
}

run_saveload() {
    sav="$OUT/s5.sav"
    saveload_phase s5-save s5-save.keys "$sav" || return 0
    saveload_phase s5-load s5-load.keys "$sav" || return 0
    python3 -c "
d = bytearray(open('$sav','rb').read())
d[6] ^= 0xFF
open('$sav.bad','wb').write(bytes(d))
"
    saveload_phase s5-corrupt s5-load.keys "$sav.bad" || return 0
}

REGEN=0
[ "$1" = "--regen" ] && REGEN=1
FAILED=0

run s1-demo    8000  ""
run s2-opening 6000  s2-opening.keys
run s3-combat  38000 s3-combat.keys
run s4-descent 6000  s4-descent.keys
run_saveload

if [ "$REGEN" = 1 ]; then
    echo "goldens rewritten under $GOLD - diff before committing"
elif [ "$FAILED" = 1 ]; then
    echo "SCENARIOS FAILED"; exit 1
else
    rm -rf "$OUT"
    echo "SCENARIOS IDENTICAL"
fi
