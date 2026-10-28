#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define SAMPLE_INTERVAL K_MSEC(50)

struct key_msg {
	uint8_t key;
	bool pressed;
};

/* The channel is shared between the sampling thread (publisher, below)
 * and whoever wants the readings (observers) - here just
 * key_log_sub, but any thread can later subscribe without the
 * sampling thread ever changing. Same template as 09-gyroscope.
 */
ZBUS_CHAN_DEFINE(key_chan,
		 struct key_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(key_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscriber, not listener: it gets its own thread and message queue,
 * so a slow or blocking reaction here can never stall the sampling
 * thread - see 09-gyroscope's README for why this is the default for
 * every observer in this course, sensors and actuators alike.
 */
ZBUS_SUBSCRIBER_DEFINE(key_log_sub, 4);

static void key_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&key_log_sub, &chan, K_FOREVER)) {
		struct key_msg msg;

		if (chan != &key_chan) {
			continue;
		}

		zbus_chan_read(&key_chan, &msg, K_MSEC(100));

		if (msg.pressed) {
			if (msg.key >= 0x20 && msg.key < 0x7f) {
				LOG_INF("key: '%c' (0x%02x) pressed", msg.key, msg.key);
			} else {
				LOG_INF("key: 0x%02x pressed", msg.key);
			}
		} else {
			LOG_INF("key: released");
		}
	}
}

#define KEY_LOG_STACK_SIZE 1024
#define KEY_LOG_PRIORITY   5

K_THREAD_DEFINE(key_log_tid, KEY_LOG_STACK_SIZE, key_log_thread,
		 NULL, NULL, NULL, KEY_LOG_PRIORITY, 0, 0);

static void key_sampling_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static const struct i2c_dt_spec kb = I2C_DT_SPEC_GET(DT_ALIAS(cardkb));

	if (!i2c_is_ready_dt(&kb)) {
		LOG_ERR("CardKB I2C device is not ready");
		return;
	}

	bool last_pressed = false;

	while (1) {
		uint8_t byte;

		/* CardKB has no register address to write first - a plain
		 * read returns its current state: 0 if no key is pressed,
		 * otherwise the ASCII code of the key.
		 */
		if (i2c_read_dt(&kb, &byte, sizeof(byte)) < 0) {
			LOG_ERR("CardKB read failed");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		bool pressed = (byte != 0);

		/* Only publish on a change, not every poll - a keypress is
		 * naturally event-shaped, same reasoning as
		 * 12-pir-motion's motion_sampling_thread().
		 */
		if (pressed != last_pressed) {
			struct key_msg msg = {
				.key = byte,
				.pressed = pressed,
			};

			/* Publish and move on - a slow or missing
			 * subscriber never blocks sampling for longer
			 * than this timeout.
			 */
			zbus_chan_pub(&key_chan, &msg, K_MSEC(100));

			last_pressed = pressed;
		}

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define KEY_SAMPLE_STACK_SIZE 1024
#define KEY_SAMPLE_PRIORITY   5

K_THREAD_DEFINE(key_sample_tid, KEY_SAMPLE_STACK_SIZE, key_sampling_thread,
		 NULL, NULL, NULL, KEY_SAMPLE_PRIORITY, 0, 0);

int main(void)
{
	/* Everything happens in the sampling thread and the logging
	 * subscriber, both started above via K_THREAD_DEFINE.
	 */
	return 0;
}
