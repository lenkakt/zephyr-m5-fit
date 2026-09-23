#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

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

/*
 * WS2812/SK6812 pixels have no "off" state of their own - each one just
 * keeps displaying whatever color it last received, forever, until a new
 * frame arrives. This flag is how the shell's "led off"/"led on" commands
 * pause and resume the color cycle below, instead of it immediately
 * overwriting a manually-cleared strip on the next loop iteration.
 */
static bool strip_on = true;

static void strip_clear(void)
{
	memset(pixels, 0, sizeof(pixels));
	int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

	if (rc) {
		LOG_ERR("couldn't update strip: %d", rc);
	}
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	strip_on = false;
	strip_clear();
	shell_print(sh, "LED strip off");

	return 0;
}

static int cmd_led_on(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	strip_on = true;
	shell_print(sh, "LED strip resuming color cycle");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(led_cmds,
	SHELL_CMD(off, NULL, "Turn the LED strip off", cmd_led_off),
	SHELL_CMD(on, NULL, "Resume the color cycle", cmd_led_on),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &led_cmds, "Control the onboard LED strip", NULL);

int main(void)
{
	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return 0;
	}

	LOG_INF("Lighting up %d pixels on %s", STRIP_NUM_PIXELS, strip->name);
	LOG_INF("Shell commands: 'led off' / 'led on'");

	size_t color = 0;

	while (1) {
		if (strip_on) {
			for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
				pixels[i] = colors[color];
			}

			int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

			if (rc) {
				LOG_ERR("couldn't update strip: %d", rc);
			}

			color = (color + 1) % ARRAY_SIZE(colors);
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
