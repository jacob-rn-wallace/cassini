/**
 * @file main.c
 * @brief Real Cassini firmware entry point - static bring-up milestone.
 *
 * Boots a compiled-in HP48SX ROM (see roms/rom_to_c.py,
 * firmware/rom_syscalls.c) against the vendored saturn_core, runs it
 * for a bounded instruction count (same safety pattern already proven
 * by tests/saturn_smoke_test.c), then renders whatever's on the
 * emulated LCD once to the physical Sharp display and idles -
 * deliberately the same "draw once, then idle" shape as
 * lcd_bringup/main.c. No keyboard, no real-time timer-driven loop yet
 * - there's no physical keyboard hardware for this project yet, so
 * real-time interactivity is its own later phase. See CLAUDE.md's
 * "Native firmware" section.
 */

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "Chf.h"
#include "chf_wrapper.h"
#include "cpu.h"
#include "emulator.h"
#include "options.h"
#include "saturn_compat.h"
#include "ui4x/src/api.h"

#include <sharpdisp/sharpdisp.h>

#include "pins.h"
#include "rom_syscalls.h"
#include "saturn_lcd.h"

/* Power of 10, Rule 2: same generous bring-up ceiling already proven
 * safe against this exact ROM by tests/saturn_smoke_test.c. */
#define MAX_INSTR 2000000

#define PANEL_WIDTH_PX  400
#define PANEL_HEIGHT_PX 240

static uint8_t disp_buffer[ BITMAP_SIZE( PANEL_WIDTH_PX, PANEL_HEIGHT_PX ) ];

static jmp_buf bad_opcode_unwind;
static bool hit_bad_opcode;
static int bad_opcode_code;

/**
 * @brief Chf handler catching CPU_E_BAD_OPCODE(2) - same pattern as
 * tests/saturn_smoke_test.c's BadOpcodeHandler(), unwinding the
 * instruction loop below instead of letting it print-and-continue.
 */
static ChfAction BadOpcodeHandler( const ChfDescriptor* descriptor, const ChfState state, void* handler_context )
{
    (void)handler_context;

    if ( state != CHF_SIGNALING || descriptor->module_id != CPU_CHF_MODULE_ID )
        return CHF_RESIGNAL;

    if ( descriptor->condition_code == CPU_E_BAD_OPCODE || descriptor->condition_code == CPU_E_BAD_OPCODE2 ) {
        hit_bad_opcode = true;
        bad_opcode_code = descriptor->condition_code;
        return CHF_UNWIND;
    }

    return CHF_RESIGNAL;
}

int main( void )
{
    stdio_init_all();
    /* pico_stdio_usb silently drops writes made before a real terminal
     * has connected (no fixed delay can guarantee that in general) -
     * block here instead of guessing, so no boot output is ever lost
     * regardless of how long the user takes to attach a serial
     * terminal. */
    while ( !stdio_usb_connected() )
        sleep_ms( 100 );
    sleep_ms( 200 ); /* let the host's terminal finish attaching */

    printf( "cassini: starting\n" );

    ui4x_config.model = MODEL_48SX;
    ui4x_config.verbose = false;

    saturn_compat_set_rom_path( EMBEDDED_ROM_PATH );

    const int chf_ret = ChfStaticInit( MAIN_CHF_MODULE_ID, "cassini", CHF_DEFAULT, message_table, message_table_size, 16, 8, EXIT_FAILURE );
    if ( chf_ret != CHF_S_OK ) {
        printf( "cassini: ChfStaticInit failed: %d\n", chf_ret );
        while ( true )
            sleep_ms( 1000 );
    }

    EmulatorInit();
    assert( cpu.pc == 0 ); /* CpuReset()'s documented cold-start value */
    printf( "cassini: ROM loaded, running up to %d instructions\n", MAX_INSTR );

    /* volatile: modified after setjmp(), read after a possible
     * longjmp() back into this frame from BadOpcodeHandler(). */
    volatile int executed = 0;

    if ( setjmp( bad_opcode_unwind ) == 0 ) {
        ChfPushHandler( CPU_CHF_MODULE_ID, BadOpcodeHandler, &bad_opcode_unwind, ( void* )NULL );
        for ( ; executed < MAX_INSTR; executed++ ) {
            OneStep();
            if ( executed % 20000 == 0 )
                printf( "cassini: ...%d instructions, pc=0x%05X\n", executed, cpu.pc );
        }
    }

    printf( "cassini: executed %d instructions, final pc=0x%05X%s\n", executed, cpu.pc,
            hit_bad_opcode ? " (hit bad opcode)" : "" );

    struct SharpDisp sd;
    /* clear_byte=0xFF -> BITMAP_BLACK mode, so a plain calculator
     * look (dark pixels on a light background) falls out of
     * saturn_lcd_render()'s filled_rect/text calls automatically -
     * see sharpdisp/src/sharpdisp.c's sharpdisp_init(). */
    sharpdisp_init( &sd, disp_buffer, PANEL_WIDTH_PX, PANEL_HEIGHT_PX, 0xFF, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI, SPI_LCD,
                     10000000 );

    saturn_lcd_render( &sd );
    sharpdisp_refresh( &sd );
    printf( "cassini: display refreshed\n" );

    while ( true ) {
        sleep_ms( 1000 );
    }
}
