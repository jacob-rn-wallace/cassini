/**
 * @file chf_compat.c
 * @brief Bare-metal-portable reimplementation of the five libChf
 * functions saturn_core/src/core/ actually calls.
 *
 * saturn_core vendors its own condition-handling library, libChf
 * (saturn_core/src/libChf/), but its real implementation leans on two
 * POSIX facilities that don't port to bare-metal RP2350: chf_msgc.c's
 * <nl_types.h> message-catalog backend, and chf_sig.c's signal-safe
 * sigsetjmp/siglongjmp unwind. Per CLAUDE.md's "The Saturn CPU core"
 * section, the actual API surface every source file under
 * src/core/ depends on
 * (via chf_wrapper.h's SIGNAL/ERROR/FATAL/WARNING/DEBUG macros) is
 * only five functions: ChfStaticInit, ChfGenerate, ChfPushHandler,
 * ChfSignal, ChfExit. This file reimplements just that surface,
 * against libChf's own unmodified Chf.h, without touching libChf
 * itself.
 *
 * Semantics reproduced: a single pending condition is built by
 * ChfGenerate() (chf_wrapper.h's macros always pair exactly one
 * ChfGenerate() call with one immediately-following ChfSignal() call -
 * confirmed by reading the macro expansions, never a multi-condition
 * group), then ChfSignal() walks the handler stack from the top,
 * invoking each handler registered for the signaled module_id until
 * one returns other than CHF_RESIGNAL. CHF_UNWIND/CHF_UNWIND_KEEP
 * performs a longjmp() back to the jmp_buf the handler was pushed
 * with (see saturn_core/src/core/emulator.c's Emulator(), which does
 * exactly `setjmp(unwind_context); ChfPushHandler(..., &unwind_context,
 * ...)`) - a plain (non-signal-safe) jmp_buf is sufficient since
 * there's no real POSIX signal delivery here, only synchronous calls
 * within saturn_core's own control flow. No handler found (or CHF_FATAL
 * with nothing willing to unwind) falls back to reporting the message
 * and exiting.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Chf.h"
#include "options.h"

/** Populated once by ChfStaticInit(); never reallocated afterward. */
static struct {
    int module_id;
    const char* app_name;
    const ChfTable* table;
    size_t table_size;
    int exit_code;
    bool initialized;
} chf_ctx;

/** One handler-stack slot, as registered by ChfPushHandler(). */
typedef struct {
    int module_id;
    ChfHandler handler;
    void* unwind_context; /* &jmp_buf, per ChfPushHandler's caller */
    void* handler_context;
} chf_handler_slot_t;

static chf_handler_slot_t* handler_stack;
static int handler_stack_capacity;
static int handler_stack_top; /* number of slots in use */

/** The one condition currently staged by ChfGenerate(), consumed by
 *  the next ChfSignal(). chf_wrapper.h's macros never stage more than
 *  one at a time (see file header), so a single slot is sufficient. */
static ChfDescriptor pending;
static bool pending_valid;

static const char* FindMessageTemplate( int module_id, int condition_code )
{
    for ( size_t i = 0; i < chf_ctx.table_size; i++ ) {
        if ( chf_ctx.table[ i ].module == module_id && chf_ctx.table[ i ].code == condition_code )
            return chf_ctx.table[ i ].msg_template;
    }
    return "(no message template registered for module %d code %d)";
}

int ChfStaticInit( const int module_id, const char* app_name, const int options, const ChfTable* table, const size_t table_size,
                   const int condition_stack_size, const int handler_stack_size, const int exit_code )
{
    (void)options;          /* CHF_ABORT vs CHF_DEFAULT: not needed - see ChfExit() */
    (void)condition_stack_size; /* single-pending-condition model, see file header */

    if ( chf_ctx.initialized )
        return CHF_F_BAD_STATE;

    handler_stack = calloc( ( size_t )handler_stack_size, sizeof( chf_handler_slot_t ) );
    if ( handler_stack == NULL )
        return CHF_F_MALLOC;

    chf_ctx.module_id = module_id;
    chf_ctx.app_name = app_name;
    chf_ctx.table = table;
    chf_ctx.table_size = table_size;
    chf_ctx.exit_code = exit_code;
    chf_ctx.initialized = true;

    handler_stack_capacity = handler_stack_size;
    handler_stack_top = 0;
    pending_valid = false;

    return CHF_S_OK;
}

