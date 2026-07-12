#!/bin/sh
# Line coverage of core/ from the deterministic scenario replays.
# gcovr is not installable on this host (pip SSL is broken), so this
# builds a --coverage shim binary, replays every scenario, and
# aggregates plain `gcov` output.  Phase-1 exit target: >= 80%.
set -e
cd "$(dirname "$0")"
R=$(cd .. && pwd)
OUT="${TMPDIR:-/tmp}/dod-coverage.$$"
mkdir -p "$OUT"
cd "$OUT"

gcc -std=c99 --coverage -O0 -g $(sdl2-config --cflags) -I"$R/core" \
    "$R/desktop/main.c" "$R"/core/*.c -lSDL2 -o dodcov

./dodcov --headless --exit-after 8000  --dump-state /dev/null >/dev/null 2>&1 || true
./dodcov --headless --exit-after 6500 --replay "$R/tests/scenarios/s2-opening.keys" \
    --dump-state /dev/null --save-file "$OUT/c.sav" >/dev/null 2>&1 || true
./dodcov --headless --exit-after 38000 --replay "$R/tests/scenarios/s3-combat.keys" \
    --dump-state /dev/null >/dev/null 2>&1 || true
./dodcov --headless --exit-after 6000 --replay "$R/tests/scenarios/s4-descent.keys" \
    --dump-state /dev/null >/dev/null 2>&1 || true

for g in *.gcda; do gcov -o . "$g" 2>/dev/null; done | awk '
/^File .*core\// { f=$0; sub(/.*core\//,"",f); sub(/'"'"'$/,"",f); want=1; next }
/^File/ { want=0 }
/^Lines executed/ && want {
    split($0,p,"[:% ]+");
    printf "%-18s %7.2f%%  %5d lines\n", f, p[3], p[5];
    tot+=p[5]; cov+=p[3]/100*p[5]; want=0
}
END { printf "%-18s %7.2f%%  %5d lines\n", "TOTAL core/", 100*cov/tot, tot }' \
    | sort -k2 -n
cd / && rm -rf "$OUT"
