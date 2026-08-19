/**
 * @file main.c
 * @brief Real Cassini firmware entry point - interactive.
 *
 * Boots a compiled-in HP48SX ROM (see roms/rom_to_c.py,
 * firmware/rom_syscalls.c) against the vendored saturn_core, then runs
 * indefinitely: driving the real Saturn T1/T2 hardware timer registers
 * the same way `saturn_core/src/core/emulator.c`'s own `EmulatorLoop()`
 * does (instruction-count-paced rather than wall-clock-paced - see
 * INSTR_PER_T2_TICK's comment), polling incoming USB-serial bytes for
 * key press/release commands (see PollKeyboardInput()) and injecting
 * them via the vendored core's own `KeybPress()`/`KeybRelease()`, and
 * redrawing the physical Sharp display on a wall-clock cadence. Power
 * of 10, Rule 2's "bounded loops" doesn't fit an interactive device
 * that's meant to run until powered off - documented as a deliberate
 * exception in DEVIATIONS.md, the same posture soynut's own main loop
 * takes. See CLAUDE.md's "Native firmware" section for the full
 * history, including the earlier bounded/static-bring-up phase this
 * grew out of.
 */

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/psram.h"
#include "hardware/structs/scb.h"
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
#include "keyboard.h"
#include "pins.h"
#include "rom_syscalls.h"
#include "saturn_lcd.h"

/* How often (in emulated instructions) the main loop checks for
 * incoming key commands / considers a wall-clock redraw - see
 * PollKeyboardInput() and the redraw block in main(). Not a real
 * hardware quantity, just "often enough" relative to human reaction
 * time given this core's real execution speed on this board. */
#define INSTR_PER_POLL 200
#define REDRAW_INTERVAL_MS 100

/* See firmware/CMakeLists.txt's CASSINI_VERBOSE_BOOT_LOG option -
 * deactivated (not removed) by default: the per-checkpoint LogLine()
 * call below is real, useful debugging infrastructure, but each call
 * does a real flash write and a full display refresh, and there are
 * up to 100 of them in one MAX_INSTR run. */
#ifndef CASSINI_VERBOSE_BOOT_LOG
#define CASSINI_VERBOSE_BOOT_LOG 0
#endif

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

/* TEMPORARY diagnostic - investigating the real-hardware-only YES/NO
 * keyboard bug (see CLAUDE.md's "Native firmware" -> "Interactive
 * keyboard bridge" section). Earlier attempt at this wrote each trace
 * line straight to flash (like the boot log does) - real flash erase/
 * program calls take real wall-clock time, which turned out to shift
 * the very timing this bug is sensitive to, making it disappear
 * whenever the tracing was detailed enough to be useful. This version
 * traces into a plain heap buffer instead (this project's heap is
 * PSRAM-backed - see CASSINI_PSRAM_HEAP - so this costs nothing but a
 * fast RAM write, no erase, no program-time delay) and only touches
 * flash once, reactively, via FlushTraceToFlash() below, well after
 * the timing-sensitive window has already passed. */
#define PSRAM_TRACE_SIZE ( 512 * 1024 )
static char* psram_trace = NULL;
static size_t psram_trace_pos = 0;

static void PsramTraceAppend( const char* line )
{
    if ( psram_trace == NULL )
        return;

    const size_t len = strlen( line );
    if ( psram_trace_pos + len + 1 >= PSRAM_TRACE_SIZE )
        psram_trace_pos = 0; /* wrap - Power of 10, Rule 2: bounded, circular, never grows */

    memcpy( psram_trace + psram_trace_pos, line, len );
    psram_trace_pos += len;
    psram_trace[ psram_trace_pos++ ] = '\n';

    printf( "%s\n", line ); /* still cheap - only a flash-write would be the real cost here */
}

/* TEMPORARY diagnostic - the one place this trace actually touches
 * flash: a single erase + program of the last (page-rounded) portion
 * of psram_trace that fits FLASH_LOG_SIZE, reusing the same reserved
 * region the boot log uses (this overwrites the boot log - acceptable
 * here, the keypress trace is what matters once this fires). Called
 * once, after the observation window has fully elapsed (see
 * trace_remaining's own comment) - not interleaved with the trace
 * itself, so it can't perturb the timing being investigated. */
