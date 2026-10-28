#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define SAMPLE_INTERVAL K_MSEC(20)

struct motion_msg {
	bool detected;
};

/* The channel is shared between the sampling thread (publisher, below)
 * and whoever wants the readings (observers) - here just
 * motion_log_sub, but any thread can later subscribe without the
 * sampling thread ever changing. Same template as 09-gyroscope.
 */
ZBUS_CHAN_DEFINE(motion_chan,
		 struct motion_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(motion_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscriber, not listener: it gets its own thread and message queue,
 * so a slow or blocking reaction here can never stall the sampling
 * thread - see 09-gyroscope's README for why this is the default for
 * every observer in this course, sensors and actuators alike.
 */
ZBUS_SUBSCRIBER_DEFINE(motion_log_sub, 4);

static void motion_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&motion_log_sub, &chan, K_FOREVER)) {
		struct motion_msg msg;

		if (chan != &motion_chan) {
			continue;
		}

		zbus_chan_read(&motion_chan, &msg, K_MSEC(100));

		LOG_INF("motion: %s", msg.detected ? "DETECTED" : "clear");
	}
}

#define MOTION_LOG_STACK_SIZE 1024
#define MOTION_LOG_PRIORITY   5

K_THREAD_DEFINE(motion_log_tid, MOTION_LOG_STACK_SIZE, motion_log_thread,
		 NULL, NULL, NULL, MOTION_LOG_PRIORITY, 0, 0);

static void motion_sampling_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static const struct gpio_dt_spec pir = GPIO_DT_SPEC_GET(DT_ALIAS(pir), gpios);

	if (!device_is_ready(pir.port)) {
		LOG_ERR("PIR GPIO is not ready");
		return;
	}
	if (gpio_pin_configure_dt(&pir, GPIO_INPUT) < 0) {
		LOG_ERR("Failed to configure PIR GPIO");
		return;
	}

	bool last_detected = false;

	while (1) {
		int val = gpio_pin_get_dt(&pir);

		if (val < 0) {
			LOG_ERR("PIR read failed");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		bool detected = (val == 1);

		/* Only publish on a change, not every poll - motion state
		 * is naturally event-shaped (it changes rarely compared to
		 * how often it's checked), unlike 09-gyroscope/10-humidity/
		 * 11-light's continuously-varying readings.
		 */
		if (detected != last_detected) {
			struct motion_msg msg = {
				.detected = detected,
			};

			/* Publish and move on - a slow or missing
			 * subscriber never blocks sampling for longer
			 * than this timeout.
			 */
			zbus_chan_pub(&motion_chan, &msg, K_MSEC(100));

			last_detected = detected;
		}

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define MOTION_SAMPLE_STACK_SIZE 1024
#define MOTION_SAMPLE_PRIORITY   5

K_THREAD_DEFINE(motion_sample_tid, MOTION_SAMPLE_STACK_SIZE, motion_sampling_thread,
		 NULL, NULL, NULL, MOTION_SAMPLE_PRIORITY, 0, 0);

int main(void)
{
	/* Everything happens in the sampling thread and the logging
	 * subscriber, both started above via K_THREAD_DEFINE.
	 */
	return 0;
}
