# Minimal portable build for Codec2-mod (Codec2 3200 bps).
#
#   make                      builds $(BUILD)/libcodec2_mod.a, c2m_enc, c2m_dec and the
#                             tests c2m_test, c2m_public_only
#   make lib                  only the static archive
#   make BUILD=/some/dir      puts all outputs under /some/dir
#   make CC=arm-none-eabi-gcc CFLAGS="-Os -mcpu=cortex-m4" lib
#   make check INPUT=speech.raw REF_BIT=speech.bit [REF_PCM=speech_dec.raw]
#                             runs c2m_test (REF_BIT e.g. from "c2enc 3200") and
#                             c2m_public_only
#   make CFLAGS="-O2 -DCODEC2_MOD_PREFIX=c2m_"
#                             optional symbol prefix (see inc/codec2_mod_prefix.h)
#
# No dependencies other than a C11 compiler and libm. c2m_public_only is
# compiled as C99 to check that the clean-header mode stays C99 clean.

CC     ?= cc
AR     ?= ar
CFLAGS ?= -O2
BUILD  ?= build

WARN    = -Wall -Wextra -Wpedantic
ALLCFLAGS = -std=c11 $(WARN) $(CFLAGS) -Iinc
C99FLAGS  = -std=c99 $(WARN) $(CFLAGS) -Iinc
HDRS     = $(wildcard inc/*.h)
LDLIBS  = -lm

LIB_SRCS = $(wildcard src/*.c)
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/obj/%.o,$(LIB_SRCS))
LIB      = $(BUILD)/libcodec2_mod.a

TOOLS = $(BUILD)/c2m_enc $(BUILD)/c2m_dec
TESTS = $(BUILD)/c2m_test $(BUILD)/c2m_public_only

.PHONY: all lib tools test check clean

all: lib tools test

tools: $(TOOLS)

test: $(TESTS)

lib: $(LIB)

$(BUILD)/obj/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(ALLCFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD)/c2m_enc: tools/c2m_enc.c $(LIB) $(HDRS)
	$(CC) $(ALLCFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD)/c2m_dec: tools/c2m_dec.c $(LIB) $(HDRS)
	$(CC) $(ALLCFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD)/c2m_test: tests/c2m_test.c $(LIB) $(HDRS)
	$(CC) $(ALLCFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(BUILD)/c2m_public_only: tests/c2m_public_only.c $(LIB) $(HDRS)
	$(CC) $(C99FLAGS) $< $(LIB) $(LDLIBS) -o $@

check: $(TESTS)
	@test -n "$(INPUT)" -a -n "$(REF_BIT)" || { echo "usage: make check INPUT=speech.raw REF_BIT=speech.bit [REF_PCM=dec.raw]"; exit 2; }
	$(BUILD)/c2m_test $(INPUT) $(REF_BIT) $(REF_PCM)
	$(BUILD)/c2m_public_only $(INPUT)

clean:
	rm -f $(LIB) $(LIB_OBJS) $(TOOLS) $(TESTS)