static void FlushTraceToFlash( void )
{
    if ( psram_trace == NULL || psram_trace_pos == 0 )
        return;

    size_t dump_len = psram_trace_pos;
    if ( dump_len > FLASH_LOG_SIZE )
        dump_len = FLASH_LOG_SIZE;
    const size_t dump_start = psram_trace_pos - dump_len;

    size_t program_len = ( dump_len + FLASH_PAGE_SIZE - 1 ) & ~( size_t )( FLASH_PAGE_SIZE - 1 );
    if ( program_len > FLASH_LOG_SIZE )
        program_len = FLASH_LOG_SIZE;

    static char staging[ FLASH_LOG_SIZE ];
    memset( staging, 0xFF, sizeof( staging ) );
    memcpy( staging, psram_trace + dump_start, dump_len );

    const uint32_t ints = save_and_disable_interrupts();
    flash_range_erase( FLASH_LOG_OFFSET, FLASH_LOG_SIZE );
    flash_range_program( FLASH_LOG_OFFSET, ( const uint8_t* )staging, program_len );
    restore_interrupts( ints );

    printf( "cassini: trace flushed to flash (%u bytes)\n", ( unsigned )dump_len );
}

/* TEMPORARY diagnostic - dumps enough real Saturn CPU state (not just
 * PC) to compare a real-hardware trace against an equivalent host
 * trace nibble-for-nibble, to find the exact first point of divergence
 * rather than continuing to guess at causes. */
static void DumpCpuState( const char* label, long step )
{
    char line[ 160 ];
    snprintf( line, sizeof( line ),
              "%s s=%ld pc=%05X p=%X hst=%X cy=%d ie=%d is=%d ip=%d rp=%d "
              "rs=%05X,%05X,%05X,%05X,%05X,%05X,%05X,%05X d=%05X,%05X",
              label, step, ( unsigned )cpu.pc, ( unsigned )cpu.p, ( unsigned )cpu.hst, ( int )cpu.carry,
              ( int )cpu.int_enable, ( int )cpu.int_service, ( int )cpu.int_pending, cpu.rstk_ptr,
              ( unsigned )cpu.rstk[ 0 ], ( unsigned )cpu.rstk[ 1 ], ( unsigned )cpu.rstk[ 2 ], ( unsigned )cpu.rstk[ 3 ],
              ( unsigned )cpu.rstk[ 4 ], ( unsigned )cpu.rstk[ 5 ], ( unsigned )cpu.rstk[ 6 ], ( unsigned )cpu.rstk[ 7 ],
              ( unsigned )cpu.d[ 0 ], ( unsigned )cpu.d[ 1 ] );
    PsramTraceAppend( line );
}

/* TEMPORARY diagnostic - PollKeyboardInput() sets this to start a
 * bounded CPU-state trace right after processing a key command; the
 * main loop below counts it down, one DumpCpuState() call every
 * TRACE_STEP_INTERVAL instructions until it reaches 0. */
static long trace_remaining = 0;
#define TRACE_STEP_INTERVAL 20
#define TRACE_LENGTH_INSTR  3000

/* TEMPORARY diagnostic - the next concrete step in the YES-path
 * bad-opcode investigation (see CLAUDE.md's "YES-path bad-opcode
 * investigation" section): the true-lockup failure mode observed
 * there never let even the independent LedHeartbeat() watchdog IRQ
 * run, which on ARMv8-M strongly suggests an unhandled fault has
 * raised execution priority high enough to mask ordinary interrupts.
 * A fault handler itself runs at a priority ordinary interrupts can't
 * preempt, so - unlike the watchdog - it can still capture state even
 * in that case. This is the working theory from that investigation,
 * not yet confirmed; this handler exists to confirm or rule it out.
 *
 * HardFaultHandlerC() never returns - once a real hard fault happens,
 * continuing to run the faulted CPU state (or app tries to resume
 * OneStep()) risks corrupting whatever it's about to write out, so
 * this deliberately dead-ends into a fast LED blink loop (distinct
 * from LedHeartbeat()'s normal 1 Hz rhythm) after flushing everything
 * it can to flash - a permanent halt, the same posture the
 * BadOpcodeHandler()/longjmp path below already takes for a
 * recoverable-but-still-fatal condition.
 */
