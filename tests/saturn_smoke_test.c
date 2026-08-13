/**
 * @file saturn_smoke_test.c
 * @brief Native (host) smoke test for the Saturn CPU core wiring.
 *
 * Not part of the firmware - builds and runs on the dev machine with
 * the system compiler, no Pico/ARM toolchain needed. Boots a
 * user-supplied HP48SX/48GX ROM (BYO, see roms/README.md - never
 * shipped in this repo) against the vendored saturn_core, wired
 * through firmware/saturn_compat/'s shims, and runs it for a bounded
 * number of instructions, watching for the ROM ever executing an
 * opcode saturn_core's own OneStep() dispatch doesn't recognize
 * (CPU_E_BAD_OPCODE/CPU_E_BAD_OPCODE2, see cpu.c's several `default:`
 * cases) - the simplest concrete evidence the compat shims are wired
 * correctly and the CPU is actually decoding real Saturn code, not
 * garbage. Mirrors soynut's tests/nut_smoke_test.c in spirit.
 *
 * Build/run: make -C tests run ROM=/path/to/your.rom MODEL=48sx
 * (see tests/Makefile and CLAUDE.md's "Native (host) tests" section).
 */

#include <assert.h>
#include <setjmp.h>
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

/* Power of 10, Rule 2: MAX_INSTR bounds total instructions executed,
 * matching nut_smoke_test.c's own reasoning - a generous ceiling for a
 * bring-up smoke test, not a tuned figure. */
#define MAX_INSTR 2000000

static jmp_buf bad_opcode_unwind;
static bool hit_bad_opcode;
static int bad_opcode_code;

/**
 * @brief Chf handler catching CPU_E_BAD_OPCODE(2), unwinding the
 * instruction loop below instead of letting it print-and-continue.
 *
 * Same pattern saturn_core's own emulator.c uses for
 * CPU_I_EMULATOR_INT (EmulatorLoopHandler) - a real exercise of
 * ChfPushHandler()/ChfSignal()'s unwind path, not a shortcut around it.
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
 * @brief Parse "48sx"/"48gx" into the ui4x_model_t saturn_core's
 * bus.c/romram48.c branch on.
 * @return true on a recognized name, false otherwise.
 */
static bool ParseModel( const char* name, ui4x_model_t* out )
{
    if ( strcmp( name, "48sx" ) == 0 ) {
        *out = MODEL_48SX;
        return true;
    }
    if ( strcmp( name, "48gx" ) == 0 ) {
        *out = MODEL_48GX;
        return true;
    }
    return false;
}

int main( int argc, char* argv[] )
{
    if ( argc != 3 ) {
        fprintf( stderr, "usage: %s <48sx|48gx> <rom-path>\n", argv[ 0 ] );
        return 2;
    }

    ui4x_model_t model;
    if ( !ParseModel( argv[ 1 ], &model ) ) {
        fprintf( stderr, "unknown model '%s' (expected 48sx or 48gx)\n", argv[ 1 ] );
        return 2;
    }
    ui4x_config.model = model;
    ui4x_config.verbose = false;

    saturn_compat_set_rom_path( argv[ 2 ] );

    const int chf_ret = ChfStaticInit( MAIN_CHF_MODULE_ID, argv[ 0 ], CHF_DEFAULT, message_table, message_table_size, 16, 8, EXIT_FAILURE );
    if ( chf_ret != CHF_S_OK ) {
        fprintf( stderr, "ChfStaticInit failed: %d\n", chf_ret );
        return 1;
    }

    EmulatorInit();
    assert( cpu.pc == 0 ); /* CpuReset()'s documented cold-start value */

    /* volatile: modified after setjmp(), read after a possible
     * longjmp() back into this frame from BadOpcodeHandler(). */
    volatile int executed = 0;

    if ( setjmp( bad_opcode_unwind ) == 0 ) {
        ChfPushHandler( CPU_CHF_MODULE_ID, BadOpcodeHandler, &bad_opcode_unwind, ( void* )NULL );
        for ( ; executed < MAX_INSTR; executed++ )
            OneStep();
    }

    printf( "instructions executed: %d\n", executed );
    printf( "final pc: 0x%05X\n", cpu.pc );

    if ( hit_bad_opcode ) {
        printf( "FAIL: hit bad opcode (code=%d) at pc=0x%05X after %d instructions\n", bad_opcode_code, cpu.pc, executed );
        return 1;
    }

    printf( "PASS: no bad opcode in %d instructions\n", executed );
    return 0;
}
