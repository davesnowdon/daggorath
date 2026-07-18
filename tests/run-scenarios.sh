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

REGEN=0
[ "$1" = "--regen" ] && REGEN=1
FAILED=0

run s1-demo    8000  ""
run s2-opening 6000  s2-opening.keys
run s3-combat  38000 s3-combat.keys
run s4-descent 6000  s4-descent.keys

if [ "$REGEN" = 1 ]; then
    echo "goldens rewritten under $GOLD - diff before committing"
elif [ "$FAILED" = 1 ]; then
    echo "SCENARIOS FAILED"; exit 1
else
    rm -rf "$OUT"
    echo "SCENARIOS IDENTICAL"
fi