void ChfGenerate( const int module_id, const char* file_name, const int line_number, const int condition_code, const ChfSeverity severity,
                  ... )
{
    va_list args;

    pending.module_id = module_id;
    pending.condition_code = condition_code;
    pending.severity = severity;
    pending.line_number = line_number;
    pending.file_name = file_name;
    pending.next = NULL;

    /* Chf.h's own ChfGenerate() declaration - which this function must
     * match exactly, it's the vendored libChf API this shim stands in
     * for - has an enum (ChfSeverity) as the last named parameter
     * before the "...". Enums undergo default argument promotion,
     * which va_start() pedantically disallows; every real compiler
     * handles it correctly regardless (this is an extremely common
     * pattern - e.g. any syslog-style variadic logger), so the
     * diagnostic is suppressed right here rather than weakening
     * -Wpedantic project-wide. See DEVIATIONS.md. */
#  if defined( __clang__ )
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wvarargs"
#  elif defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wvarargs"
#  endif
    va_start( args, severity );
#  if defined( __clang__ )
#    pragma clang diagnostic pop
#  elif defined( __GNUC__ )
#    pragma GCC diagnostic pop
#  endif
    vsnprintf( pending.message, CHF_MAX_MESSAGE_LENGTH, FindMessageTemplate( module_id, condition_code ), args );
    va_end( args );

    pending_valid = true;
}

void ChfPushHandler( const int module_id, ChfHandler new_handler, void* unwind_context, void* handler_context )
{
    if ( handler_stack_top >= handler_stack_capacity ) {
        fprintf( stderr, "%s: FATAL: Chf handler stack full\n", chf_ctx.app_name );
        exit( chf_ctx.exit_code );
    }

    handler_stack[ handler_stack_top ].module_id = module_id;
    handler_stack[ handler_stack_top ].handler = new_handler;
    handler_stack[ handler_stack_top ].unwind_context = unwind_context;
    handler_stack[ handler_stack_top ].handler_context = handler_context;
    handler_stack_top++;
}

static void ReportUnhandled( const ChfDescriptor* d )
{
    static const char* severity_name[] = {"SUCCESS", "INFO", "WARNING", "ERROR", "FATAL"};

    fprintf( stderr, "%s: %s: %s (module %d code %d, %s:%d)\n", chf_ctx.app_name, severity_name[ d->severity ], d->message, d->module_id,
             d->condition_code, d->file_name != NULL ? d->file_name : "?", d->line_number );
}

void ChfSignal( const int module_id )
{
    if ( !pending_valid )
        return;

    ChfDescriptor descriptor = pending;
    pending_valid = false;

    for ( int i = handler_stack_top - 1; i >= 0; i-- ) {
        if ( handler_stack[ i ].module_id != module_id )
            continue;

        const ChfAction action = handler_stack[ i ].handler( &descriptor, CHF_SIGNALING, handler_stack[ i ].handler_context );

        if ( action == CHF_RESIGNAL )
            continue;

        if ( action == CHF_UNWIND || action == CHF_UNWIND_KEEP ) {
            jmp_buf* target = ( jmp_buf* )handler_stack[ i ].unwind_context;
            handler_stack_top = i; /* pop this handler and everything above it */
            longjmp( *target, 1 );
        }

        /* CHF_CONTINUE: condition handled, resume at the call site. */
        return;
    }

    /* No handler claimed it. */
    if ( descriptor.severity == CHF_FATAL ) {
        ReportUnhandled( &descriptor );
        exit( chf_ctx.exit_code );
    }
    if ( descriptor.severity >= CHF_WARNING || ui4x_config.verbose )
        ReportUnhandled( &descriptor );
}

void ChfExit( void )
{
    free( handler_stack );
    handler_stack = NULL;
    handler_stack_capacity = 0;
    handler_stack_top = 0;
    chf_ctx.initialized = false;
}
