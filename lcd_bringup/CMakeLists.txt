cmake_minimum_required(VERSION 3.13)

# Standalone from ../firmware on purpose - no Saturn core, no ROM,
# nothing else. Sole purpose: get anything showing on the physical
# LS027B7DH01 over its 3-wire SPI link. See main.c.

if (NOT DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../pico-sdk)
endif ()

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(lcd_bringup C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Pulls in the vendored sharpdisp library's own INTERFACE library
# targets (SHARPDISP, BITMAPSHAPES, BITMAPTEXT, per-font targets, ...) -
# see sharpdisp/src/CMakeLists.txt and sharpdisp/fonts/CMakeLists.txt.
# Never edited directly, same "vendored, treat as a black box" posture
# this project applies to saturn_core/ - see sharpdisp/README.md.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sharpdisp/src sharpdisp_src)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sharpdisp/fonts sharpdisp_fonts)

add_executable(lcd_bringup
        main.c
)

# Power of 10, Rule 10: strict warnings for our own one file only.
# Scoped with set_property/COMPILE_OPTIONS rather than
# target_compile_options - the sharpdisp INTERFACE libraries above
# attach their own (vendored, not ours to fix) sources directly to
# this target under the hood, and target_compile_options would apply
# to those too. Same pattern (and reason) as soynut's lcd_bringup/
# CMakeLists.txt.
#
# -UNDEBUG is for Rule 5: the Pico SDK's default build type is Release,
# which sets -DNDEBUG - and <assert.h> compiles every assert() to
# nothing under NDEBUG, silently defeating this file's assertions.
set_property(SOURCE main.c APPEND PROPERTY COMPILE_OPTIONS
        -Wall -Wextra -Wpedantic -Werror -UNDEBUG
)

target_link_libraries(lcd_bringup
        pico_stdlib
        SHARPDISP
        BITMAPSHAPES
        BITMAPTEXT
        LIBERATION_SANS_36
)

pico_enable_stdio_usb(lcd_bringup 1)
pico_enable_stdio_uart(lcd_bringup 0)

pico_add_extra_outputs(lcd_bringup)
