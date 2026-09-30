# 06-text

Renders real text on the M5Stack Fire's 320×240 color screen without
LVGL — a hand-rolled bitmap-font renderer, cycling through four screens
every 3 seconds: a welcome message, the alphabet/digits, a note on why
Zephyr's own text subsystem doesn't apply here, and a "BIG TEXT" screen
demonstrating scaled-up glyphs.

Everything this example depends on lives in [`../common`](../common),
not in its own `src/` — `src/main.c` is the only file here.

## Why not Zephyr's own CFB (Character Framebuffer) subsystem?

Zephyr ships a text subsystem for displays, CFB (subsys/fb/cfb.c) — worth checking before hand-rolling anything. But this board has a color display, and CFB only ever supports 1-bit-per-pixel monochrome — it can't drive color output at all.

`draw_char()`/`draw_string()` (in `common/font.c`) are what a color-aware
equivalent of CFB looks like: no framebuffer of the whole screen at all —
each glyph row is rendered straight into a small RGB565 buffer and pushed
with `display_write()` via `common/display_direct.c`.

## Why a separate `display_direct.c` from `font.c`

`common/font.c` doesn't know about PWM, `display_write()`, or byte order —
it only calls the abstract `display_write_row_span()`/`screen_clear()`.
`common/display_direct.c` owns everything board-specific: backlight PWM
(same fix as `05-display`), display init, and the actual `display_write()`
call. That split keeps the font renderer hardware-agnostic and reusable.

## The font

`common/font16.c` is a simple bitmap font generated for this project by
[`common/gen_font16.py`](../common/gen_font16.py) — see that script for
how to regenerate it.

## Bigger text without a second font

`draw_char_scaled()`/`draw_string_scaled()` (also in `common/font.c`)
draw the same `font16` glyph data as `NxN` pixel blocks per source pixel
— blockier, not crisper, but no second font file needed. See the "BIG
TEXT" screen (`draw_string_scaled` at `scale = 3`) in `src/main.c`.

## Colors

Uses [`common/colors.h`](../common/colors.h) — RGB565 values for the 16
HTML4/CSS named colors — rather than raw hex literals, shared with
`05-display`.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 06-text
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)). Check
`lsof /dev/ttyACM0` first if this fails to open the port — a leftover
`picocom` session holding it open is a common cause.

## Verify

Watch the screen — it should cycle every 3 seconds through: a welcome
message, the alphabet/digits, a note about CFB, and "BIG TEXT" in 3x
scaled glyphs with a smaller explanation below it.

Serial console + `device list` at the `uart:~$` prompt works the same way
as every other example in this repo (see
[../01-hello/README.md](../01-hello/README.md)).
