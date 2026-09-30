#include "font.h"
#include "font16.h"
#include "display_direct.h"
#include "colors.h"

#include <zephyr/sys/byteorder.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define INTERLINING   2
#define FONT_MAX_SCALE 6

const Font *font = &font16;

/* ── draw_char ────────────────────────────────────────────────────────
 * Renders one character directly to the display, one row at a time.
 * Uses a small stack buffer (max font width = 11 px → 11 × 2 = 22 B).
 */
void draw_char(int16_t x0, int16_t y0, char c, uint16_t color)
{
    if (!font || c < font->first_char ||
        c >= (char)(font->first_char + font->num_chars)) return;

    uint8_t ci        = (uint8_t)(c - font->first_char);
    const uint8_t *bm = font->char_table[ci];
    uint8_t w         = font->width_table[ci];
    uint8_t h         = font->char_height;
    uint8_t ds        = (w + 7) / 8; /* bytes per row in bitmap */

    uint16_t fg = sys_cpu_to_be16(color);
    uint16_t bg = sys_cpu_to_be16(COLOR_BLACK);

    /* Stack row buffer — widest glyph in font16 is 11 px */
    uint16_t row_buf[12];

    for (uint8_t y = 0; y < h; y++) {
        int screen_y = y0 + y;
        if (screen_y < 0 || screen_y >= SCREEN_HEIGHT) continue;

        for (uint8_t x = 0; x < w; x++) {
            uint8_t byte_idx = y * ds + (x / 8);
            uint8_t bit      = 7 - (x % 8);
            row_buf[x] = ((bm[byte_idx] >> bit) & 1) ? fg : bg;
        }

        int screen_x = x0;
        int draw_w   = w;

        /* Clamp left */
        if (screen_x < 0) {
            draw_w  += screen_x;
            /* shift pointer — row_buf is small so just offset index */
            display_write_row_span(0, screen_y, draw_w,
                                   row_buf + (w - draw_w));
            continue;
        }
        /* Clamp right */
        if (screen_x + draw_w > SCREEN_WIDTH)
            draw_w = SCREEN_WIDTH - screen_x;
        if (draw_w <= 0) continue;

        display_write_row_span(screen_x, screen_y, draw_w, row_buf);
    }
}

/* ── draw_char_scaled ─────────────────────────────────────────────────
 * Same glyph data as draw_char(), but each source pixel is drawn as a
 * scale × scale block - a blocky upscale rather than a second, larger
 * font.
 */
void draw_char_scaled(int16_t x0, int16_t y0, char c, uint16_t color, uint8_t scale)
{
    if (!font || c < font->first_char ||
        c >= (char)(font->first_char + font->num_chars)) return;

    if (scale <= 1) {
        draw_char(x0, y0, c, color);
        return;
    }
    if (scale > FONT_MAX_SCALE) {
        scale = FONT_MAX_SCALE;
    }

    uint8_t ci        = (uint8_t)(c - font->first_char);
    const uint8_t *bm = font->char_table[ci];
    uint8_t w         = font->width_table[ci];
    uint8_t h         = font->char_height;
    uint8_t ds        = (w + 7) / 8; /* bytes per row in bitmap */

    uint16_t fg = sys_cpu_to_be16(color);
    uint16_t bg = sys_cpu_to_be16(COLOR_BLACK);

    /* Widest glyph in font16 is 11 px; built once per source row, then
     * the same scaled row is written out `scale` times for the height.
     */
    uint16_t row_buf[12 * FONT_MAX_SCALE];
    uint16_t scaled_w = (uint16_t)w * scale;

    for (uint8_t y = 0; y < h; y++) {
        for (uint8_t x = 0; x < w; x++) {
            uint8_t byte_idx = y * ds + (x / 8);
            uint8_t bit      = 7 - (x % 8);
            uint16_t px = ((bm[byte_idx] >> bit) & 1) ? fg : bg;

            for (uint8_t s = 0; s < scale; s++) {
                row_buf[x * scale + s] = px;
            }
        }

        int screen_x = x0;
        int draw_w   = scaled_w;
        const uint16_t *src = row_buf;

        /* Clamp left */
        if (screen_x < 0) {
            draw_w += screen_x;
            src    -= screen_x;
            screen_x = 0;
        }
        /* Clamp right */
        if (screen_x + draw_w > SCREEN_WIDTH) {
            draw_w = SCREEN_WIDTH - screen_x;
        }
        if (draw_w <= 0) continue;

        for (uint8_t s = 0; s < scale; s++) {
            int screen_y = y0 + y * scale + s;

            if (screen_y < 0 || screen_y >= SCREEN_HEIGHT) continue;
            display_write_row_span(screen_x, screen_y, draw_w, src);
        }
    }
}

/* ── draw_string ──────────────────────────────────────────────────── */
void draw_string(int16_t x, int16_t y, const char *str, uint16_t color)
{
    if (!font) return;

    int16_t cx = x;
    int16_t cy = y;
    const int16_t max_x = SCREEN_WIDTH - 10;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += font->char_height + INTERLINING;
            str++;
            continue;
        }
        if (*str < font->first_char ||
            *str >= (char)(font->first_char + font->num_chars)) {
            str++;
            continue;
        }

        uint8_t ci = (uint8_t)(*str - font->first_char);
        int16_t cw = font->width_table[ci];

        if (cx + cw > max_x) {
            cx = x;
            cy += font->char_height + INTERLINING;
            if (cy + font->char_height > SCREEN_HEIGHT) break;
        }

        draw_char(cx, cy, *str, color);
        cx += cw + 1;
        str++;
    }
}

/* ── draw_string_scaled ──────────────────────────────────────────────
 * Same layout logic as draw_string(), but every metric (char width,
 * line height, advance) is multiplied by `scale` to match
 * draw_char_scaled()'s blown-up glyphs.
 */
void draw_string_scaled(int16_t x, int16_t y, const char *str, uint16_t color, uint8_t scale)
{
    if (!font) return;

    if (scale <= 1) {
        draw_string(x, y, str, color);
        return;
    }
    if (scale > FONT_MAX_SCALE) {
        scale = FONT_MAX_SCALE;
    }

    int16_t cx = x;
    int16_t cy = y;
    const int16_t max_x = SCREEN_WIDTH - 10;
    const int16_t line_height = (font->char_height + INTERLINING) * scale;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += line_height;
            str++;
            continue;
        }
        if (*str < font->first_char ||
            *str >= (char)(font->first_char + font->num_chars)) {
            str++;
            continue;
        }

        uint8_t ci = (uint8_t)(*str - font->first_char);
        int16_t cw = font->width_table[ci] * scale;

        if (cx + cw > max_x) {
            cx = x;
            cy += line_height;
            if (cy + font->char_height * scale > SCREEN_HEIGHT) break;
        }

        draw_char_scaled(cx, cy, *str, color, scale);
        cx += cw + scale;
        str++;
    }
}

/* ── str_width ────────────────────────────────────────────────────── */
int str_width(const char *str)
{
    if (!font) return 0;
    int w = 0;
    while (*str) {
        if (*str >= font->first_char &&
            *str < (char)(font->first_char + font->num_chars)) {
            uint8_t ci = (uint8_t)(*str - font->first_char);
            w += font->width_table[ci] + 1;
        }
        str++;
    }
    return w;
}
