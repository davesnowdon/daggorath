# Enterprise 64/128 build: z88dk +enterprise, zsdcc, EXOS type-5 .com output.
# Same toolchain (with enterprise.cfg) as the Spectrum Next port.
TOOLCHAIN ?= $(HOME)/retro-computing/spectrum-next/dev/toolchains
export Z88DK  := $(TOOLCHAIN)/z88dk
export ZCCCFG := $(Z88DK)/lib/config
export PATH   := $(Z88DK)/bin:$(PATH)

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
# Phase 2: IM1 takeover (isr_ep.asm = jiffies + keyboard) and the RANDOM.ASM
# RNG in Z80 (rng_z80.asm, -DDOD_RNG_ASM - mandatory at 4 MHz; compiles out
# the C rng_RANDOM).  The C line rasterizer in plat_ep.c is still the
# reference until draw_ep.asm (Phase 3) supersedes it.
SRC      := main.c plat_ep.c isr_ep.asm rng_z80.asm $(CORE_SRC)

# Bare crt0: the loader (not EXOS) places the 3 game segments and kills EXOS,
# so the stock EXOS-integrated crt0 (opens VIDEO/KEYBOARD, forces FFh into
# page 2) must NOT run.  bare_crt0.asm keeps only SP/BSS/atexit init + call main.
ZCCFLAGS := +enterprise -compiler=sdcc -subtype=com -crt0=$(CURDIR)/bare_crt0 \
            -SO3 --max-allocs-per-node100000 --opt-code-size -DDOD_RNG_ASM \
            -I$(CORE) -pragma-include:zpragma_ep.inc

# 8.3-safe name (FAT12 base is max 8 chars): DAGGORAT.COM on disk.
daggorat.com: $(SRC) zpragma_ep.inc bare_crt0.asm
	zcc $(ZCCFLAGS) $(SRC) -o daggorat -create-app -m
	@ls -l daggorat.com 2>/dev/null || ls -l daggorat

# The 34.5 KB game exceeds EXOS's ~16 KB single-segment module load, so it is
# shipped as a raw image (game.bin) plus a small loader.com that claims video
# segments FC/FD/FE directly, reads the image across them, and enters the game.
loader.com game.bin: daggorat.com loader.s build-loader.sh
	./build-loader.sh

.PHONY: clean run loader
loader: loader.com

clean:
	rm -f daggorat.com daggorat daggorat.map *.o zcc_opt.def \
	      loader.com game.bin sizes.inc

# build the disk (loader + raw game image), boot ep128emu under Xephyr,
# LOAD "A:LOADER.COM", screenshot.  The loader then loads+starts the game.
DEVTOOLS := $(HOME)/retro-computing/elan-enterprise/dev-tools
run: loader.com game.bin
	OUTDIR=$(CURDIR)/run $(DEVTOOLS)/runcom.sh $(CURDIR)/loader.com $(CURDIR)/game.bin
