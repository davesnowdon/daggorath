CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -O2 -g $(shell sdl2-config --cflags)
LDLIBS  := $(shell sdl2-config --libs)
CORE    := ../core

CORE_SRC := $(wildcard $(CORE)/*.c)

daggorath: main.c $(CORE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f daggorath
