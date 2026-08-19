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
 * lcd_bringup/main.c. Still no keyboard (no physical keyboard hardware
 * for this project yet) and still not real-time/wall-clock-throttled
 * (deliberately bounded by instruction count, not by `Emulator()`'s
 * own unbounded loop - Power of 10, Rule 2) - but the loop below now
 * drives the real Saturn T1/T2 hardware timer registers the same way
 * `saturn_core/src/core/emulator.c`'s own `EmulatorLoop()` does,
 * instruction-count-paced instead of wall-clock-paced, since a first
 * bring-up run confirmed (via a real nibble probe of the emulated LCD
 * memory) that the ROM was sitting fully idle with a completely blank
 * LCD buffer - consistent with waiting on a timer interrupt this
 * harness never used to deliver at all. See CLAUDE.md's "Native
 * firmware" section.
 */

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/psram.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Chf.h"
#include "chf_wrapper.h"
#include "cpu.h"
#include "emulator.h"
#include "options.h"
#include "saturn_compat.h"
#include "ui4x/src/api.h"

#include <fonts/liberation_mono_10.h>
#include <sharpdisp/bitmaptext.h>
#include <sharpdisp/sharpdisp.h>

#include "hdw.h"
#include "pins.h"
#include "rom_syscalls.h"
#include "saturn_lcd.h"

/* Power of 10, Rule 2: same generous bring-up ceiling already proven
 * safe against this exact ROM by tests/saturn_smoke_test.c. */
#define MAX_INSTR 2000000

/* Real Saturn/HP48 hardware timer register control bits (hdw.t1_ctrl/
 * hdw.t2_ctrl - hdw.h:52-57) and the real T2-ticks-per-T1-tick ratio
 * (8192 Hz / 16 Hz). Copied verbatim from saturn_core/src/core/
 * emulator.c's own file-local `T1_CTRL_*`/`T2_CTRL_*`/`T1_MULTIPLIER`
 * macros (not exported via any header) - these are real HP48 hardware
 * register bit meanings, not vendored implementation logic, the same
 * "copy the reference behavior, don't touch the vendored file" posture
 * saturn_lcd.c's own decode already uses. INSTR_PER_T2_TICK has no
 * real-hardware equivalent - EmulatorLoop() paces T2 ticks by wall-clock
 * time (T2_INTERVAL, ~122us/tick), which this deliberately
 * non-real-time, instruction-count-bounded harness has no equivalent
 * of; this just needs to tick "often enough" within MAX_INSTR to give
 * the ROM's own timer setup a real chance to fire. */
#define T1_CTRL_INT  0x02
#define T1_CTRL_WAKE 0x04
#define T1_CTRL_SREQ 0x08
#define T2_CTRL_TRUN 0x01
#define T2_CTRL_INT  0x02
#define T2_CTRL_WAKE 0x04
#define T2_CTRL_SREQ 0x08
#define T1_MULTIPLIER ( 8192 / 16 ) /* real T2 ticks per T1 tick = 512 */
#define INSTR_PER_T2_TICK 10

#define PANEL_WIDTH_PX  400
#define PANEL_HEIGHT_PX 240

static uint8_t disp_buffer[ BITMAP_SIZE( PANEL_WIDTH_PX, PANEL_HEIGHT_PX ) ];
static struct SharpDisp sd;

/* Power of 10, Rule 2: fixed upper bounds - a scrolling on-screen log
 * of what main() is doing, so progress is visible without depending
 * on a serial terminal ever successfully connecting (a real, recurring
 * problem on this project's host machine - see CLAUDE.md's "Current
 * blocker" section). Still also printf()'d, for whenever a terminal
 * IS attached. */
#define LOG_MAX_LINES 18
#define LOG_LINE_MAXLEN 64
static char log_lines[ LOG_MAX_LINES ][ LOG_LINE_MAXLEN ];
static int log_count = 0;

