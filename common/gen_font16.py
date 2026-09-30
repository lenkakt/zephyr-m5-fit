#!/usr/bin/env python3
"""
Generates font16.c for this project - NOT copied from any third-party
bitmap font. Rasterizes ASCII 32-127 from the system's DejaVu Sans Mono
font (Bitstream Vera License - explicitly permits exactly this kind of
reuse, see /usr/share/fonts/truetype/dejavu/LICENSE or
https://dejavu-fonts.github.io/License.html) into the widtbl_f16/
chrtbl_f16 bitmap table format font.c/font.h expect: one byte per
ceil(width/8) pixels per row, MSB first, char_height rows per glyph.

Run this to regenerate font16.c (e.g. after changing FONT_SIZE below):

    python3 common/gen_font16.py > common/font16.c

Requires Pillow (`pip install Pillow`) and the DejaVu fonts package
(usually preinstalled on Linux; `apt install fonts-dejavu-core`
otherwise).
"""

import datetime
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONT_SIZE = 13
CHAR_HEIGHT = 16
FIRST_CHAR = 32
NUM_CHARS = 96  # 32..127 inclusive
CANVAS_WIDTH = 24  # generous working width, trimmed per glyph below


def render_glyph(font, ascent, descent, ch):
    img = Image.new("L", (CANVAS_WIDTH, CHAR_HEIGHT), 0)
    d = ImageDraw.Draw(img)
    d.text((1, CHAR_HEIGHT - ascent - descent), ch, font=font, fill=255)
    return img


def trim_width(img, ch):
    px = img.load()
    if ch == " ":
        return 5
    last_lit = -1
    for x in range(CANVAS_WIDTH):
        for y in range(CHAR_HEIGHT):
            if px[x, y] > 128:
                last_lit = x
                break
    return max(last_lit + 1, 1) if last_lit >= 0 else 5


def pack_rows(img, width):
    px = img.load()
    ds = (width + 7) // 8
    out = []
    for y in range(CHAR_HEIGHT):
        row_bytes = [0] * ds
        for x in range(width):
            if px[x, y] > 128:
                row_bytes[x // 8] |= 1 << (7 - (x % 8))
        out.extend(row_bytes)
    return out


def main():
    font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
    ascent, descent = font.getmetrics()

    widths = []
    glyph_bytes = []
    for code in range(FIRST_CHAR, FIRST_CHAR + NUM_CHARS):
        ch = chr(code)
        img = render_glyph(font, ascent, descent, ch)
        w = trim_width(img, ch)
        widths.append(w)
        glyph_bytes.append(pack_rows(img, w))

    print("/*")
    print(f" * Generated {datetime.date.today().isoformat()} by gen_font16.py from")
    print(f" * DejaVu Sans Mono ({FONT_PATH}, size {FONT_SIZE}px) - the Bitstream")
    print(" * Vera License explicitly permits this (see")
    print(" * https://dejavu-fonts.github.io/License.html). Not copied from any")
    print(" * TFT/embedded-display font library. Run gen_font16.py to regenerate")
    print(" * (e.g. after changing FONT_SIZE there).")
    print(" *")
    print(" * Same table layout font.c/font.h expect: one byte per ceil(width/8)")
    print(" * pixels per bitmap row, MSB first, char_height rows per glyph.")
    print(" */")
    print()
    print('#include "font.h"')
    print('#include "font16.h"')
    print()
    print(f"const uint8_t widtbl_f16[{NUM_CHARS}] = {{")
    for i in range(0, NUM_CHARS, 16):
        chunk = widths[i:i + 16]
        print("\t" + ", ".join(str(w) for w in chunk) + ",")
    print("};")
    print()

    for idx, code in enumerate(range(FIRST_CHAR, FIRST_CHAR + NUM_CHARS)):
        w = widths[idx]
        ds = (w + 7) // 8
        data = glyph_bytes[idx]
        print(f"static const uint8_t chr{code:02X}_f16[] = {{")
        for r in range(CHAR_HEIGHT):
            row = data[r * ds:(r + 1) * ds]
            hex_bytes = ", ".join(f"0x{b:02X}" for b in row)
            print(f"\t{hex_bytes},")
        print("};")
    print()

    print(f"const uint8_t* const chrtbl_f16[{NUM_CHARS}] = {{")
    names = [f"chr{code:02X}_f16" for code in range(FIRST_CHAR, FIRST_CHAR + NUM_CHARS)]
    for i in range(0, NUM_CHARS, 8):
        print("\t" + ", ".join(names[i:i + 8]) + ",")
    print("};")
    print()

    print("const Font font16 = {")
    print(f"\t.char_height = {CHAR_HEIGHT},")
    print(f"\t.baseline = {ascent},")
    print(f"\t.first_char = {FIRST_CHAR},")
    print("\t.data_size = 2,")
    print(f"\t.num_chars = {NUM_CHARS},")
    print("\t.width_table = widtbl_f16,")
    print("\t.char_table = chrtbl_f16")
    print("};")


if __name__ == "__main__":
    main()
