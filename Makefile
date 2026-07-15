# Top-level convenience targets.
#   make            build every platform
#   make check      the full verification gate (unit + A/B + scenarios)
#   make release    build + copy playable sets into the per-system folders
NEXT_DEST ?= $(HOME)/retro-computing/spectrum-next/games/Daggorath
M65_DEST  ?= $(HOME)/retro-computing/mega65/daggorath

.PHONY: all desktop next mega65 check release clean
all: desktop next mega65

desktop:
	$(MAKE) -C desktop

next:
	$(MAKE) -C spectrum-next

mega65:
	$(MAKE) -C mega65 daggorath.prg daggorath.d81 DAGGOR65.SAV

check:
	$(MAKE) -C tests check

release: next mega65
	mkdir -p $(NEXT_DEST) $(M65_DEST)
	cp spectrum-next/daggorath.nex spectrum-next/daggorath.sfx \
	   release/CONTROLS.txt $(NEXT_DEST)/
	cp release/README-next.txt $(NEXT_DEST)/README.txt
	cp mega65/daggorath.prg mega65/daggorath.d81 mega65/DAGGOR65.SFX \
	   mega65/DAGGOR65.SAV release/CONTROLS.txt $(M65_DEST)/
	cp release/README-mega65.txt $(M65_DEST)/README.txt
	@echo "release copied to $(NEXT_DEST) and $(M65_DEST)"

clean:
	$(MAKE) -C desktop clean
	$(MAKE) -C spectrum-next clean
	$(MAKE) -C mega65 clean
	$(MAKE) -C tests clean