/* Persists the FULL log (not just the on-screen tail above) to a
 * reserved region of onboard flash, so it can be read back after the
 * fact with `picotool save -r 0x10FF0000 0x11000000 -t bin out.bin -f`
 * (no BOOTSEL button needed - `-f` forces it) even across a reset or a
 * run that scrolled past too fast to read/photograph - see CLAUDE.md's
 * "Current blocker" section for why this exists (this project's
 * serial terminal has been unreliable enough that "read it back later
 * from flash" is more dependable than "catch it live"), and for the
 * real bugs this already found (the flash write bug below, and the
 * hardware_psram linking trap noted where psram_is_available() is
 * called). Reserves the last 64 KiB of the Pico Plus 2's 16 MiB flash
 * (this board's PICO_FLASH_SIZE_BYTES, per its board header) - far
 * above the ~2.3 MiB the actual program + embedded ROM occupy, so this
 * can't collide with them. Kept as a standing debug facility, not
 * removed once the original blocker was found - proved useful enough
 * to keep around for whatever's next. */
#define FLASH_LOG_SIZE ( 64 * 1024 )
#define FLASH_LOG_OFFSET ( ( 16 * 1024 * 1024 ) - FLASH_LOG_SIZE )
#define FULL_LOG_MAXLEN ( FLASH_LOG_SIZE / 2 ) /* generous headroom under FLASH_LOG_SIZE */
/* Trailing, not-yet-appended-to bytes MUST read as 0xFF (the erased
 * flash state), never 0x00 - main() memset()s this to 0xFF before
 * first use. flash_range_program() can only clear bits (1 -> 0), so
 * the page-rounding in FlashLogFlush() below would otherwise
 * permanently zero out not-yet-written bytes on the first flush of
 * each page, making it impossible for a later, longer flush to ever
 * write real content into those same byte positions again (found the
 * hard way - a first attempt at this left every log line's tail
 * silently corrupted/truncated). */
static char full_log[ FULL_LOG_MAXLEN ];
static size_t full_log_len = 0;

/**
 * @brief Reprogram flash from offset 0 of the reserved log region
 * through the current (page-rounded) full_log_len. Never re-erases -
 * that happens once, in main(), before the first FlashLogAppend() -
 * so this only ever needs to flip already-erased (1) bits to 0,
 * which is a legal, repeatable flash program operation. Bytes already
 * written on a prior call are supplied again unchanged, which is also
 * legal (writing the same value again is a no-op).
 */
static void FlashLogFlush( void )
{
    size_t program_len = ( full_log_len + FLASH_PAGE_SIZE - 1 ) & ~( size_t )( FLASH_PAGE_SIZE - 1 );
    if ( program_len == 0 )
        return;
    if ( program_len > FLASH_LOG_SIZE )
        program_len = FLASH_LOG_SIZE; /* defensive clamp - FULL_LOG_MAXLEN already keeps this unreachable */

    const uint32_t ints = save_and_disable_interrupts();
    flash_range_program( FLASH_LOG_OFFSET, ( const uint8_t* )full_log, program_len );
    restore_interrupts( ints );
}

/**
 * @brief Append one already-formatted line (plus a newline) to the
 * full in-RAM log buffer, then flush it to flash. Silently stops
 * appending (Power of 10, Rule 2: bounded, no dynamic growth) if
 * FULL_LOG_MAXLEN would be exceeded - the on-screen tail in log_lines
 * keeps working regardless.
 */
static void FlashLogAppend( const char* line )
{
    const size_t len = strlen( line );
    if ( full_log_len + len + 1 >= FULL_LOG_MAXLEN )
        return;

    memcpy( full_log + full_log_len, line, len );
    full_log_len += len;
    full_log[ full_log_len++ ] = '\n';

    FlashLogFlush();
}

static jmp_buf bad_opcode_unwind;
static bool hit_bad_opcode;
static int bad_opcode_code;

/**
 * @brief Redraw the entire panel from the current log_lines buffer.
 */
static void LogRedraw( void )
{
    bitmap_clear( &sd.bitmap );

    struct BitmapText text;
    text_init( &text, liberation_mono_10, &sd.bitmap );
    const int16_t line_height = ( int16_t )text_height( &text );

    for ( int i = 0; i < log_count; i++ ) {
        text.x = 2;
        text.y = ( int16_t )( i * line_height );
        text_str( &text, log_lines[ i ] );
    }

    sharpdisp_refresh( &sd );
}

