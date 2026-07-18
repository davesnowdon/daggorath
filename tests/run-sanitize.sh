#!/bin/sh
# Replay every scenario under ASan+UBSan with recovery disabled: any
# undefined behaviour or memory error aborts the run and fails the
# gate.  This is the automated form of the check that caught the
# rasterizer's negative-shift UB (fixed 2026-07-18) - golden diffs
# alone cannot see UB that happens to produce the expected bytes.
set -e
cd "$(dirname "$0")"
R=$(cd .. && pwd)
OUT="${TMPDIR:-/tmp}/dod-sanitize.$$"
mkdir -p "$OUT"

gcc -std=c99 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    $(sdl2-config --cflags) -I"$R/core" \
    "$R/desktop/main.c" "$R"/core/*.c -lSDL2 -lm -o "$OUT/dod-san"

run() {
    name=$1; exit_after=$2; keys=$3
    set -- --headless --exit-after "$exit_after" \
        --dump-state "$OUT/$name.dump" --save-file "$OUT/$name.sav"
    [ -n "$keys" ] && set -- "$@" --replay "scenarios/$keys"
    if ! timeout 180 "$OUT/dod-san" "$@" >/dev/null 2>"$OUT/$name.err"; then
        echo "SANITIZE FAIL: $name"
        cat "$OUT/$name.err"
        exit 1
    fi
    echo "clean  $name"
}

run s1-demo    8000  ""
run s2-opening 6000  s2-opening.keys
run s3-combat  38000 s3-combat.keys
run s4-descent 6000  s4-descent.keys

rm -rf "$OUT"
echo "SANITIZE CLEAN (ASan+UBSan, all scenarios)"
