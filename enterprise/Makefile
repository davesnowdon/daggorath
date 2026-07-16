# Enterprise 64/128 build: z88dk +enterprise, zsdcc, EXOS type-5 .com output.
# Same toolchain (with enterprise.cfg) as the Spectrum Next port.
TOOLCHAIN ?= $(HOME)/retro-computing/spectrum-next/dev/toolchains
export Z88DK  := $(TOOLCHAIN)/z88dk
export ZCCCFG := $(Z88DK)/lib/config
export PATH   := $(Z88DK)/bin:$(PATH)

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
# isr_ep.asm = IM1 jiffies + keyboard + PCM sample engine; rng_z80.asm =
# RANDOM.ASM RNG (-DDOD_RNG_ASM, mandatory at 4 MHz); draw_ep.asm = the
# line rasterizer (linear stride, solid H/V fast paths - proven
# pixel-identical by tests/z80draw-ep).  Build with -DDRAW_C_FALLBACK to
# fall back to the C DDA in plat_ep.c.  sound_ep.c = Dave DAC playback
# (blobs + index from tools/gen_sfxep.py, loaded by loader.s).
SRC      := main.c plat_ep.c sound_ep.c isr_ep.asm rng_z80.asm draw_ep.asm \
            $(CORE_SRC)

# Bare crt0: the loader (not EXOS) places the 3 game segments and kills EXOS,
# so the stock EXOS-integrated crt0 (opens VIDEO/KEYBOARD, forces FFh into
# page 2) must NOT run.  bare_crt0.asm keeps only SP/BSS/atexit init + call main.
ZCCFLAGS := +enterprise -compiler=sdcc -subtype=com -crt0=$(CURDIR)/bare_crt0 \
            -SO3 --max-allocs-per-node100000 --opt-code-size \
            -DDOD_RNG_ASM -DDOD_HEART_STATUS_ONLY \
            -I$(CORE) -pragma-include:zpragma_ep.inc

# 8.3-safe name (FAT12 base is max 8 chars): DAGGORAT.COM on disk.
# Post-link placement asserts:
#   * _frame_isr must sit in Z80 page 0 (< 0x4000): the sample ISR remaps
#     page 1 while executing, so it must never live there (isr_ep.asm goes
#     in code_compiler right after plat_ep.c for exactly this reason);
#   * BSS must end below 0x9700: the volume LUT lives at 0x9700-0x97FF
#     and the FE-segment SFX bin at 0x9800-0xBAFF (gen_sfxep.py bakes those
#     addresses into the blob layout; the stack starts at 0xBF00).
daggorat.com: $(SRC) zpragma_ep.inc bare_crt0.asm sfx_index_ep.h
	zcc $(ZCCFLAGS) $(SRC) -o daggorat -create-app -m
	@ls -l daggorat.com 2>/dev/null || ls -l daggorat
	@ISR=$$(grep -E '^_frame_isr\b' daggorat.map | sed -E 's/.*\$$([0-9a-fA-F]+).*/\1/'); \
	 [ -n "$$ISR" ] && [ $$((0x$$ISR)) -lt 16384 ] \
	   || { echo "ASSERT FAILED: _frame_isr at 0x$$ISR is not in page 0" >&2; exit 1; }
	@BSS=$$(grep -E '^__BSS_END_tail\b' daggorat.map | sed -E 's/.*\$$([0-9a-fA-F]+).*/\1/'); \
	 [ -n "$$BSS" ] && [ $$((0x$$BSS)) -le $$((0x9700)) ] \
	   || { echo "ASSERT FAILED: BSS ends at 0x$$BSS, past the 0x9700 LUT slot" >&2; exit 1; }

# SFX blobs + generated index/bin tables (bin-packed, 128-aligned; see
# tools/gen_sfxep.py).  DAGGOR1.SFX = stock EP128, DAGGOR2.SFX = expanded.
DAGGOR1.SFX DAGGOR2.SFX sfx_index_ep.h sfx_bins.inc: ../tools/gen_sfxep.py \
		$(wildcard ../assets/raw-7350/*.raw)
	python3 ../tools/gen_sfxep.py ../assets/raw-7350 .

# The 34.5 KB game exceeds EXOS's ~16 KB single-segment module load, so it is
# shipped as a raw image (game.bin) plus a small loader.com that allocates the
# machine's segments, reads the image across FC/FD/FE, loads the SFX blob,
# and enters the game.
loader.com game.bin: daggorat.com loader.s build-loader.sh sfx_bins.inc
	./build-loader.sh

.PHONY: clean run loader
loader: loader.com

clean:
	rm -f daggorat.com daggorat daggorat.map *.o zcc_opt.def \
	      loader.com game.bin sizes.inc \
	      DAGGOR1.SFX DAGGOR2.SFX sfx_index_ep.h sfx_bins.inc

# build the disk (loader + raw game image + SFX blobs), boot ep128emu under
# Xephyr, LOAD "A:LOADER.COM", screenshot.  The loader then loads+starts the
# game with sound.
DEVTOOLS := $(HOME)/retro-computing/elan-enterprise/dev-tools
run: loader.com game.bin DAGGOR1.SFX DAGGOR2.SFX
	OUTDIR=$(CURDIR)/run $(DEVTOOLS)/runcom.sh $(CURDIR)/loader.com \
	    $(CURDIR)/game.bin $(CURDIR)/DAGGOR1.SFX $(CURDIR)/DAGGOR2.SFX
