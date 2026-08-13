/**
 * @file config_compat.c
 * @brief Storage for saturn_core/src/options.h's `extern config_t
 * config;` global.
 *
 * The real definition lives in saturnng's own src/options.c, which
 * Cassini doesn't vendor (it's argv/Lua-config parsing for the real
 * GTK app, not something src/core/ needs). config.reset is set true
 * so EmulatorInit() always performs a cold CpuReset()/bus_reset()
 * rather than trying to resume a session that was never saved (see
 * ui4x_compat.c's filename-resolution policy, which makes every
 * saved-state load fail on purpose).
 */

#include "options.h"

config_t config = {
    .throttle = false,
    .reset = true,
    .monitor = false,
    .speed = 0,
    .print_config_and_exit = false,
    .debug_level = 0,
};
