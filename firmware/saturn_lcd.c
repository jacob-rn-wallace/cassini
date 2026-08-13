/**
 * @file saturn_lcd.c
 * @brief HP48SX/GX LCD + annunciator decode, scaling, and blit into a
 * sharpdisp bitmap.
 *
 * Reimplemented directly against the vendored core's `hdw` global and
 * `bus_fetch_nibble()` - deliberately NOT reusing
 * saturn_core/src/emulator_api.c, since that file depends on the
 * un-vendored `ui4x` submodule (LCD_WIDTH, ui4x_get_lcd_height(), ...
 * - see CLAUDE.md's "The Saturn CPU core" section). It IS used as the
 * reference for the exact decode algorithm, copied verbatim rather
 * than re-derived: row-then-menu-row split, and bit 0 (LSB) of each
 * nibble = leftmost of its 4 pixels, bit 3 (MSB) = rightmost
 * (emulator_api.c:202-236). Geometry (131x64) is hardcoded here since
 * it's not present anywhere in vendored source for HP48SX/GX
 * specifically - confirmed HP48 standard, not a guess.
 */

#include <stdbool.h>

#include "bus.h"
#include "hdw.h"
#include "types.h"

#include <fonts/liberation_mono_10.h>
#include <sharpdisp/bitmapshapes.h>
#include <sharpdisp/bitmaptext.h>

#include "saturn_lcd.h"

#define SATURN_LCD_WIDTH  131
#define SATURN_LCD_HEIGHT 64
#define SATURN_LCD_NIBBLES_PER_ROW 34
#define SATURN_LCD_SCALE 3
#define SATURN_LCD_SCALED_WIDTH  ( SATURN_LCD_WIDTH * SATURN_LCD_SCALE )   /* 393 */
#define SATURN_LCD_SCALED_HEIGHT ( SATURN_LCD_HEIGHT * SATURN_LCD_SCALE )  /* 192 */

#define PANEL_WIDTH_PX  400
#define PANEL_HEIGHT_PX 240

/* 400-393=7, split 3 left / 4 right (unused, just documented here);
 * 240-192=48, split evenly 24 top / 24 bottom - the user's explicit
 * direction: centered, with the top band used for annunciator labels
 * rather than left blank. */
#define MARGIN_LEFT ( ( PANEL_WIDTH_PX - SATURN_LCD_SCALED_WIDTH ) / 2 )   /* 3 */
#define MARGIN_TOP  ( ( PANEL_HEIGHT_PX - SATURN_LCD_SCALED_HEIGHT ) / 2 ) /* 24 */

/* Same bits/order as emulator_api.c's annunciators_bits_t. */
typedef struct {
    int bit;
    const char* label;
} saturn_ann_t;

static const saturn_ann_t kAnnunciators[] = {
    {0x81, "L"  },
    {0x82, "R"  },
    {0x84, "A"  },
    {0x88, "BAT"},
    {0x90, "BSY"},
    {0xa0, "IO" },
};
#define N_ANNUNCIATORS ( ( int )( sizeof( kAnnunciators ) / sizeof( kAnnunciators[ 0 ] ) ) )

/**
 * @brief Draw one source nibble's 4 pixels as 3x3 scaled blocks.
 * @param bitmap Destination bitmap (already BITMAP_BLACK-mode, so a
 *               filled_rect here draws a dark/"lit" pixel).
 * @param v The nibble read from emulated LCD memory.
 * @param col_x Source column of the nibble's first (leftmost) pixel.
 * @param row_y Source row.
 */
static void PlotNibble( struct Bitmap* bitmap, Nibble v, int col_x, int row_y )
{
    for ( int nx = 0; nx < 4; nx++ ) {
        if ( ( v & ( 1 << nx ) ) == 0 )
            continue; /* pixel off - leave background alone */

        const int src_x = col_x + nx;
        if ( src_x >= SATURN_LCD_WIDTH )
            continue; /* raw nibble data extends past the 131 visible columns */

        const int16_t dst_x = ( int16_t )( MARGIN_LEFT + src_x * SATURN_LCD_SCALE );
        const int16_t dst_y = ( int16_t )( MARGIN_TOP + row_y * SATURN_LCD_SCALE );
        bitmap_filled_rect( bitmap, dst_x, dst_y, SATURN_LCD_SCALE, SATURN_LCD_SCALE );
    }
}

/**
 * @brief Scan one full display row (34 nibbles) starting at row_addr.
 */
static void RenderRow( struct Bitmap* bitmap, Address row_addr, int row_y )
{
    Address addr = row_addr;
    for ( int x = 0; x < SATURN_LCD_NIBBLES_PER_ROW; x++ ) {
        const Nibble v = bus_fetch_nibble( addr );
        addr++;
        PlotNibble( bitmap, v, x * 4, row_y );
    }
}

/**
 * @brief Draw a row of short text labels for any set annunciator bit,
 * evenly spaced across the scaled display's width, in the top margin.
 */
static void RenderAnnunciators( struct Bitmap* bitmap )
{
    struct BitmapText text;
    text_init( &text, liberation_mono_10, bitmap );

    const int16_t label_y = ( int16_t )( ( MARGIN_TOP - text_height( &text ) ) / 2 );
    const int slot_width = SATURN_LCD_SCALED_WIDTH / N_ANNUNCIATORS;

    for ( int i = 0; i < N_ANNUNCIATORS; i++ ) {
        if ( ( kAnnunciators[ i ].bit & hdw.lcd_ann ) != kAnnunciators[ i ].bit )
            continue;

        text.x = ( int16_t )( MARGIN_LEFT + i * slot_width );
        text.y = label_y;
        text_str( &text, kAnnunciators[ i ].label );
    }
}

void saturn_lcd_render( struct SharpDisp* sd )
{
    struct Bitmap* bitmap = &sd->bitmap;

    /* Main display region: hdw.lcd_base_addr, advancing
     * SATURN_LCD_NIBBLES_PER_ROW + hdw.lcd_line_offset nibbles between
     * rows - matches emulator_api.c's get_lcd_buffer() exactly (its
     * addr++ happens NIBBLES_PER_ROW times in the inner loop, then
     * += hdw.lcd_line_offset once per row). Bounded by
     * SATURN_LCD_HEIGHT as a defensive clamp (Power of 10, Rule 2) in
     * case hdw.lcd_vlc is ever misconfigured/out of range. */
    Address addr = hdw.lcd_base_addr;
    int y = 0;
    for ( ; y <= hdw.lcd_vlc && y < SATURN_LCD_HEIGHT; y++ ) {
        RenderRow( bitmap, addr, y );
        addr += SATURN_LCD_NIBBLES_PER_ROW;
        addr += hdw.lcd_line_offset;
    }

    /* Menu-row region: hdw.lcd_menu_addr, no extra offset. */
    addr = hdw.lcd_menu_addr;
    for ( ; y < SATURN_LCD_HEIGHT; y++ ) {
        RenderRow( bitmap, addr, y );
        addr += SATURN_LCD_NIBBLES_PER_ROW;
    }

    RenderAnnunciators( bitmap );
}
