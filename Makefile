# Prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
.DEFAULT_GOAL := all
ifeq ($(origin CC), default)
CC := gcc
endif
CC ?= gcc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?=
export CC CPPFLAGS CFLAGS LDFLAGS LDLIBS

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))

.PHONY: all build check-sources clean
all: ifj26
build: all

check-sources:
	@test -n "$(SOURCES)" || { echo 'Chybi zdroje v src/. Nejprve dopln vlastni implementaci.' >&2; exit 1; }

ifj26: $(OBJECTS) | check-sources
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf build dist ifj26

-include $(OBJECTS:.o=.d)
# V odevzdani tools/ neni. Prostemu make to nevadi.
-include tools/development.mk
