# 05-display

Fills the M5Stack Fire's built-in 320×240 color screen with a solid color,
cycling red → green → blue → white every second — the display equivalent
of `03-led-strip`.

## Why no devicetree overlay is needed here

Unlike the LED strip, the display is already fully described in Zephyr's
mainline `m5stack_fire` devicetree: an `ili9342c` panel node, wired up over
the `mipi_dbi` SPI wrapper, already `status = "okay"`, and already marked
as *the* display via `chosen { zephyr,display = &ili9342c; };`. So this
example only needs `CONFIG_DISPLAY=y` — no overlay.

## What I checked, and why this isn't using Zephyr's higher-level display subsystem

Zephyr actually has *two* display-related layers, and it's worth being
explicit about which one this uses and why:

- **`CONFIG_DISPLAY`** (used here) — the low-level driver API: write raw
  pixel data to a rectangular region. Works with any pixel format a driver
  supports.
- **CFB, the Character FrameBuffer subsystem** (`CONFIG_CHARACTER_FRAMEBUFFER`,
  `subsys/fb/cfb.c`) — a higher-level text/shape API (`cfb_print()`,
  `cfb_draw_line()`, etc.), with its own shell (`cfb_shell` sample) that
  would have fit this project's shell-driven verification pattern nicely.

I checked whether CFB would work here before writing any code, and it
won't: CFB's own `cmd_init` (and the plain `samples/subsys/display/cfb`
sample) both hardcode `display_set_pixel_format(dev, PIXEL_FORMAT_MONO10)`,
falling back to `PIXEL_FORMAT_MONO01` — i.e. **CFB is built for monochrome
displays**. The `ili9342c` driver
(`drivers/display/display_ili9xxx.c`) only ever advertises
`PIXEL_FORMAT_RGB_565 | PIXEL_FORMAT_RGB_888 | PIXEL_FORMAT_RGB_565X` as
supported — neither mono format — so CFB's init would simply fail on this
board. That's a real limitation of CFB, not something wrong with this
board or this app; a proper CFB-on-color-display would need someone to add
RGB support to `subsys/fb/cfb.c` itself.

So this example talks to the raw `CONFIG_DISPLAY` API directly instead,
following the same approach as Zephyr's own
`samples/drivers/display/src/display.c`.

## How the code works

### Sizing a buffer without touching the whole 320×240 screen at once

A full-screen RGB565 buffer would be 320 × 240 × 2 bytes ≈ 150 KB — far
more RAM than is worth spending on a "fill it with one color" demo. So the
code allocates a buffer covering the full width but only 10 rows at a
time, and writes the screen in 24 passes (240 ÷ 10) instead of one:

```c
#define ROWS_PER_WRITE 10
static uint16_t row_buf[DISPLAY_WIDTH * ROWS_PER_WRITE];
```

### Reading resolution from the devicetree, not hardcoding it

Same idea as `chain-length` in the LED strip example: the panel's
`width`/`height` properties are already in the devicetree
(`ili9342c@0 { width = <320>; height = <240>; ... }`), so the code reads
them instead of hardcoding `320`/`240` — if this ran on a different-size
panel, only the devicetree would need to change.

```c
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)
```

`DT_CHOSEN(zephyr_display)` is the counterpart to `DT_ALIAS(...)` used
elsewhere in this repo — it resolves whichever node the devicetree's
`chosen { zephyr,display = ...; }` points at, rather than a named alias.

### Writing pixels

`display_write()` takes an (x, y) origin, a
`struct display_buffer_descriptor` describing the buffer's shape, and the
raw pixel bytes. The loop just slides that 10-row window down the screen:

```c
for (uint16_t y = 0; y < DISPLAY_HEIGHT; y += ROWS_PER_WRITE) {
	desc.height = MIN(ROWS_PER_WRITE, DISPLAY_HEIGHT - y);
	desc.buf_size = DISPLAY_WIDTH * desc.height * sizeof(uint16_t);
	display_write(display, 0, y, &desc, row_buf);
}
```

### The backlight is a separate thing entirely

The `ili9342c` driver only pushes pixel data over SPI — it has no idea the
screen even has a backlight. On the M5Stack Fire, the backlight is its own
PWM-driven LED (the `lcd-bg` alias), controlled completely independently.
Skip this and the display is receiving perfectly correct pixel data while
staying completely dark. (This is exactly what happened on the first pass
of this example — see below.)

```c
#define LCD_BG_NODE DT_ALIAS(lcd_bg)
static const struct pwm_dt_spec backlight = PWM_DT_SPEC_GET(LCD_BG_NODE);
```

`pwm_set_dt(&backlight, period_ns, period_ns / 2)` sets a 1 kHz PWM at 50%
duty — on, at half brightness. This has to happen once, before the first
`display_write()`, or the pixels are correct but invisible.

### Byte order matters, and I initially got it backwards

Colors are RGB565 (`0xF800` = red, `0x07E0` = green, ...), but the bytes
have to go out **big-endian** — `sys_cpu_to_be16(color)` before writing.
My first pass at this file assumed native (little-endian) byte order,
reasoning from how Zephyr's generic `samples/drivers/display` sample
handles its two RGB565 variants. That reasoning was wrong for this panel.

**Confirmed against real, previously-verified code for this exact
board/display** — `~/work/m5stack-fire-soil-ble-server`'s
`display_direct.c`, whose own doc comment says explicitly: *"pixels must
point to len uint16_t values in RGB565 big-endian byte order."* Fixed to
match.

**What's verified:** builds cleanly; the backlight PWM and pixel byte
order both now match a known-working reference for this exact
board/panel, not just first-principles reasoning. **Not yet verified on
this specific app:** actual colors on the physical screen — but the
approach it now follows is the one already proven to work, not a guess.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 05-display
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Watch the screen — it should cycle red, green, blue, white, one second
each, filling the whole 320×240 area, at half brightness (bump the duty
cycle in `backlight_on()` if you want it brighter).

If the screen stays dark: check the backlight PWM section above first —
that was the actual cause the first time this example didn't work. If
colors show but look wrong (e.g. red appears blue), try removing the
`sys_cpu_to_be16()` call — that would mean this particular board/driver
combination wants native byte order after all, contrary to what the
reference app's own comment says.

Open a serial console (see [../01-hello/README.md](../01-hello/README.md))
and run `device list` at the `uart:~$` prompt — the `ili9342c@0` display
device should show as `READY`, independent of whether the colors on
screen are correct.
