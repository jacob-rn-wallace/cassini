# Native (host) build for tests/*.c - see CLAUDE.md's "Native (host)
# tests" section for what these prove and how they fit into the project.
#
# Power of 10, Rule 10: our own hand-written sources (this directory's
# *_test.c files, firmware/saturn_compat/*.c) are compiled with -Wall
# -Wextra -Wpedantic -Werror. The vendored saturn_core/src/core/*.c is
# compiled with only the flags it functionally needs - never edited to
# satisfy our own warning level, same principle soynut's tests/Makefile
# applies to emu41gcc. Splitting this per source file (rather than one
# flag set for the whole build) is the actual point: strict warnings
# must never leak onto code we don't own and can't fix.
#
# Usage:
#   make -C tests ROM=/path/to/your.rom MODEL=48sx        # build
#   make -C tests run ROM=/path/to/your.rom MODEL=48sx    # build + run
#   make -C tests clean
#
# ROM must be a real, user-supplied Saturn ROM dump - see
# roms/README.md. Never committed to this repo. MODEL defaults to
# 48sx (CLAUDE.md's confirmed simplest-tied-with-48gx first target).

CC ?= cc
ROOT := ..
BUILD := build
MODEL ?= 48sx

STD := -std=gnu11
STRICT := -Wall -Wextra -Wpedantic -Werror
CORE := $(ROOT)/saturn_core/src/core
LIBCHF := $(ROOT)/saturn_core/src/libChf/src
COMPAT := $(ROOT)/firmware/saturn_compat
INC := -I$(CORE) -I$(ROOT)/saturn_core/src -I$(LIBCHF) -I$(COMPAT)

CORE_SRCS := bus.c chf_messages.c cpu.c disassembler.c disk_io.c emulator.c \
             flash49.c hdw.c keyboard.c monitor.c romram48.c romram49.c serial.c
CORE_OBJS := $(addprefix $(BUILD)/core_,$(CORE_SRCS:.c=.o))

COMPAT_SRCS := chf_compat.c ui4x_compat.c config_compat.c
COMPAT_OBJS := $(addprefix $(BUILD)/,$(COMPAT_SRCS:.c=.o))

.PHONY: all run clean
all: $(BUILD)/saturn_smoke_test

$(BUILD):
	mkdir -p $(BUILD)

# --- Vendored saturn_core/src/core/: not ours, compiled leniently -
# only the flags actually required to build it at all. ---

$(BUILD)/core_%.o: $(CORE)/%.c | $(BUILD)
	$(CC) $(STD) $(INC) -c -o $@ $<

# serial.c calls read()/write()/close() without including <unistd.h>
# itself (its original build must have picked those declarations up
# transitively through some other header - not true on macOS's libc).
# Force-including it is the same "make a modern compiler accept
# unmodified vendored source" trick soynut's tests/Makefile uses for
# nutcpu.c's implicit declarations, applied to a single file rather
# than the whole vendored tree.
$(BUILD)/core_serial.o: $(CORE)/serial.c | $(BUILD)
	$(CC) $(STD) $(INC) -include unistd.h -c -o $@ $<

# --- Our own compat shims: strict warnings apply. ---

$(BUILD)/%.o: $(COMPAT)/%.c | $(BUILD)
	$(CC) $(STD) $(STRICT) $(INC) -c -o $@ $<

# --- Our own test files: strict warnings apply. ---

$(BUILD)/%_test.o: %_test.c | $(BUILD)
	$(CC) $(STD) $(STRICT) $(INC) -c -o $@ $<

# --- Link recipe. ---

$(BUILD)/saturn_smoke_test: $(BUILD)/saturn_smoke_test.o $(CORE_OBJS) $(COMPAT_OBJS)
	$(CC) -o $@ $^

run: all
	@if [ -z "$(ROM)" ]; then \
		echo "usage: make -C tests run ROM=/path/to/your.rom [MODEL=48sx|48gx]"; \
		exit 2; \
	fi
	./$(BUILD)/saturn_smoke_test $(MODEL) $(ROM)

clean:
	rm -rf $(BUILD)
