# Host-side unit tests for the portable core.
# `make` builds+runs all tests; `make coverage` adds gcovr reporting.
CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g
CORE    := ../core

TESTS   := test_rng test_draw

.PHONY: all clean coverage golden
all: $(TESTS:%=run-%)

golden:
	../tools/make-goldens.sh

test_rng: test_rng.c $(CORE)/rng.c
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $^

test_draw: test_draw.c $(CORE)/draw_ref.c
	$(CC) $(CFLAGS) -I$(CORE) -o $@ $^

run-%: %
	@./$<

coverage: CFLAGS += --coverage
coverage: clean all
	gcovr -r .. --filter '.*core/.*' --print-summary

clean:
	rm -f $(TESTS) *.gcda *.gcno