/* __attribute__((used)): only ever referenced from isr_hardfault()'s
 * inline asm below (`b HardFaultHandlerC`), which the compiler can't
 * see as a real call site - without this, --gc-sections plus this
 * project's -Werror would drop/reject it as apparently dead code. */
static void __attribute__( ( noreturn, used ) ) HardFaultHandlerC( uint32_t* stack_frame )
{
    const uint32_t r0 = stack_frame[ 0 ];
    const uint32_t r1 = stack_frame[ 1 ];
    const uint32_t r2 = stack_frame[ 2 ];
    const uint32_t r3 = stack_frame[ 3 ];
    const uint32_t r12 = stack_frame[ 4 ];
    const uint32_t lr = stack_frame[ 5 ];
    const uint32_t pc = stack_frame[ 6 ];
    const uint32_t psr = stack_frame[ 7 ];
    const uint32_t cfsr = scb_hw->cfsr;
    const uint32_t hfsr = scb_hw->hfsr;
    const uint32_t mmfar = scb_hw->mmfar;
    const uint32_t bfar = scb_hw->bfar;

    char line[ 200 ];
    snprintf( line, sizeof( line ),
              "HARDFAULT pc=%08lX lr=%08lX psr=%08lX r0=%08lX r1=%08lX r2=%08lX r3=%08lX r12=%08lX "
              "cfsr=%08lX hfsr=%08lX mmfar=%08lX bfar=%08lX",
              ( unsigned long )pc, ( unsigned long )lr, ( unsigned long )psr, ( unsigned long )r0, ( unsigned long )r1,
              ( unsigned long )r2, ( unsigned long )r3, ( unsigned long )r12, ( unsigned long )cfsr, ( unsigned long )hfsr,
              ( unsigned long )mmfar, ( unsigned long )bfar );
    PsramTraceAppend( line );
    DumpCpuState( "FAULT-saturn", 0 );
    FlushTraceToFlash();

    for ( ;; ) {
        gpio_put( PICO_DEFAULT_LED_PIN, true );
        for ( volatile uint32_t i = 0; i < 500000; i++ ) { }
        gpio_put( PICO_DEFAULT_LED_PIN, false );
        for ( volatile uint32_t i = 0; i < 500000; i++ ) { }
    }
}

/* TEMPORARY diagnostic - overrides crt0.S's weak isr_hardfault stub
 * (pico_crt0/crt0.S: `.weak isr_hardfault` / `decl_isr_bkpt
 * isr_hardfault`) directly, rather than going through
 * exception_set_exclusive_handler() (hardware_exception): that API
 * explicitly asserts if used to override a link-time strong symbol,
 * and can't hand the handler the exception stack frame anyway - naked
 * + a strong isr_hardfault definition is the standard, minimal way to
 * get that on Cortex-M. EXC_RETURN (in LR at fault entry) bit 2 tells
 * MSP vs. PSP; the 8-word hardware-pushed frame (r0-r3, r12, lr, pc,
 * psr) lives at whichever one was active - this project never uses
 * the FPU, so there's no extra FP state to skip. Tail-branches to
 * HardFaultHandlerC (a normal, non-naked C function, [noreturn], so
 * it never needs to return through this asm) with that frame pointer
 * in r0.
 */
