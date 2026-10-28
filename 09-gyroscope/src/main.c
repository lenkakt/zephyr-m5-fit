#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define SAMPLE_INTERVAL K_MSEC(500)

struct gyro_msg {
	struct sensor_value x;
	struct sensor_value y;
	struct sensor_value z;
};

/* The channel is shared between the sampling
 * thread (publisher, below) and whoever wants the readings (observers) -
 * here just gyro_log_sub, but any thread can later subscribe without
 * the sampling thread ever changing.
 */
ZBUS_CHAN_DEFINE(gyro_chan,
		 struct gyro_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(gyro_log_sub),
		 ZBUS_MSG_INIT(0)
);

/* Subscribers get their own thread and message queue, so a slow (or
 * blocking) reaction here can never stall the sampling thread above -
 * unlike a listener, which would run straight inside it. This is the
 * default for every observer in this course, sensors and actuators
 * alike, precisely so nobody has to judge case by case whether their
 * reaction counts as "slow enough" to need it.
 */
ZBUS_SUBSCRIBER_DEFINE(gyro_log_sub, 4);

static void gyro_log_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&gyro_log_sub, &chan, K_FOREVER)) {
		struct gyro_msg msg;

		if (chan != &gyro_chan) {
			continue;
		}

		zbus_chan_read(&gyro_chan, &msg, K_MSEC(100));

		LOG_INF("gyro (rad/s): x=%d.%06d y=%d.%06d z=%d.%06d",
			msg.x.val1, msg.x.val2,
			msg.y.val1, msg.y.val2,
			msg.z.val1, msg.z.val2);
	}
}

#define GYRO_LOG_STACK_SIZE 1024
#define GYRO_LOG_PRIORITY   5

K_THREAD_DEFINE(gyro_log_tid, GYRO_LOG_STACK_SIZE, gyro_log_thread,
		 NULL, NULL, NULL, GYRO_LOG_PRIORITY, 0, 0);

static void gyro_sampling_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *const mpu = DEVICE_DT_GET_ONE(invensense_mpu6050);

	if (!device_is_ready(mpu)) {
		LOG_ERR("Gyroscope device %s is not ready", mpu->name);
		return;
	}

	while (1) {
		struct sensor_value gyro[3];

		if (sensor_sample_fetch(mpu) < 0) {
			LOG_ERR("Failed to fetch sample");
			k_sleep(SAMPLE_INTERVAL);
			continue;
		}

		sensor_channel_get(mpu, SENSOR_CHAN_GYRO_XYZ, gyro);

		struct gyro_msg msg = {
			.x = gyro[0],
			.y = gyro[1],
			.z = gyro[2],
		};

		/* Publish and move on - a slow or missing subscriber never
		 * blocks sampling for longer than this timeout.
		 */
		zbus_chan_pub(&gyro_chan, &msg, K_MSEC(100));

		k_sleep(SAMPLE_INTERVAL);
	}
}

#define GYRO_THREAD_STACK_SIZE 1024
#define GYRO_THREAD_PRIORITY   5

K_THREAD_DEFINE(gyro_tid, GYRO_THREAD_STACK_SIZE, gyro_sampling_thread,
		 NULL, NULL, NULL, GYRO_THREAD_PRIORITY, 0, 0);

int main(void)
{
	/* Nothing to do here - the sampling thread and the logging
	 * subscriber (both started above via K_THREAD_DEFINE) do all
	 * the work.
	 */
	return 0;
}
