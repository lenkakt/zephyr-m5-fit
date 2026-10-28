#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified - see boards/m5stack_fire_esp32_procpu.overlay"
#endif

#define SAMPLE_INTERVAL K_SECONDS(2)

/* Calibrate these against your own probe: watch the "raw=" value in the
 * log output in open air and in wet soil, and set these two to match.
 */
#define ADC_MIN_VALUE 830
#define ADC_MAX_VALUE 1940
#define PCT_MIN       0
#define PCT_MAX       100

struct humidity_msg {
	int16_t raw;
	int16_t percent;
	bool threshold_high;
};

/* The channel is shared between the sampling thread (publisher, below)
 * and whoever wants the readings (observers) - here just
 * humidity_log_sub, but any thread can later subscribe without the
 * sampling thread ever changing. Same template as 09-gyroscope.
 */
ZBUS_CHAN_DEFINE(humidity_chan,
		 struct humidity_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(humidity_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscriber, not listener: it gets its own thread and message queue,
 * so a slow or blocking reaction here can never stall the sampling
 * thread - see 09-gyroscope's README for why this is the default for
 * every observer in this course, sensors and actuators alike.
 */
ZBUS_SUBSCRIBER_DEFINE(humidity_log_sub, 4);

static void humidity_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&humidity_log_sub, &chan, K_FOREVER)) {
		struct humidity_msg msg;

		if (chan != &humidity_chan) {
			continue;
		}

		zbus_chan_read(&humidity_chan, &msg, K_MSEC(100));

		LOG_INF("humidity: %d%% (raw=%d, threshold %s)",
			msg.percent, msg.raw, msg.threshold_high ? "HIGH" : "LOW");
	}
}

#define HUMIDITY_LOG_STACK_SIZE 1024
#define HUMIDITY_LOG_PRIORITY   5

K_THREAD_DEFINE(humidity_log_tid, HUMIDITY_LOG_STACK_SIZE, humidity_log_thread,
		 NULL, NULL, NULL, HUMIDITY_LOG_PRIORITY, 0, 0);

static void humidity_sampling_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static const struct adc_dt_spec probe = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
	static const struct gpio_dt_spec thresh = GPIO_DT_SPEC_GET(DT_ALIAS(thresh), gpios);

	if (!adc_is_ready_dt(&probe)) {
		LOG_ERR("ADC device is not ready");
		return;
	}
	if (adc_channel_setup_dt(&probe) < 0) {
		LOG_ERR("Failed to set up ADC channel");
		return;
	}
	if (!device_is_ready(thresh.port)) {
		LOG_ERR("Threshold GPIO is not ready");
		return;
	}
	if (gpio_pin_configure_dt(&thresh, GPIO_INPUT) < 0) {
		LOG_ERR("Failed to configure threshold GPIO");
		return;
	}

	/* Only the buffer pointer changes per sample (filled in below) -
	 * everything adc_sequence_init_dt() derives from the devicetree
	 * (channel mask, resolution, oversampling) is fixed, so it only
	 * needs to run once, not on every iteration.
	 */
	int16_t raw;
	struct adc_sequence sequence = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	adc_sequence_init_dt(&probe, &sequence);

	while (1) {
		if (adc_read_dt(&probe, &sequence) < 0) {
			LOG_ERR("ADC read failed");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		int32_t pct = PCT_MAX - (raw - ADC_MIN_VALUE) *
				(PCT_MAX - PCT_MIN) / (ADC_MAX_VALUE - ADC_MIN_VALUE);
		pct = CLAMP(pct, PCT_MIN, PCT_MAX);

		struct humidity_msg msg = {
			.raw = raw,
			.percent = (int16_t)pct,
			.threshold_high = gpio_pin_get_dt(&thresh) == 1,
		};

		/* Publish and move on - a slow or missing subscriber never
		 * blocks sampling for longer than this timeout.
		 */
		zbus_chan_pub(&humidity_chan, &msg, K_MSEC(100));

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define HUMIDITY_SAMPLE_STACK_SIZE 1024
#define HUMIDITY_SAMPLE_PRIORITY   5

K_THREAD_DEFINE(humidity_sample_tid, HUMIDITY_SAMPLE_STACK_SIZE, humidity_sampling_thread,
		 NULL, NULL, NULL, HUMIDITY_SAMPLE_PRIORITY, 0, 0);

int main(void)
{
	/* Everything happens in the sampling thread and the logging
	 * subscriber, both started above via K_THREAD_DEFINE.
	 */
	return 0;
}
