/**
 * @file pins.h
 * @brief GPIO pin assignments for the real Cassini firmware.
 */
#ifndef FIRMWARE_PINS_H
#define FIRMWARE_PINS_H

#include "hardware/spi.h"

// Same physical wiring as lcd_bringup/pins.h (this is the same Sharp
// LS027B7DH01 + Adafruit breakout #4694, just driven by the real
// firmware instead of the standalone bring-up harness) - see that
// file and the root CLAUDE.md's "Hardware" section for the full
// rationale. Kept as its own file per this project's existing
// lcd_bringup/firmware separation convention, even though the values
// currently match exactly.
#define PIN_LCD_CS   17
#define PIN_LCD_SCK  18
#define PIN_LCD_MOSI 19
#define SPI_LCD      spi0

#endif // FIRMWARE_PINS_H
