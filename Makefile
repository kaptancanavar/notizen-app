# Notizen-App fuer Kindle Scribe
.RECIPEPREFIX = >

TC      := $(HOME)/x-tools/arm-kindlehf-linux-gnueabihf/bin/arm-kindlehf-linux-gnueabihf
CC      := $(TC)-gcc
CFLAGS  := -O2 -g -Wall -Wextra -IFBInk
FBLIB   := FBInk/Release/libfbink.a
LDLIBS  := $(FBLIB) -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: notizen

notizen: $(OBJ) $(FBLIB)
> $(CC) -o $@ $(OBJ) $(LDLIBS)
> @file $@

build/%.o: src/%.c src/app.h
> @mkdir -p build
> $(CC) $(CFLAGS) -c $< -o $@

clean:
> rm -rf build notizen

.PHONY: all clean
