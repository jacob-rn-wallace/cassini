/**
 * @file main.c
 * @brief Standalone Sharp LS027B7DH01 bring-up program - see the block
 *        comment below for its purpose.
 */

#include "pico/stdlib.h"
#include <assert.h>

#include <fonts/liberation_sans_36.h>
#include <sharpdisp/bitmapshapes.h>
#include <sharpdisp/bitmaptext.h>
#include <sharpdisp/sharpdisp.h>

#include "pins.h"

// Sole purpose of this program: get anything to show up on the
// physical LS027B7DH01, with zero dependency on saturn_core, a ROM, or
// any other Cassini firmware code - all irrelevant to this problem. If
// this can't get a single pixel lit, the problem is in the
// display/wiring/toolchain layer, not anything above it.
//
// Draws a static test pattern (project name, centered, plus a border)
// and stops - deliberately the same shape as the vendored sharpdisp
// library's own hello_world example, which was already built, flashed,
// and visually confirmed working on this exact display/breakout/board
// combination before being vendored into this repo (see
// sharpdisp/README.md). The only real differences here are Cassini's
// own build wiring (this CMakeLists.txt, not sharpdisp's) and naming
// the CS/SCK/MOSI pins explicitly via pins.h rather than relying on
// sharpdisp_init_default()'s internal default (which happens to match
// anyway - see pins.h).

#define LCD_WIDTH_PX  400
#define LCD_HEIGHT_PX 240

static uint8_t disp_buffer[ BITMAP_SIZE( LCD_WIDTH_PX, LCD_HEIGHT_PX ) ];

/**
 * @brief Entry point: draw a centered label and border, push it to the
 *        display once, then idle forever.
 * @return Never returns.
 */
int main( void )
{
    sleep_ms( 100 ); /* let the breakout's onboard regulator stabilize */

    struct SharpDisp sd;
    sharpdisp_init( &sd, disp_buffer, LCD_WIDTH_PX, LCD_HEIGHT_PX, 0x00, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI, SPI_LCD,
                     10000000 );

    struct BitmapText text;
    text_init( &text, liberation_sans_36, &sd.bitmap );

    const char* label = "Cassini";
    text.x = ( int16_t )( ( LCD_WIDTH_PX - text_str_width( &text, label ) ) / 2 );
    text.y = ( int16_t )( ( LCD_HEIGHT_PX - text_height( &text ) ) / 2 );
    text_str( &text, label );

    const uint16_t border = 15;
    assert( LCD_WIDTH_PX > border * 2 && LCD_HEIGHT_PX > border * 2 );
    bitmap_rect( &sd.bitmap, border, border, LCD_WIDTH_PX - border * 2, LCD_HEIGHT_PX - border * 2 );

    sharpdisp_refresh( &sd );

    while ( true ) {
        sleep_ms( 1000 );
    }
}
