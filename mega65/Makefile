# MEGA65 build: llvm-mos, .prg output (loads at $2001, BASIC65 SYS stub).
# Needs the toolchain at ~/retro-computing/mega65/dev/llvm-mos.
LLVM_MOS ?= $(HOME)/retro-computing/mega65/dev/llvm-mos
CC := $(LLVM_MOS)/bin/mos-mega65-clang

# mega65-libc: only src/llvm/fileio.s (hyppo file access) is used
LIBC65 ?= $(HOME)/retro-computing/mega65/dev/mega65-libc

CORE     := ../core
CORE_SRC := $(wildcard $(CORE)/*.c)
SRC      := main.c plat_mega65.c sound_mega65.c irq.s hyppo_write.s \
            layout.s $(LIBC65)/src/llvm/fileio.s $(CORE_SRC)

CFLAGS := -Os -flto -Wall -Wextra -I$(CORE) -I$(LIBC65)/include

daggorath.prg: $(SRC) sfx_index65.h
	$(CC) $(CFLAGS) -o $@ $(filter-out sfx_index65.h,$(SRC)) \
	    -Wl,-Map=daggorath.map

# SFX blob + index at 7350 Hz (= 22050/3; regenerate the raws from
# assets/sfx-src with tools/wav2raw.c, factor 3).  DAGGOR65.SFX must
# sit in the SD root (xemu: the -virtsd directory) beside the .prg.
SFXDIR := ../assets/raw-7350
DAGGOR65.SFX sfx_index65.h: ../tools/gen_sfxbin.py $(wildcard $(SFXDIR)/*.raw)
	python3 ../tools/gen_sfxbin.py $(SFXDIR) DAGGOR65.SFX sfx_index65.h 7350

# distributable disk image (PRG inside; DAGGOR65.SFX still goes in the
# SD card's FAT root - hyppo reads it from there, not from the D81)
daggorath.d81: daggorath.prg ../tools/make_d81.py
	python3 ../tools/make_d81.py $@ DAGGORATH daggorath.prg DAGGORATH

# empty pre-sized save slot: hyppo's writefile can only overwrite an
# existing file, so this goes in the SD FAT root beside the .sfx
DAGGOR65.SAV:
	dd if=/dev/zero of=$@ bs=4096 count=1 2>/dev/null

.PHONY: clean run
clean:
	rm -f daggorath.prg daggorath.prg.elf daggorath.map daggorath.d81 \
	      DAGGOR65.SFX sfx_index65.h

# boot into xemu with the virtual SD (window opens; close it to stop)
EMU := $(HOME)/retro-computing/mega65/emulators/xemu/build/bin/xmega65.native
ROM := $(HOME)/retro-computing/mega65/dev/open-roms/bin/mega65.rom
run: daggorath.prg
	$(EMU) -besure -hicked 0 -rom $(ROM) -prg daggorath.prg
