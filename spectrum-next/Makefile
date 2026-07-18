# Spectrum Next build: z88dk newlib, zsdcc, .nex output.
# Needs the toolchain at ~/retro-computing/spectrum-next/dev/toolchains.
TOOLCHAIN ?= $(HOME)/retro-computing/spectrum-next/dev/toolchains
export Z88DK  := $(TOOLCHAIN)/z88dk
export ZCCCFG := $(Z88DK)/lib/config
export PATH   := $(Z88DK)/bin:$(PATH)

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
SRC      := main.c plat_next.c sound_next.c isr.asm draw_z80n.asm \
            snd_scale.asm rng_z80.asm $(CORE_SRC)

# -SO3 + max-allocs is the z88dk-recommended aggressive setting; drop to
# plain -SO2 if build time hurts during iteration.
# -DDOD_RNG_ASM swaps in the asm rng_RANDOM (rng_z80.asm, proven
# byte-identical by tests/z80rng/run.sh); remove the define AND
# rng_z80.asm from SRC to fall back to the C RNG.
ZCCFLAGS := +zxn -compiler=sdcc -clib=sdcc_iy -subtype=nex \
            -SO3 --max-allocs-per-node100000 --opt-code-size \
            -DDOD_RNG_ASM \
            -I$(CORE) -pragma-include:zpragma.inc

daggorath.nex: $(SRC) zpragma.inc sfx_index.h daggorath.sfx
	zcc $(ZCCFLAGS) $(SRC) -o daggorath -create-app -m

# SFX blob + index from the 11025 Hz raws (regenerate the raws from
# assets/sfx-src with tools/wav2raw.c, factor 2)
SFXDIR := ../assets/raw-11025
# grouped target (&:, GNU make 4.3+): ONE generator run produces both
# files; the plain 'a b:' form runs the recipe once per target under
# -j and the concurrent writers race
daggorath.sfx sfx_index.h &: ../tools/gen_sfxbin.py $(wildcard $(SFXDIR)/*.raw)
	python3 ../tools/gen_sfxbin.py $(SFXDIR) daggorath.sfx sfx_index.h

.PHONY: clean run
clean:
	rm -f daggorath.nex daggorath *.o

# smartload into ZEsarUX (window opens; close it to stop)
EMU := $(HOME)/retro-computing/spectrum-next/emulators/ZEsarUX/zesarux/src/zesarux
run: daggorath.nex
	$(EMU) --machine tbblue --realvideo \
	       --enable-esxdos-handler --esxdos-root-dir $(CURDIR) \
	       daggorath.nex
