#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define BRIGHTNESS 0x20

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(BRIGHTNESS, 0x00, 0x00), /* red */
	RGB(0x00, BRIGHTNESS, 0x00), /* green */
	RGB(0x00, 0x00, BRIGHTNESS), /* blue */
	RGB(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS), /* white */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

int main(void)
{
	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return 0;
	}

	LOG_INF("Lighting up %d pixels on %s", STRIP_NUM_PIXELS, strip->name);

	size_t color = 0;

	while (1) {
		for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
			pixels[i] = colors[color];
		}

		int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

		if (rc) {
			LOG_ERR("couldn't update strip: %d", rc);
		}

		color = (color + 1) % ARRAY_SIZE(colors);
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
