#ifndef ZEPHYR_M5_FIT_COLORS_H
#define ZEPHYR_M5_FIT_COLORS_H

/*
 * RGB565 values for the 16 named colors from the original HTML 4.01 /
 * CSS Level 1 color keyword set - a well-known, citable reference
 * rather than an arbitrary pick of 16 colors. Converted with the
 * standard truncating 5-6-5 bit split (R>>3, G>>2, B>>3): e.g.
 * COLOR_RED is 0xF800 because 0xFF (red channel of #FF0000) truncates
 * to 0x1F at 5 bits, not because of any rounding choice.
 *
 * Several of these already matched literal values used in this repo
 * before this header existed (COLOR_LIME/COLOR_BLUE/COLOR_WHITE/
 * COLOR_YELLOW/COLOR_AQUA), which is what confirmed this was the right
 * reference set rather than inventing one.
 *
 * Not for the LED strip examples (03-led-strip, 04-led-shell) - those
 * drive WS2812/SK6812 pixels directly as 8-bit-per-channel RGB(W), a
 * different representation from this panel's 16-bit RGB565.
 */

#define COLOR_BLACK   0x0000 /* #000000 */
#define COLOR_MAROON  0x8000 /* #800000 */
#define COLOR_GREEN   0x0400 /* #008000 - HTML's darker "green" */
#define COLOR_OLIVE   0x8400 /* #808000 */
#define COLOR_NAVY    0x0010 /* #000080 */
#define COLOR_PURPLE  0x8010 /* #800080 */
#define COLOR_TEAL    0x0410 /* #008080 */
#define COLOR_SILVER  0xC618 /* #C0C0C0 */
#define COLOR_GRAY    0x8410 /* #808080 */
#define COLOR_RED     0xF800 /* #FF0000 */
#define COLOR_LIME    0x07E0 /* #00FF00 - the bright, fully-saturated green */
#define COLOR_YELLOW  0xFFE0 /* #FFFF00 */
#define COLOR_BLUE    0x001F /* #0000FF */
#define COLOR_FUCHSIA 0xF81F /* #FF00FF */
#define COLOR_AQUA    0x07FF /* #00FFFF */
#define COLOR_WHITE   0xFFFF /* #FFFFFF */

/* Aliases for names used elsewhere for the same colors above. */
#define COLOR_MAGENTA COLOR_FUCHSIA
#define COLOR_CYAN    COLOR_AQUA

#endif /* ZEPHYR_M5_FIT_COLORS_H */
