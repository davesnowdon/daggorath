# MEGA65 build: llvm-mos, .prg output (loads at $2001, BASIC65 SYS stub).
# Needs the toolchain at ~/retro-computing/mega65/dev/llvm-mos.
LLVM_MOS ?= $(HOME)/retro-computing/mega65/dev/llvm-mos
CC := $(LLVM_MOS)/bin/mos-mega65-clang

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
SRC      := main.c plat_mega65.c irq.s $(CORE_SRC)

CFLAGS := -Os -flto -Wall -Wextra -I$(CORE)

daggorath.prg: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -Wl,-Map=daggorath.map

.PHONY: clean run
clean:
	rm -f daggorath.prg daggorath.prg.elf daggorath.map

# boot into xemu with the virtual SD (window opens; close it to stop)
EMU := $(HOME)/retro-computing/mega65/emulators/xemu/build/bin/xmega65.native
ROM := $(HOME)/retro-computing/mega65/dev/open-roms/bin/mega65.rom
run: daggorath.prg
	$(EMU) -besure -hicked 0 -rom $(ROM) -prg daggorath.prg