/**
 * @brief printf-style status line: appended to the on-screen scrolling
 * log (oldest line dropped once LOG_MAX_LINES is reached) and also
 * sent over printf() for whenever a serial terminal happens to be
 * attached. Redraws the whole panel every call - see LedHeartbeat()'s
 * comment for why the periodic cadence this is called at (at most
 * every 20000 emulated instructions) keeps that cheap enough.
 */
static void LogLine( const char* fmt, ... )
{
    if ( log_count == LOG_MAX_LINES ) {
        memmove( log_lines[ 0 ], log_lines[ 1 ], ( LOG_MAX_LINES - 1 ) * LOG_LINE_MAXLEN );
        log_count--;
    }

    va_list args;
    va_start( args, fmt );
    vsnprintf( log_lines[ log_count ], LOG_LINE_MAXLEN, fmt, args );
    va_end( args );
    log_count++;

    printf( "%s\n", log_lines[ log_count - 1 ] );
    FlashLogAppend( log_lines[ log_count - 1 ] );

    LogRedraw();
}

/**
 * @brief Repeating-timer callback toggling the onboard LED once a
 * second - a heartbeat independent of main()'s own progress. Runs in
 * an alarm IRQ context, so it keeps ticking even while main() is deep
 * inside a long blocking call (EmulatorInit()'s malloc/ROM-unpack
 * loop, the instruction loop, ...): if this LED ever stops blinking,
 * the board is genuinely wedged (interrupts disabled or a stalled bus
 * access), not just slow - see CLAUDE.md's "Current blocker" section
 * for why that distinction matters here.
 */
