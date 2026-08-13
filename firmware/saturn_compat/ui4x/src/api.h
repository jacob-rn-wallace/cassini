/**
 * @file api.h
 * @brief Stand-in for saturnng's real src/ui4x/src/api.h.
 *
 * saturn_core/src/options.h does `#include "ui4x/src/api.h"`, but
 * saturnng's src/ui4x/ is a separate git submodule Cassini deliberately
 * never initializes (see CLAUDE.md's "The Saturn CPU core" section) -
 * ui4x is the GTK4/SDL/ncurses/Lua UI layer, and src/core/ never needs
 * it for anything beyond the narrow surface reproduced here.
 *
 * This header is picked up via the compiler's quote-include search
 * order: `#include "ui4x/src/api.h"` first looks relative to the
 * including file's own directory (saturn_core/src/, where no such path
 * exists since the submodule isn't checked out), then falls through to
 * the -I search path, which is where this file lives. saturn_core
 * itself is never modified to make this work.
 *
 * Surface reproduced here is exactly what a direct grep of every
 * source file under saturn_core/src/core/ for `ui4x_` confirmed as
 * actually referenced from core code: the `ui4x_config.model`/`.verbose`
 * fields, the model enum they're compared against, and the two
 * filename-resolution callbacks. Real ui4x defines many more symbols
 * (ui4x_init, ui4x_start, LCD_WIDTH/LCD_HEIGHT, ...) - none of those
 * are needed here because src/core/ never calls them; they're used
 * only by saturnng's own main.c/emulator_api.c, which Cassini doesn't
 * build.
 */
#ifndef UI4X_API_H
#define UI4X_API_H 1

#include <stdbool.h>

/** Saturn-family models saturn_core's core distinguishes at runtime
 *  (src/core/bus.c's bus_hp48/bus_hp49 selection,
 *  src/core/romram48.c's RomInit48() ROM-size branch). Numeric values
 *  are Cassini's own choice - saturn_core/src/core/ only ever compares
 *  ui4x_config.model against these names, never against a fixed
 *  integer, so no specific encoding is required to match upstream. */
typedef enum {
    MODEL_48SX,
    MODEL_48GX,
    MODEL_40G,
    MODEL_49G,
    MODEL_50G
} ui4x_model_t;

/** Minimal stand-in for ui4x's real (much larger) config struct -
 *  only the two fields src/core/ actually reads. */
typedef struct {
    ui4x_model_t model;
    bool verbose;
} ui4x_config_t;

extern ui4x_config_t ui4x_config;

/** Resolve a logical saturn_core state-file name (e.g. "rom", "cpu",
 *  "mod" - see saturn_core/src/options.h's *_FILE_NAME constants) to a
 *  path openable with fopen(). See saturn_compat_ui4x.c for the real
 *  policy: "rom" resolves to the BYO ROM path Cassini's test harness
 *  supplies, everything else resolves to a path that deliberately
 *  doesn't exist, so saturn_core's own ReadStructFromFile() fallback
 *  (cold CpuReset()/bus_reset()) kicks in instead of trying to load
 *  nonexistent saved state. */
extern char* ui4x_make_filename_absolute_for_loading(const char* name);
extern char* ui4x_make_filename_absolute_for_saving(const char* name);

#endif /* !UI4X_API_H */
