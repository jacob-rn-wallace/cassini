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

set(PICO_BOARD pico2 CACHE STRING "Board type")

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
        SHARPDISP
        BITMAPSHAPES
        BITMAPTEXT
        LIBERATION_MONO_10
)

pico_enable_stdio_usb(cassini 1)
pico_enable_stdio_uart(cassini 0)

pico_add_extra_outputs(cassini)
