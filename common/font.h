#ifndef FONT_H
#define FONT_H

#include <stdint.h>

typedef struct {
    uint8_t char_height;
    uint8_t baseline;
    uint8_t data_size;
    uint8_t first_char;
    uint8_t num_chars;
    const uint8_t *width_table;
    const uint8_t * const *char_table;
} Font;

extern const Font *font;

void draw_char(int16_t x0, int16_t y0, char c, uint16_t color);
void draw_string(int16_t x, int16_t y, const char *str, uint16_t color);
int  str_width(const char *str);

/*
 * Blocky upscale of the same glyph data - each source pixel becomes a
 * scale x scale block. Not a separate, larger font; see font.c.
 */
void draw_char_scaled(int16_t x0, int16_t y0, char c, uint16_t color, uint8_t scale);
void draw_string_scaled(int16_t x, int16_t y, const char *str, uint16_t color, uint8_t scale);

#endif /* FONT_H */
