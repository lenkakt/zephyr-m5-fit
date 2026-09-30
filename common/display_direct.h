#ifndef DISPLAY_DIRECT_H
#define DISPLAY_DIRECT_H

#include <stdint.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

/*
 * No global framebuffer — pixels are written to the display one row at a
 * time through a single 640-byte row buffer.  All drawing functions are
 * therefore "immediate": they write to the hardware during the call.
 *
 * The display must be initialised (display_direct_init) before any draw
 * call is made.
 */

/* Hardware init — call once from main(). */
void display_direct_init(void);

/* Fill a rectangle with a solid colour (RGB565). */
void display_fill_rect(int x, int y, int w, int h, uint16_t color);

/* Clear the whole screen to black. */
void screen_clear(void);

/*
 * Low-level: write one horizontal span into the hardware at position
 * (x, y), 'len' pixels wide.  'pixels' must point to 'len' uint16_t
 * values in RGB565 big-endian byte order.
 * Used by font.c to push individual character rows.
 */
void display_write_row_span(int x, int y, int len, const uint16_t *pixels);

#endif /* DISPLAY_DIRECT_H */
