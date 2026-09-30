#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "display_direct.h"
#include "font.h"
#include "colors.h"

LOG_MODULE_REGISTER(main);

/*
 * This demo uses a a hand-rolled bitmap-font renderer
 * (font.c/font.h) Zephyr's own text
 * subsystem, CFB (subsys/fb/cfb.c), doesn't apply here: it hardcodes
 * 8 pixels per byte when sizing its framebuffer (1 bit per pixel) and
 * only ever consults the display's actual pixel format to decide
 * black/white inversion - there's no path from CFB to this panel's
 * 16-bit RGB565 color. draw_char()/draw_string() are what a
 * color-aware equivalent of CFB looks like: render each glyph row
 * straight into a small RGB565 buffer and push it with display_write(),
 * no framebuffer of the whole screen required.
 *
 * draw_char_scaled()/draw_string_scaled() draw the same glyph data
 * blown up as NxN blocks per source pixel - a bigger, blockier look,
 * not a second font - to get larger text without sourcing another
 *  whole separate bitmap font.
 */

static const char *const screens[] = {
	"Hello, M5Stack Fire!\n\nText is a bitmap pushed straight\n to the display driver.",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789",
	"Zephyr's CFB subsystem is\nmono-only (1 bit per pixel) -\nit can't drive this panel's\n16-bit RGB565 color output.",
};

int main(void)
{
	display_direct_init();

	LOG_INF("Ready. Cycling text screens.");

	size_t idx = 0;

	while (1) {
		screen_clear();

		if (idx < ARRAY_SIZE(screens)) {
			draw_string(10, 10, screens[idx], idx == 1 ? COLOR_YELLOW : COLOR_WHITE);
		} else {
			draw_string_scaled(10, 10, "BIG TEXT", COLOR_AQUA, 3);
			draw_string(10, 90, "Same font16 data as above,\ndrawn 3x scaled - see\ndraw_string_scaled() in font.c.",
				    COLOR_WHITE);
		}

		idx = (idx + 1) % (ARRAY_SIZE(screens) + 1);
		k_sleep(K_SECONDS(3));
	}

	return 0;
}
