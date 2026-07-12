CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -O2 -g $(shell sdl2-config --cflags)
LDLIBS  := $(shell sdl2-config --libs)
CORE    := ../core

# core objects that exist and compile today; game modules join as they land
CORE_SRC := $(CORE)/rng.c $(CORE)/draw_ref.c \
            $(CORE)/tables_vec.c $(CORE)/tables_font.c $(CORE)/tables_text.c

daggorath: main.c $(CORE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f daggorath
