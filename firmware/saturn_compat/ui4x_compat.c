/**
 * @file ui4x_compat.c
 * @brief Storage for ui4x_config and the two ui4x_make_filename_*
 * callbacks saturn_core/src/core/ calls directly (see this repo's
 * CLAUDE.md, "The Saturn CPU core" - confirmed by grep, not assumed).
 *
 * Policy: every logical name (ROM_FILE_NAME/CPU_FILE_NAME/... from
 * saturn_core/src/options.h) resolves to a path that deliberately
 * doesn't exist, *except* ROM_FILE_NAME, which resolves to whatever
 * saturn_compat_set_rom_path() was last given. That makes
 * saturn_core's own ReadStructFromFile() fallback logic (cold
 * CpuReset()/bus_reset() on load failure - see emulator.c's
 * EmulatorInit()) do the right thing for a from-scratch smoke test:
 * only the ROM loads for real, nothing else pretends a prior session
 * exists.
 */

#include <string.h>

#include "options.h"
#include "saturn_compat.h"
#include "ui4x/src/api.h"

ui4x_config_t ui4x_config;

static const char* rom_path = "/nonexistent-cassini-rom-path";

void saturn_compat_set_rom_path( const char* path ) { rom_path = path; }

static char* Resolve( const char* name )
{
    if ( strcmp( name, ROM_FILE_NAME ) == 0 )
        return ( char* )rom_path;

    /* CPU_FILE_NAME/BUS_FILE_NAME/HDW_FILE_NAME/RAM_FILE_NAME/
     * PORT1_FILE_NAME/PORT2_FILE_NAME - none of them exist here on
     * purpose, so their ReadStructFromFile() callers fall back to a
     * cold init instead of loading stale/absent state. */
    return "/nonexistent-cassini-state-path";
}

char* ui4x_make_filename_absolute_for_loading( const char* name ) { return Resolve( name ); }

char* ui4x_make_filename_absolute_for_saving( const char* name ) { return Resolve( name ); }
