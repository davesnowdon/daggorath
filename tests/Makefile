# Host-side unit tests for the portable core.
#   make             build+run the unit tests
#   make ab-modules  module-level A/B vs the C++ port's code verbatim
#   make scenarios   replay the scripted scenarios vs stored state goldens
#   make check       all of the above (the full Phase-1 gate)
#   make coverage    line coverage of core/ from the scenario replays (gcov)
CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g
CORE    := ../core

TESTS   := test_rng test_draw test_scale

.PHONY: all clean coverage golden scenarios sanitize check ab-modules
all: $(TESTS:%=run-%)

check: all ab-modules scenarios sanitize

golden:
	../tools/make-goldens.sh

test_rng: test_rng.c $(CORE)/rng.c
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $^

test_draw: test_draw.c $(CORE)/draw_ref.c
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $^

# links viewer_list only; provides its own viewer_state and plat stub
test_scale: test_scale.c $(CORE)/viewer_list.c stub_plat_line.c
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $^

run-%: %
	@./$<

# Scenario regression: deterministic replays through the desktop shim,
# state dumped at every command prompt, diffed against goldens.
# Regenerate (after an intentional behavior change) with
#   ./run-scenarios.sh --regen
scenarios:
	./run-scenarios.sh

# The same replays under ASan+UBSan (no recover): catches UB-class
# defects the golden diffs cannot (e.g. the negative-shift fix).
sanitize:
	./run-sanitize.sh

# Line coverage of core/ from the scenario replays (gcovr is not
# installable on this host; plain gcov + awk).  Phase-1 target: >= 80%.
coverage:
	./run-coverage.sh

clean:
	rm -f $(TESTS) ab_c ab_ref *.out *.gcda *.gcno *.c.gcov

# The same harness compiled by llvm-mos and run on a simulated 6502:
# output must be identical to the host build's (MEGA65 codegen check).
LLVM_MOS ?= $(HOME)/retro-computing/mega65/dev/llvm-mos
ab-mos: ab-modules
	$(LLVM_MOS)/bin/mos-sim-clang -Os -I$(CORE) -o ab_mos \
	    ab_modules_c.c ab_stubs.c $(CORE)/dungeon.c $(CORE)/parser.c \
	    $(CORE)/rng.c $(CORE)/object.c $(CORE)/creature.c \
	    $(CORE)/sched.c $(CORE)/player.c
	$(LLVM_MOS)/bin/mos-sim ab_mos > ab_mos.out
	diff ab_mos.out ab_c.out && echo "AB-MOS IDENTICAL"

# Module-level A/B: extracted C dungeon/parser/object/creature/sched/
# player-math vs the port's code verbatim.  Both dump CRC-tagged state;
# outputs must be identical.  The reference side needs the C++ PC-Port
# checkout (an external input, not vendored - see LICENSE.md); when it
# is absent, our side still builds and runs (ab-mos needs its output)
# but the reference diff is skipped with a warning.
PORT_SRC := ../../../native-port/DungeonsOfDaggorath/src
ab-modules: ab_modules_c.c ab_stubs.c $(CORE)/dungeon.c $(CORE)/parser.c \
            $(CORE)/rng.c $(CORE)/object.c $(CORE)/creature.c \
            $(CORE)/sched.c $(CORE)/player.c
	@set -e; \
	$(CC) $(CFLAGS) -Wno-unused-parameter -I$(CORE) -o ab_c $^; \
	./ab_c > ab_c.out; \
	if [ ! -d $(PORT_SRC) ]; then \
	    echo "== SKIP ab-modules reference diff (C++ port not at" \
	         "$(PORT_SRC) - clone the Daggorath PC-Port there)"; \
	else \
	    g++ -O1 -w -o ab_ref ../tools/ab_modules_ref.cpp \
	        -I$(PORT_SRC) $$(sdl2-config --cflags) -I$$HOME/.local/include; \
	    ./ab_ref > ab_ref.out; \
	    diff ab_c.out ab_ref.out && echo "AB-MODULES IDENTICAL"; \
	fi
