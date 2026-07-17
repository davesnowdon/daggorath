# Top-level convenience targets.
#   make            build every platform
#   make check      the full verification gate (unit + A/B + scenarios)
#   make check-all  check + every backend harness in tests/*/run.sh
#                   (identity tests, emulator scene/sound/save/loadfail
#                   suites; each SKIPs loudly if its toolchain/emulator
#                   is absent)
#   make release    build + copy playable sets into the per-system folders
NEXT_DEST ?= $(HOME)/retro-computing/spectrum-next/games/Daggorath
M65_DEST  ?= $(HOME)/retro-computing/mega65/daggorath
EP_DEST   ?= $(HOME)/retro-computing/elan-enterprise/games/Daggorath

# check-all harnesses: ordered cheap-first (pure-CPU identity tests,
# then the emulator suites).  Each entry is dir:prerequisite-command.
HARNESSES := \
    z80draw:zcc z80rng:zcc z80scale:zcc z80draw-ep:zcc z80rng-ep:zcc \
    ep-scene:ep128emu-check ep-sound:ep128emu-check ep-save:ep128emu-check \
    ep-loadfail:ep128emu-check \
    next-scene:zesarux-check next-sound:zesarux-check next-save:zesarux-check \
    mega65-scene:xemu-check mega65-sound:xemu-check mega65-save:xemu-check

EP_EMU_BIN  ?= $(HOME)/retro-computing/elan-enterprise/emulators/ep128emu/ep128emu-2.0.11.2/ep128emu
ZESARUX_BIN ?= $(HOME)/retro-computing/spectrum-next/emulators/ZEsarUX/zesarux/src/zesarux
XEMU_BIN    ?= $(HOME)/retro-computing/mega65/emulators/xemu/build/bin/xmega65.native
Z88DK_BIN   ?= $(HOME)/retro-computing/spectrum-next/dev/toolchains/z88dk/bin/zcc

.PHONY: all desktop next mega65 enterprise check check-all release clean
all: desktop next mega65 enterprise

desktop:
	$(MAKE) -C desktop

next:
	$(MAKE) -C spectrum-next

mega65:
	$(MAKE) -C mega65 daggorath.prg daggorath.d81 DAGGOR65.SAV

enterprise:
	$(MAKE) -C enterprise loader.com game.bin daggorath-ep.img

check:
	$(MAKE) -C tests check

check-all: check
	@mkdir -p tests/check-logs; fail=0; \
	for entry in $(HARNESSES); do \
	    dir=$${entry%%:*}; kind=$${entry##*:}; \
	    case $$kind in \
	        zcc)            need="$(Z88DK_BIN)";; \
	        ep128emu-check) need="$(EP_EMU_BIN)";; \
	        zesarux-check)  need="$(ZESARUX_BIN)";; \
	        xemu-check)     need="$(XEMU_BIN)";; \
	    esac; \
	    log=tests/check-logs/$$dir.log; \
	    if [ ! -x "$$need" ]; then \
	        echo "== SKIP tests/$$dir (missing $$need)"; continue; \
	    fi; \
	    echo "== RUN  tests/$$dir"; \
	    if ( cd tests/$$dir && ./run.sh ) >"$$log" 2>&1; then \
	        tail -1 "$$log"; \
	    else \
	        echo "== FAIL tests/$$dir (log: $$log)"; \
	        tail -5 "$$log"; fail=1; \
	    fi; \
	done; \
	[ $$fail -eq 0 ] && echo "CHECK-ALL PASS" || { echo "CHECK-ALL FAIL"; exit 1; }

release: next mega65 enterprise
	mkdir -p $(NEXT_DEST) $(M65_DEST) $(EP_DEST)
	cp spectrum-next/daggorath.nex spectrum-next/daggorath.sfx \
	   release/CONTROLS.txt $(NEXT_DEST)/
	cp release/README-next.txt $(NEXT_DEST)/README.txt
	cp mega65/daggorath.prg mega65/daggorath.d81 mega65/DAGGOR65.SFX \
	   mega65/DAGGOR65.SAV release/CONTROLS.txt $(M65_DEST)/
	cp release/README-mega65.txt $(M65_DEST)/README.txt
	cp enterprise/loader.com enterprise/game.bin enterprise/DAGGOR1.SFX \
	   enterprise/DAGGOR2.SFX enterprise/daggorath-ep.img \
	   release/CONTROLS.txt $(EP_DEST)/
	cp release/README-enterprise.txt $(EP_DEST)/README.txt
	@echo "release copied to $(NEXT_DEST), $(M65_DEST) and $(EP_DEST)"

clean:
	$(MAKE) -C desktop clean
	$(MAKE) -C spectrum-next clean
	$(MAKE) -C mega65 clean
	$(MAKE) -C enterprise clean
	$(MAKE) -C tests clean
