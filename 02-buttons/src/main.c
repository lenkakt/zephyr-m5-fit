#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

static const char *button_name(uint16_t code)
{
	switch (code) {
	case INPUT_KEY_0:
		return "left";
	case INPUT_KEY_1:
		return "middle";
	case INPUT_KEY_2:
		return "right";
	case INPUT_KEY_3:
		return "2 (side)";
	default:
		return "unknown";
	}
}

static void button_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	LOG_INF("Button %s (code %d) %s", button_name(evt->code), evt->code,
		evt->value ? "pressed" : "released");
}

INPUT_CALLBACK_DEFINE(NULL, button_input_cb, NULL);

int main(void)
{
	LOG_INF("Ready - press any of the four M5Stack Fire buttons");

	return 0;
}
