#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "colors.h"

LOG_MODULE_REGISTER(main);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)
#define ROWS_PER_WRITE 10

/*
 * The ili9342c driver only handles pixel data over SPI - the screen's
 * backlight is a *separate* PWM-driven LED ("lcd-bg" alias) that the
 * display driver knows nothing about. Without turning this on, the
 * screen keeps receiving correct pixel data but stays completely dark.
 */
#define LCD_BG_NODE DT_ALIAS(lcd_bg)
#define LCD_PWM_FREQ_HZ 1000U

static const struct pwm_dt_spec backlight = PWM_DT_SPEC_GET(LCD_BG_NODE);

/*
 * RGB565 (see common/colors.h), but the panel expects it big-endian on
 * the wire - confirmed against a known-working app for this exact
 * board/display (m5stack-fire-soil-ble-server's display_direct.c), not
 * assumed.
 */
static const uint16_t colors[] = {
	COLOR_RED,
	COLOR_LIME, /* the fully-saturated "green" used throughout this repo */
	COLOR_BLUE,
	COLOR_WHITE,
};

static uint16_t row_buf[DISPLAY_WIDTH * ROWS_PER_WRITE];

static const struct device *const display = DEVICE_DT_GET(DISPLAY_NODE);

static int backlight_on(void)
{
	if (!pwm_is_ready_dt(&backlight)) {
		LOG_ERR("Backlight PWM not ready");
		return -ENODEV;
	}

	uint32_t period_ns = NSEC_PER_SEC / LCD_PWM_FREQ_HZ;

	return pwm_set_dt(&backlight, period_ns, period_ns / 2);
}

static void fill_screen(uint16_t color)
{
	uint16_t color_be = sys_cpu_to_be16(color);

	for (size_t i = 0; i < ARRAY_SIZE(row_buf); i++) {
		row_buf[i] = color_be;
	}

	struct display_buffer_descriptor desc = {
		.width = DISPLAY_WIDTH,
		.pitch = DISPLAY_WIDTH,
	};

	for (uint16_t y = 0; y < DISPLAY_HEIGHT; y += ROWS_PER_WRITE) {
		desc.height = MIN(ROWS_PER_WRITE, DISPLAY_HEIGHT - y);
		desc.buf_size = DISPLAY_WIDTH * desc.height * sizeof(uint16_t);

		int rc = display_write(display, 0, y, &desc, row_buf);

		if (rc) {
			LOG_ERR("display_write failed at row %d: %d", y, rc);
			return;
		}
	}
}

int main(void)
{
	if (!device_is_ready(display)) {
		LOG_ERR("Display device %s not ready", display->name);
		return 0;
	}

	if (backlight_on()) {
		LOG_ERR("Failed to turn on backlight");
		return 0;
	}

	display_blanking_off(display);

	LOG_INF("Display %s ready, %dx%d, cycling colors", display->name,
		DISPLAY_WIDTH, DISPLAY_HEIGHT);

	size_t idx = 0;

	while (1) {
		fill_screen(colors[idx]);
		idx = (idx + 1) % ARRAY_SIZE(colors);
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
