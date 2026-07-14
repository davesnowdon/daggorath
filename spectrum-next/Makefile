# Spectrum Next build: z88dk newlib, zsdcc, .nex output.
# Needs the toolchain at ~/retro-computing/spectrum-next/dev/toolchains.
TOOLCHAIN ?= $(HOME)/retro-computing/spectrum-next/dev/toolchains
export Z88DK  := $(TOOLCHAIN)/z88dk
export ZCCCFG := $(Z88DK)/lib/config
export PATH   := $(Z88DK)/bin:$(PATH)

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
SRC      := main.c plat_next.c isr.asm $(CORE_SRC)

# -SO3 + max-allocs is the z88dk-recommended aggressive setting; drop to
# plain -SO2 if build time hurts during iteration.
ZCCFLAGS := +zxn -compiler=sdcc -clib=sdcc_iy -subtype=nex \
            -SO3 --max-allocs-per-node100000 --opt-code-size \
            -I$(CORE) -pragma-include:zpragma.inc

daggorath.nex: $(SRC) zpragma.inc
	zcc $(ZCCFLAGS) $(SRC) -o daggorath -create-app -m

.PHONY: clean run
clean:
	rm -f daggorath.nex daggorath *.o

# smartload into ZEsarUX (window opens; close it to stop)
EMU := $(HOME)/retro-computing/spectrum-next/emulators/ZEsarUX/zesarux/zesarux
run: daggorath.nex
	$(EMU) --machine tbblue --realvideo --quickexit-fileselector \
	       daggorath.nex
