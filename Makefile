# Minimal portable build for Codec2-mod (Codec2 3200 bps).
#
#   make                      builds $(BUILD)/libcodec2_mod.a
#   make BUILD=/some/dir      puts all outputs under /some/dir
#   make CC=arm-none-eabi-gcc CFLAGS="-Os -mcpu=cortex-m4" lib
#
# No dependencies other than a C11 compiler and libm.

CC     ?= cc
AR     ?= ar
CFLAGS ?= -O2
BUILD  ?= build

WARN    = -Wall -Wextra -Wpedantic
ALLCFLAGS = -std=c11 $(WARN) $(CFLAGS) -Iinc
LDLIBS  = -lm

LIB_SRCS = $(wildcard src/*.c)
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/obj/%.o,$(LIB_SRCS))
LIB      = $(BUILD)/libcodec2_mod.a

.PHONY: all lib clean

all: lib

lib: $(LIB)

$(BUILD)/obj/%.o: src/%.c $(wildcard inc/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(ALLCFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

clean:
	rm -f $(LIB) $(LIB_OBJS)