void __attribute__( ( naked ) ) isr_hardfault( void )
{
    __asm volatile( "movs r0, #4        \n"
                     "mov  r1, lr        \n"
                     "tst  r0, r1        \n"
                     "beq  1f            \n"
                     "mrs  r0, psp       \n"
                     "b    2f            \n"
                     "1:                 \n"
                     "mrs  r0, msp       \n"
                     "2:                 \n"
                     "b    HardFaultHandlerC \n" );
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

/* TEMPORARY diagnostic - incremented once per main-loop iteration (see
 * main()'s while(true) below); LedHeartbeat() watches this to detect a
 * true hang (as opposed to a bad-opcode cascade that keeps the loop
 * itself running - see CLAUDE.md's "Interactive keyboard bridge"
 * section for why that distinction turned out to matter: a true hang
 * never reaches FlushTraceToFlash()'s normal call site, since that
 * only fires once the main loop's own trace window naturally elapses).
 * volatile: written by main(), read by the timer ISR. */
static volatile long main_loop_progress = 0;

/**
 * @brief Repeating-timer callback toggling the onboard LED once a
 * second - a heartbeat independent of main()'s own progress. Runs in
 * an alarm IRQ context, so it keeps ticking even while main() is deep
 * inside a long blocking call (EmulatorInit()'s malloc/ROM-unpack
 * loop, the instruction loop, ...): if this LED ever stops blinking,
 * the board is genuinely wedged (interrupts disabled or a stalled bus
 * access), not just slow - see CLAUDE.md's "Current blocker" section
 * for why that distinction matters here.
 *
 * TEMPORARY diagnostic addition: also watches main_loop_progress for a
 * true hang and force-flushes whatever's in the PSRAM trace buffer to
 * flash from right here, in IRQ context - the one case
 * FlushTraceToFlash()'s normal call site (inside the main loop itself)
 * can never reach, since a true hang means that loop never gets back
 * around to it. Flash operations are safe to call from this context
 * the same way they're safe anywhere else on this single-core
 * target - flash_range_erase()/flash_range_program() already disable
 * interrupts for their own duration regardless of caller.
 */
static bool LedHeartbeat( struct repeating_timer* t )
{
    (void)t;
    static bool led_on = false;
    led_on = !led_on;
    gpio_put( PICO_DEFAULT_LED_PIN, led_on );

    static long last_seen_progress = -1;
    static int stall_ticks = 0;
    static bool stall_flushed = false;
    if ( main_loop_progress == last_seen_progress ) {
        stall_ticks++;
        if ( stall_ticks >= 2 && !stall_flushed ) {
            stall_flushed = true;
            FlushTraceToFlash();
        }
    } else {
        last_seen_progress = main_loop_progress;
        stall_ticks = 0;
        stall_flushed = false;
    }

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

/**
 * @brief Wire protocol for tools/hp48_keyboard_gui.py: exactly 3 raw
 * bytes per message, no framing byte or newline needed - 'P' or 'R',
 * then 2 uppercase-hex-digit ASCII chars encoding the keycode
 * saturn_core's own KeybPress()/KeybRelease() expect (see keyboard.h
 * and emulator_api.c's keyboard48[] table, the reference this
 * project's copy of the keymap - in the GUI, not duplicated here - was
 * taken from, same "copy the reference values, don't include the
 * file" posture saturn_lcd.c's LCD decode already established). Every
 * real row/col keycode fits in one byte (max 0xBF); the one exception,
 * the ON key's real code 0x8000, doesn't - the wire byte 0xFF is
 * reserved to mean ON instead, translated back to 0x8000 below.
 *
 * A GUI mouse-down sends 'P' immediately; mouse-up sends 'R'
 * immediately - no threshold/hold-detection protocol is needed here,
 * unlike soynut's HP-41 key bridge: KeybPress()/KeybRelease() persist
 * real state in the emulator's own keyboard matrix between calls, so
 * the ROM's own keyboard scan sees a key as held for exactly as long
 * as it takes the GUI to send the release, with no "sustain" plumbing
 * required on this side.
 *
 * Power of 10, Rule 2/3: fixed 3-byte message, bounded number of bytes
 * read per call (at most INSTR_PER_POLL-cadence calls' worth), and any
 * unrecognized byte (wrong position, bad hex digit, mid-message 'P'/
 * 'R') resyncs cleanly by discarding the in-progress message rather
 * than misinterpreting partial input - same recovery posture soynut's
 * own hp41_key_bridge.c established for its escape-sequence protocol.
 */
static void PollKeyboardInput( void )
{
    static char pending_cmd = 0; /* 'P', 'R', or 0 (no message in progress) */
    static char pending_hex[ 2 ];
    static int pending_hex_count = 0;

    for ( int i = 0; i < 32; i++ ) { /* bounded work per call */
        const int c = getchar_timeout_us( 0 );
        if ( c == PICO_ERROR_TIMEOUT )
            break;

        if ( c == 'P' || c == 'R' ) {
            pending_cmd = ( char )c;
            pending_hex_count = 0;
            continue;
        }

        if ( pending_cmd == 0 )
            continue; /* stray byte outside any message - ignore */

        const bool is_hex_digit = ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' );
        if ( !is_hex_digit ) {
            pending_cmd = 0; /* malformed - resync */
            continue;
        }

        pending_hex[ pending_hex_count++ ] = ( char )c;
        if ( pending_hex_count < 2 )
            continue;

        const int nibble0 = ( pending_hex[ 0 ] <= '9' ) ? pending_hex[ 0 ] - '0' : pending_hex[ 0 ] - 'A' + 10;
        const int nibble1 = ( pending_hex[ 1 ] <= '9' ) ? pending_hex[ 1 ] - '0' : pending_hex[ 1 ] - 'A' + 10;
        const int wire_byte = ( nibble0 << 4 ) | nibble1;
        const int keycode = ( wire_byte == 0xFF ) ? 0x8000 : wire_byte;

        if ( pending_cmd == 'P' )
            psram_trace_pos = 0; /* TEMPORARY diagnostic - fresh trace per press, see PsramTraceAppend()'s comment */
        DumpCpuState( pending_cmd == 'P' ? "before-P" : "before-R", 0 ); /* TEMPORARY diagnostic */

        if ( pending_cmd == 'P' )
            KeybPress( keycode );
        else
            KeybRelease( keycode );
        printf( "cassini: key %c 0x%04X\n", pending_cmd, keycode ); /* serial-only, see LogLine()'s own precedent */

        DumpCpuState( pending_cmd == 'P' ? "after-P" : "after-R", 0 ); /* TEMPORARY diagnostic */
        trace_remaining = TRACE_LENGTH_INSTR;                          /* TEMPORARY diagnostic */

        pending_cmd = 0;
        pending_hex_count = 0;
    }
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

    psram_trace = malloc( PSRAM_TRACE_SIZE ); /* TEMPORARY diagnostic - see PsramTraceAppend()'s comment */

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
    LogLine( "ROM loaded, entering interactive mode" );

    bitmap_clear( &sd.bitmap ); /* erase the log before the first real render */
    saturn_lcd_render( &sd );
    sharpdisp_refresh( &sd );
    printf( "cassini: display refreshed\n" ); /* serial-only - the log view is now replaced by the real render */

    long executed = 0;
    int t1_subcount = 0; /* T2 ticks accumulated toward the next T1 tick */
    absolute_time_t next_redraw = make_timeout_time_ms( REDRAW_INTERVAL_MS );

    if ( setjmp( bad_opcode_unwind ) == 0 ) {
        ChfPushHandler( CPU_CHF_MODULE_ID, BadOpcodeHandler, &bad_opcode_unwind, ( void* )NULL );

        /* Power of 10, Rule 2 deliberate exception - see DEVIATIONS.md.
         * A real HP48SX runs until powered off; there is no meaningful
         * instruction bound to apply once real keyboard interaction is
         * in play (the earlier bounded static-bring-up harness this
         * grew out of used MAX_INSTR precisely because it never
         * accepted input and had to stop somewhere). */
        while ( true ) {
            OneStep();
            executed++;
            main_loop_progress++; /* TEMPORARY diagnostic - see its own comment */

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

#if CASSINI_VERBOSE_BOOT_LOG
            if ( executed % 20000 == 0 )
                printf( "cassini: ...%ld instr, pc=0x%05X\n", executed, cpu.pc );
#endif

            if ( executed % INSTR_PER_POLL == 0 ) {
                PollKeyboardInput();

                if ( time_reached( next_redraw ) ) {
                    bitmap_clear( &sd.bitmap );
                    saturn_lcd_render( &sd );
                    sharpdisp_refresh( &sd );
                    next_redraw = make_timeout_time_ms( REDRAW_INTERVAL_MS );
                }
            }

            /* TEMPORARY diagnostic - see trace_remaining's comment. */
            if ( trace_remaining > 0 && executed % TRACE_STEP_INTERVAL == 0 ) {
                DumpCpuState( "trc", TRACE_LENGTH_INSTR - trace_remaining );
                trace_remaining -= TRACE_STEP_INTERVAL;
                if ( trace_remaining <= 0 ) {
                    trace_remaining = 0;
                    FlushTraceToFlash(); /* one-time, after the timing-sensitive window has passed */
                }
            }
        }
    }

    /* Only reached via longjmp from BadOpcodeHandler() - a genuine
     * failure, unlike normal operation above, so this one spot still
     * uses LogLine() to put the error on screen (nothing more will be
     * rendered after this point). */
    LogLine( "hit bad opcode 0x%X at pc=0x%05X, halted", bad_opcode_code, cpu.pc );

    while ( true ) {
        sleep_ms( 1000 );
    }
}
