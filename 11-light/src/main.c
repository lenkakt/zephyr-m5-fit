#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified - see boards/m5stack_fire_esp32_procpu.overlay"
#endif

#define SAMPLE_INTERVAL K_SECONDS(2)

/* No lux conversion here on purpose: a bare photoresistor's raw ADC
 * count doesn't map to a physical unit without per-device calibration
 * against a real light meter, which nobody has done for this probe.
 * Report the raw count and let whoever uses this decide what "bright"
 * means for their own project.
 *
 * Also deliberately one ADC read per publish, not an internal
 * fast-sampled rolling average: the ESP32 ADC driver re-applies its
 * hardware calibration code on every single read
 * (adc_set_hw_calibration_code(), called unconditionally from
 * adc_esp32_read() in drivers/adc/adc_esp32.c), and that is audible on
 * this board's onboard speaker - confirmed on real hardware to scale
 * with read frequency, not just happen once at boot. Reading at 10 Hz
 * (for a 20-sample window) was audible as constant clicking; reading
 * once every couple of seconds is not.
 */
struct light_msg {
	int16_t raw;
};

/* The channel is shared between the sampling thread (publisher, below)
 * and whoever wants the readings (observers) - here just
 * light_log_sub, but any thread can later subscribe without the
 * sampling thread ever changing. Same template as 09-gyroscope.
 */
ZBUS_CHAN_DEFINE(light_chan,
		 struct light_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(light_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscriber, not listener: it gets its own thread and message queue,
 * so a slow or blocking reaction here can never stall the sampling
 * thread - see 09-gyroscope's README for why this is the default for
 * every observer in this course, sensors and actuators alike.
 */
ZBUS_SUBSCRIBER_DEFINE(light_log_sub, 4);

static void light_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&light_log_sub, &chan, K_FOREVER)) {
		struct light_msg msg;

		if (chan != &light_chan) {
			continue;
		}

		zbus_chan_read(&light_chan, &msg, K_MSEC(100));

		LOG_INF("light: raw=%d", msg.raw);
	}
}

#define LIGHT_LOG_STACK_SIZE 1024
#define LIGHT_LOG_PRIORITY   5

K_THREAD_DEFINE(light_log_tid, LIGHT_LOG_STACK_SIZE, light_log_thread,
		 NULL, NULL, NULL, LIGHT_LOG_PRIORITY, 0, 0);

static void light_sampling_thread(void *p1, void *p2, void *p3)
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

	while (1) {
		if (adc_read_dt(&sensor, &sequence) < 0) {
			LOG_ERR("ADC read failed");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		struct light_msg msg = {
			.raw = raw,
		};

		/* Publish and move on - a slow or missing subscriber never
		 * blocks sampling for longer than this timeout.
		 */
		zbus_chan_pub(&light_chan, &msg, K_MSEC(100));

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define LIGHT_SAMPLE_STACK_SIZE 1024
#define LIGHT_SAMPLE_PRIORITY   5

K_THREAD_DEFINE(light_sample_tid, LIGHT_SAMPLE_STACK_SIZE, light_sampling_thread,
		 NULL, NULL, NULL, LIGHT_SAMPLE_PRIORITY, 0, 0);

int main(void)
{
	/* Everything happens in the sampling thread and the logging
	 * subscriber, both started above via K_THREAD_DEFINE.
	 */
	return 0;
}
