cmake_minimum_required(VERSION 3.13)

# Boots a real, user-embedded HP48SX ROM against the vendored
# saturn_core, wired through firmware/saturn_compat/'s shims and this
# directory's own Pico-only glue (rom_syscalls.c, saturn_lcd.c), and
# renders its emulated LCD output to the physical Sharp display via
# the vendored sharpdisp/ library. See CLAUDE.md's "Native firmware"
# section for what this proves and its deliberately limited scope
# (static bring-up, no keyboard/interactivity yet).

if (NOT DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../pico-sdk)
endif ()

# Was pico2 through the "Out of mem" discovery (see CLAUDE.md's
# "Current blocker" section) - updated to the Pico Plus 2 now that
# PSRAM hardware has been ordered specifically to resolve that
# blocker. Still overridable the same way as before
# (-DPICO_BOARD=... or the PICO_BOARD env var), e.g. to go back to
# pico2 for any reason.
set(PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(cassini_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Vendored sharpdisp library - never edited, see sharpdisp/README.md.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sharpdisp/src sharpdisp_src)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sharpdisp/fonts sharpdisp_fonts)

set(SATURN_CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../saturn_core/src/core)
set(SATURN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../saturn_core/src)
set(LIBCHF_DIR ${CMAKE_CURRENT_LIST_DIR}/../saturn_core/src/libChf/src)
set(SATURN_COMPAT_DIR ${CMAKE_CURRENT_LIST_DIR}/saturn_compat)
set(ROMS_DIR ${CMAKE_CURRENT_LIST_DIR}/../roms)

