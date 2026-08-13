/**
 * @file saturn_lcd.h
 * @brief Renders the vendored Saturn core's emulated LCD + annunciator
 * state onto a sharpdisp bitmap.
 */
#ifndef SATURN_LCD_H
#define SATURN_LCD_H 1

#include <sharpdisp/sharpdisp.h>

/**
 * @brief Read the current HP48SX/GX emulated LCD (131x64) and
 * annunciator state out of the vendored core's global `hdw`/bus state
 * and draw it into sd->bitmap: 3x nearest-neighbor scaled (393x192),
 * centered in the panel, with a row of annunciator text labels in the
 * top margin band. Does not call sharpdisp_refresh() - the caller
 * decides when to push the buffer to hardware.
 *
 * @param sd An already-initialized SharpDisp (sharpdisp_init() already
 *           called) sized to the physical panel (400x240).
 */
void saturn_lcd_render( struct SharpDisp* sd );

#endif /* !SATURN_LCD_H */