static bool LedHeartbeat( struct repeating_timer* t )
{
    (void)t;
    static bool led_on = false;
    led_on = !led_on;
    gpio_put( PICO_DEFAULT_LED_PIN, led_on );
    return true; /* keep repeating */
}

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
    /* Both of these start before stdio_init_all()/the USB-serial wait
     * below, deliberately: they're the physical, no-terminal-required
     * confirmation that the board is alive at all - see CLAUDE.md's
     * "Current blocker" section for why that matters (a prior run
     * looked hung with no way to tell, over a flaky host-side serial
     * connection, whether the board was genuinely wedged or just slow
     * inside a long PSRAM-backed call). */
    gpio_init( PICO_DEFAULT_LED_PIN );
    gpio_set_dir( PICO_DEFAULT_LED_PIN, GPIO_OUT );
    static struct repeating_timer led_timer;
    add_repeating_timer_ms( -1000, LedHeartbeat, NULL, &led_timer );

    /* See full_log's comment: unwritten bytes must read as 0xFF, not
     * BSS's default 0x00. */
    memset( full_log, 0xFF, sizeof( full_log ) );

    /* One-time erase of the reserved flash log region - see
     * FlashLogFlush()'s comment for why this only needs to happen
     * once, before any FlashLogAppend() call. */
    {
        const uint32_t ints = save_and_disable_interrupts();
        flash_range_erase( FLASH_LOG_OFFSET, FLASH_LOG_SIZE );
        restore_interrupts( ints );
    }

    /* clear_byte=0xFF -> BITMAP_BLACK mode, so a plain calculator
     * look (dark pixels on a light background) falls out of
     * saturn_lcd_render()'s filled_rect/text calls automatically -
     * see sharpdisp/src/sharpdisp.c's sharpdisp_init(). */
    sharpdisp_init( &sd, disp_buffer, PANEL_WIDTH_PX, PANEL_HEIGHT_PX, 0xFF, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI, SPI_LCD,
                     10000000 );
    LogLine( "Booting..." );

    stdio_init_all();
    /* Deliberately NOT blocking on stdio_usb_connected() here anymore -
     * that used to gate all progress on a serial terminal actually
     * attaching, which defeated the point of the on-screen log above
     * (see CLAUDE.md's "Current blocker" section: a real serial
     * terminal on the host machine used for this project has been
     * unreliable, hanging on open() more often than not). printf()
     * output is simply dropped if nothing is attached yet - the panel
     * is now the primary, always-available channel; a terminal can
     * still attach at any point and will see everything from then on. */
    LogLine( "starting" );

    /* Keep this call, even though it looks like "just a log line": it's
     * what force-links hardware_psram's runtime_init_setup_psram()
     * into this binary in the first place. Static linking only pulls a
     * .o out of an archive if something references a symbol from it,
     * and this project's link uses --gc-sections - without a real call
     * to some hardware_psram function somewhere, PSRAM would silently
     * go back to being unconfigured, exactly the bug this project spent
     * a full session chasing (see CLAUDE.md's "Current blocker"
     * section: the first attempt at this malloc'd/wrote/read-back
     * "successfully" against completely unconfigured QMI hardware,
     * because nothing forced psram.c's init code into the link). */
    LogLine( "psram: available=%d size=%u", ( int )psram_is_available(), ( unsigned )psram_get_size() );

    ui4x_config.model = MODEL_48SX;
    ui4x_config.verbose = false;

    saturn_compat_set_rom_path( EMBEDDED_ROM_PATH );

    const int chf_ret = ChfStaticInit( MAIN_CHF_MODULE_ID, "cassini", CHF_DEFAULT, message_table, message_table_size, 16, 8, EXIT_FAILURE );
    if ( chf_ret != CHF_S_OK ) {
        LogLine( "ChfStaticInit failed: %d", chf_ret );
        while ( true )
            sleep_ms( 1000 );
    }

    LogLine( "loading ROM..." );
    EmulatorInit();
    assert( cpu.pc == 0 ); /* CpuReset()'s documented cold-start value */
    LogLine( "ROM loaded, running up to %d instr", MAX_INSTR );

    /* volatile: modified after setjmp(), read after a possible
     * longjmp() back into this frame from BadOpcodeHandler(). */
    volatile int executed = 0;
    int t1_subcount = 0; /* T2 ticks accumulated toward the next T1 tick */

    if ( setjmp( bad_opcode_unwind ) == 0 ) {
        ChfPushHandler( CPU_CHF_MODULE_ID, BadOpcodeHandler, &bad_opcode_unwind, ( void* )NULL );
        for ( ; executed < MAX_INSTR; executed++ ) {
            OneStep();

            /* Drive the real Saturn T1/T2 timer registers - see the
             * T1_CTRL_.../T2_CTRL_.../T1_MULTIPLIER block above for
             * the full rationale. T2 only ticks while the ROM has set
             * T2_CTRL_TRUN (real hardware behavior - T1 has no such
             * gate and always ticks). */
            if ( executed % INSTR_PER_T2_TICK == 0 ) {
                if ( hdw.t2_ctrl & T2_CTRL_TRUN ) {
                    hdw.t2_val--;
                    if ( hdw.t2_val == ( int )0xFFFFFFFF ) {
                        hdw.t2_ctrl |= T2_CTRL_SREQ;
                        if ( hdw.t2_ctrl & T2_CTRL_WAKE )
                            CpuWake();
                        if ( hdw.t2_ctrl & T2_CTRL_INT )
                            CpuIntRequest( INT_REQUEST_IRQ );
                    }
                }

                if ( ++t1_subcount >= T1_MULTIPLIER ) {
                    t1_subcount = 0;
                    hdw.t1_val = ( hdw.t1_val - 1 ) & NIBBLE_MASK;
                    if ( hdw.t1_val == 0xF ) {
                        hdw.t1_ctrl |= T1_CTRL_SREQ;
                        if ( hdw.t1_ctrl & T1_CTRL_WAKE )
                            CpuWake();
                        if ( hdw.t1_ctrl & T1_CTRL_INT )
                            CpuIntRequest( INT_REQUEST_IRQ );
                    }
                }
            }

            if ( executed % 20000 == 0 )
                LogLine( "...%d instr, pc=0x%05X", executed, cpu.pc );
        }
    }

    LogLine( "executed %d instr, pc=0x%05X%s", executed, cpu.pc, hit_bad_opcode ? " (bad opcode)" : "" );

    bitmap_clear( &sd.bitmap ); /* erase the log before the real render */
    saturn_lcd_render( &sd );
    sharpdisp_refresh( &sd );
    printf( "cassini: display refreshed\n" ); /* serial-only - the log view is now replaced by the real render */

    while ( true ) {
        sleep_ms( 1000 );
    }
}