# First (and so far only) edit to the vendored saturn_core - see
# CLAUDE.md's "Current blocker" section for the full rationale.
# N_PORT_2_BANK_48 (saturn_core/src/core/bus.h) is hardcoded to 32
# (an 8 MiB Port_2/NCE3 card-slot buffer) for every HP48-series model,
# even though HP48SX - this project's actual target - doesn't have
# that card slot in real hardware at all. That single constant pushes
# struct BusStatus_48's one malloc() call to ~9.5 MiB, too big even
# for the Pico Plus 2's 8 MiB PSRAM heap (see CASSINI_PSRAM_HEAP
# below). Applied as a maintained patch (../saturn_core.patch, tracked
# in this repo) rather than hand-editing the submodule's checked-out
# files, so `git submodule update --init` still reproduces the exact
# pinned upstream commit - the patch is the only thing this repo
# actually commits. Checked with --reverse --check first so this is
# idempotent across repeated cmake reconfigures.
execute_process(
        COMMAND git apply --reverse --check ${CMAKE_CURRENT_LIST_DIR}/../saturn_core.patch
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../saturn_core
        RESULT_VARIABLE SATURN_CORE_PATCH_ALREADY_APPLIED
        OUTPUT_QUIET ERROR_QUIET
)
if (NOT SATURN_CORE_PATCH_ALREADY_APPLIED EQUAL 0)
    execute_process(
            COMMAND git apply ${CMAKE_CURRENT_LIST_DIR}/../saturn_core.patch
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../saturn_core
            RESULT_VARIABLE SATURN_CORE_PATCH_RESULT
    )
    if (NOT SATURN_CORE_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply ../saturn_core.patch to saturn_core/ - see CLAUDE.md's \"Current blocker\" section")
    endif ()
endif ()

if (NOT EXISTS ${ROMS_DIR}/rom_images.c)
    message(FATAL_ERROR
        "roms/rom_images.c not found - generate it first:\n"
        "  python3 roms/rom_to_c.py roms/<your-rom>.rom roms/rom_images.c\n"
        "(gitignored, BYO ROM - see roms/README.md)")
endif ()

add_executable(cassini
        main.c
        rom_syscalls.c
        saturn_lcd.c
        ${SATURN_COMPAT_DIR}/chf_compat.c
        ${SATURN_COMPAT_DIR}/ui4x_compat.c
        ${SATURN_COMPAT_DIR}/config_compat.c
        ${ROMS_DIR}/rom_images.c
        ${SATURN_CORE_DIR}/bus.c
        ${SATURN_CORE_DIR}/chf_messages.c
        ${SATURN_CORE_DIR}/cpu.c
        ${SATURN_CORE_DIR}/disassembler.c
        ${SATURN_CORE_DIR}/disk_io.c
        ${SATURN_CORE_DIR}/emulator.c
        ${SATURN_CORE_DIR}/flash49.c
        ${SATURN_CORE_DIR}/hdw.c
        ${SATURN_CORE_DIR}/keyboard.c
        ${SATURN_CORE_DIR}/monitor.c
        ${SATURN_CORE_DIR}/romram48.c
        ${SATURN_CORE_DIR}/romram49.c
        ${SATURN_CORE_DIR}/serial.c
)

target_include_directories(cassini PRIVATE
        ${SATURN_CORE_DIR}
        ${SATURN_SRC_DIR}
        ${LIBCHF_DIR}
        ${SATURN_COMPAT_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/stubs
)

# serial.c calls read()/write()/close() without including <unistd.h>
# itself (its original build must have picked those declarations up
# transitively through some other header - not true against this
# toolchain). Force-including it is the same "make unmodified vendored
# source build under a different compiler" trick tests/Makefile
# already uses for this exact file on the host build.
set_property(SOURCE ${SATURN_CORE_DIR}/serial.c APPEND PROPERTY COMPILE_OPTIONS
        -include unistd.h
)

# Power of 10, Rule 10: strict warnings only for this project's own
# hand-written files - never the vendored saturn_core/src/core/*.c
# (compiled leniently, same split tests/Makefile uses) or the
# generated roms/rom_images.c (data only). Scoped with
# set_property/COMPILE_OPTIONS rather than target_compile_options -
# the sharpdisp INTERFACE libraries attach their own vendored sources
# to this target too, same reasoning lcd_bringup/CMakeLists.txt
# documents.
#
# -UNDEBUG is for Rule 5: the Pico SDK's default build type is Release,
# which sets -DNDEBUG - and <assert.h> compiles every assert() to
# nothing under NDEBUG, silently defeating this project's assertions.
set_property(SOURCE
        main.c rom_syscalls.c saturn_lcd.c
        ${SATURN_COMPAT_DIR}/chf_compat.c
        ${SATURN_COMPAT_DIR}/ui4x_compat.c
        ${SATURN_COMPAT_DIR}/config_compat.c
        APPEND PROPERTY COMPILE_OPTIONS
        -Wall -Wextra -Wpedantic -Werror -UNDEBUG
)

target_link_libraries(cassini
        pico_stdlib
        hardware_psram
        SHARPDISP
        BITMAPSHAPES
        BITMAPTEXT
        LIBERATION_MONO_10
)

# CONFIRMED WORKING on real hardware, 2026-08-19 - see CLAUDE.md's
# "Native firmware" -> "Resolved: ROM memory footprint" section for
# the full history. ON by default: this is now the required path for
# this project's actual target board (Pico Plus 2) - without it, the
# heap stays in on-chip SRAM and saturn_core's malloc() fails exactly
# as it did on the plain Pico 2. Still a named option (not hardcoded
# in) so it can be turned off if ever building for a board without
# PSRAM.
#
# First attempt here was a plain --defsym=HEAP_LOC/HEAP_LIMIT override,
# on the theory that the Pico SDK's section_heap.incl's HEAP_LOC/
# HEAP_LIMIT hooks were enough by themselves. Real hardware link
# testing (the actual Pico Plus 2, first time in hand) proved that
# wrong: those hooks only move the heap's *address*, not which memory
# *region* the linker checks it against - section_heap.incl hardcodes
# `.heap ... > RAM` regardless, so that attempt failed at link time
# with "address 0x11000800 of cassini.elf section `.heap' is not
# within region `RAM'".
#
# The real fix uses the Pico SDK's own supported mechanism for exactly
# this - pico_add_linker_script_override_path() - to substitute
# linker_overrides/section_heap.incl (this project's own file, not a
# vendored/edited SDK file) for the SDK's version, targeting the PSRAM
# region SDK 2.x already defines (PSRAM, ORIGIN = 0x11000000, LENGTH =
# PICO_PSRAM_SIZE_BYTES - pico_standard_link/pico_psram_region.template.ld)
# instead of RAM. See that file for the full mechanism.
#
# This alone was necessary but not sufficient - saturn_core's ~9.5 MiB
# struct BusStatus_48 request still exceeded 8 MiB of PSRAM until
# saturn_core.patch (repo root, applied automatically below) right-sized
# N_PORT_2_BANK_48.
#
# Real caveat, not yet benchmarked either way: bus_fetch_nibble() runs
# on every emulated instruction, and ROM/RAM now live in PSRAM instead
# of on-chip SRAM. QMI PSRAM access latency could meaningfully slow
# execution - the 2,000,000-instruction bring-up run completes in well
# under a few seconds regardless, so this clearly isn't prohibitive,
# but no on-chip-SRAM-vs-PSRAM timing comparison has been done.
option(CASSINI_PSRAM_HEAP "Relocate the newlib malloc() heap into PSRAM" ON)
if (CASSINI_PSRAM_HEAP)
    pico_add_linker_script_override_path(cassini
            ${CMAKE_CURRENT_LIST_DIR}/linker_overrides
            FILES section_heap.incl
    )
endif ()

pico_enable_stdio_usb(cassini 1)
pico_enable_stdio_uart(cassini 0)

pico_add_extra_outputs(cassini)
