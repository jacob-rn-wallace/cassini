/**
 * @file pins.h
 * @brief GPIO pin assignments for lcd_bringup's standalone Sharp LCD
 *        bring-up project.
 */
#ifndef LCD_BRINGUP_PINS_H
#define LCD_BRINGUP_PINS_H

#include "hardware/spi.h"

// Sharp LS027B7DH01 on the Adafruit breakout board (#4694), wired per
// the root CLAUDE.md's "Hardware" section: only 3 signal pins needed
// (CS/SCK/MOSI) - the breakout handles level-shifting/regulation from
// the Pico's 3.3V logic on-board, and VCOM is toggled entirely in
// software over the SPI command byte (see sharpdisp/src/sharpdisp.c),
// so there's no separate EXTCOMIN pin/oscillator to wire.
//
// This matches sharpdisp_init_freq_hz()'s own built-in default
// (sharpdisp/include/sharpdisp/sharpdisp.h) - named here explicitly,
// and passed explicitly to sharpdisp_init() in main.c, rather than
// relying on that default, so this file stays the single source of
// truth for wiring - same convention soynut's own lcd_bringup/pins.h
// uses for its (unrelated, parallel-interface) LCD.
#define PIN_LCD_CS   17
#define PIN_LCD_SCK  18
#define PIN_LCD_MOSI 19
#define SPI_LCD      spi0

#endif // LCD_BRINGUP_PINS_H
