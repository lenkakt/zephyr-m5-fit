#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

/* The ESP32 ADC driver re-applies its hardware calibration code on
 * every single read, which is audible on this board's onboard speaker
 * and scales with read frequency - see the main zephyr-m5-fit README's
 * "Known issue: using the ADC makes a sound" section. Sampling every 2
 * seconds (same as 10-humidity/11-light) keeps that click infrequent.
 */
#define SAMPLE_INTERVAL K_SECONDS(2)

/* How much the raw reading has to move between samples to count as a
 * turn rather than noise.
 */
#define ANGLE_THRESHOLD 30

enum angle_direction {
	ANGLE_NONE,
	ANGLE_LEFT,
	ANGLE_RIGHT,
};

/* No degrees conversion here on purpose, same reasoning as 11-light: a
 * bare potentiometer's raw ADC count doesn't map to a physical angle
 * without calibrating against this specific unit's real rotation
 * range, which nobody has done. Report the raw count and a simple
 * turned-left/turned-right/no-change direction, and let whoever uses
 * this decide what to do with it.
 */
struct angle_msg {
	int16_t raw;
	enum angle_direction direction;
};

/* The channel is shared between the sampling thread (publisher, below)
 * and whoever wants the readings (observers) - here just
 * angle_log_sub, but any thread can later subscribe without the
 * sampling thread ever changing. Same template as 09-gyroscope.
 */
ZBUS_CHAN_DEFINE(angle_chan,
		 struct angle_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(angle_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscriber, not listener: it gets its own thread and message queue,
 * so a slow or blocking reaction here can never stall the sampling
 * thread - see 09-gyroscope's README for why this is the default for
 * every observer in this course, sensors and actuators alike.
 */
ZBUS_SUBSCRIBER_DEFINE(angle_log_sub, 4);

static const char *direction_str(enum angle_direction dir)
{
	switch (dir) {
	case ANGLE_LEFT:
		return "L";
	case ANGLE_RIGHT:
		return "R";
	default:
		return "N";
	}
}

static void angle_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&angle_log_sub, &chan, K_FOREVER)) {
		struct angle_msg msg;

		if (chan != &angle_chan) {
			continue;
		}

		zbus_chan_read(&angle_chan, &msg, K_MSEC(100));

		LOG_INF("angle: raw=%d direction=%s", msg.raw, direction_str(msg.direction));
	}
}

#define ANGLE_LOG_STACK_SIZE 1024
#define ANGLE_LOG_PRIORITY   5

K_THREAD_DEFINE(angle_log_tid, ANGLE_LOG_STACK_SIZE, angle_log_thread,
		 NULL, NULL, NULL, ANGLE_LOG_PRIORITY, 0, 0);

static void angle_sampling_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static const struct adc_dt_spec sensor = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

	if (!adc_is_ready_dt(&sensor)) {
		LOG_ERR("ADC device is not ready");
		return;
	}
	if (adc_channel_setup_dt(&sensor) < 0) {
		LOG_ERR("Failed to set up ADC channel");
		return;
	}

	/* Only the buffer pointer changes per sample - everything
	 * adc_sequence_init_dt() derives from the devicetree (channel
	 * mask, resolution, oversampling) is fixed, so it only needs to
	 * run once, not on every iteration.
	 */
	int16_t raw;
	struct adc_sequence sequence = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	adc_sequence_init_dt(&sensor, &sequence);

	int16_t last_raw = 0;

	while (1) {
		if (adc_read_dt(&sensor, &sequence) < 0) {
			LOG_ERR("ADC read failed");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		enum angle_direction direction = ANGLE_NONE;

		if (abs(raw - last_raw) > ANGLE_THRESHOLD) {
			direction = (raw > last_raw) ? ANGLE_RIGHT : ANGLE_LEFT;
			last_raw = raw;
		}

		struct angle_msg msg = {
			.raw = raw,
			.direction = direction,
		};

		/* Publish and move on - a slow or missing subscriber never
		 * blocks sampling for longer than this timeout.
		 */
		zbus_chan_pub(&angle_chan, &msg, K_MSEC(100));

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define ANGLE_SAMPLE_STACK_SIZE 1024
#define ANGLE_SAMPLE_PRIORITY   5

K_THREAD_DEFINE(angle_sample_tid, ANGLE_SAMPLE_STACK_SIZE, angle_sampling_thread,
		 NULL, NULL, NULL, ANGLE_SAMPLE_PRIORITY, 0, 0);

int main(void)
{
	/* Everything happens in the sampling thread and the logging
	 * subscriber, both started above via K_THREAD_DEFINE.
	 */
	return 0;
}
