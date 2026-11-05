/*
 * Copyright (c) 2026 Lenka Koskova Triskova
 *
 * SPDX-License-Identifier: Apache-2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/poll.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(main);

/* Set these to what you're given in class - see README.md. A plain
 * dotted IPv4 address, not a hostname - this example doesn't do DNS.
 */
#define BROKER_HOST "10.42.0.1"
#define BROKER_PORT 1883

/* Give this a unique value per device - if two clients connect with
 * the same client_id, the broker drops the older connection.
 */
#define CLIENT_ID "m5-fire-mqtt-demo"

/* This example talks on the same single topic it subscribes to - the
 * broker echoes a client's own publish back to itself, so this is
 * enough to see a full publish/subscribe round trip on just one
 * device, no second board required. Change it to something per-device
 * (and subscribe/publish to different topics) to try it between two
 * boards instead.
 */
#define TOPIC "zephyr-m5-fit/test"

#define RX_BUF_SIZE 256
#define POLL_TIMEOUT_MS 500
#define PUBLISH_EVERY_MS 1000

static uint8_t rx_buffer[RX_BUF_SIZE];
static uint8_t tx_buffer[RX_BUF_SIZE];
static struct mqtt_client client;
static struct sockaddr_storage broker;

static void subscribe(struct mqtt_client *c)
{
	struct mqtt_topic sub_topic = {
		.topic = {
			.utf8 = TOPIC,
			.size = strlen(TOPIC),
		},
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
	};
	struct mqtt_subscription_list sub_list = {
		.list = &sub_topic,
		.list_count = 1,
		.message_id = 1,
	};

	int rc = mqtt_subscribe(c, &sub_list);

	if (rc != 0) {
		LOG_ERR("mqtt_subscribe failed: %d", rc);
	} else {
		LOG_INF("Subscribed to '%s'", TOPIC);
	}
}

static void mqtt_event_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed: %d", evt->result);
			break;
		}
		LOG_INF("MQTT connected");
		subscribe(c);
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_WRN("MQTT disconnected");
		break;

	case MQTT_EVT_SUBACK:
		LOG_INF("Subscription acknowledged");
		break;

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;
		uint8_t payload[32];
		int len = mqtt_read_publish_payload(c, payload, sizeof(payload) - 1);

		if (len < 0) {
			LOG_ERR("Failed to read publish payload: %d", len);
			break;
		}
		payload[len] = '\0';

		LOG_INF("Received on '%s': %s", p->message.topic.topic.utf8, payload);
		break;
	}

	default:
		break;
	}
}

static int broker_init(void)
{
	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(BROKER_PORT);
	return inet_pton(AF_INET, BROKER_HOST, &broker4->sin_addr);
}

/* Gated on Wi-Fi being up - see event_handler() below. Everything in
 * this thread is either a blocking call (mqtt_connect's TCP handshake,
 * poll()) or needs to happen in a loop that never ends, so it gets its
 * own thread rather than running in main() or on the system workqueue -
 * the same reasoning as the sampling thread in 09-gyroscope, just with
 * a socket standing in for a sensor.
 */
static K_SEM_DEFINE(net_ready, 0, 1);

static void mqtt_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_take(&net_ready, K_FOREVER);

	if (broker_init() != 1) {
		LOG_ERR("Invalid broker address: %s", BROKER_HOST);
		return;
	}

	mqtt_client_init(&client);
	client.broker = &broker;
	client.evt_cb = mqtt_event_handler;
	client.client_id.utf8 = CLIENT_ID;
	client.client_id.size = strlen(CLIENT_ID);
	client.protocol_version = MQTT_VERSION_3_1_1;
	client.rx_buf = rx_buffer;
	client.rx_buf_size = sizeof(rx_buffer);
	client.tx_buf = tx_buffer;
	client.tx_buf_size = sizeof(tx_buffer);
	client.transport.type = MQTT_TRANSPORT_NON_SECURE;

	LOG_INF("Connecting to MQTT broker at %s:%d", BROKER_HOST, BROKER_PORT);

	if (mqtt_connect(&client) != 0) {
		LOG_ERR("mqtt_connect failed");
		return;
	}

	struct pollfd fds[1] = {
		{ .fd = client.transport.tcp.sock, .events = POLLIN },
	};
	uint32_t counter = 0;
	uint16_t msg_id = 1;
	int64_t next_publish = k_uptime_get() + PUBLISH_EVERY_MS;

	/* mqtt_input() (process anything received) and mqtt_live() (send
	 * a keep-alive if one is due) both need calling regularly - Zephyr
	 * doesn't run a background thread for you to do that. This also
	 * publishes the next counter value once a second.
	 */
	while (1) {
		if (poll(fds, 1, POLL_TIMEOUT_MS) > 0) {
			mqtt_input(&client);
		}
		mqtt_live(&client);

		if (k_uptime_get() >= next_publish) {
			char payload[12];
			int len = snprintf(payload, sizeof(payload), "%u", counter++);
			struct mqtt_publish_param param = {
				.message.topic.topic.utf8 = TOPIC,
				.message.topic.topic.size = strlen(TOPIC),
				.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
				.message.payload.data = payload,
				.message.payload.len = len,
				.message_id = msg_id++,
			};

			if (mqtt_publish(&client, &param) == 0) {
				LOG_INF("Published \"%s\" to '%s'", payload, TOPIC);
			} else {
				LOG_ERR("mqtt_publish failed");
			}

			next_publish += PUBLISH_EVERY_MS;
		}
	}
}

#define MQTT_THREAD_STACK_SIZE 2048
#define MQTT_THREAD_PRIORITY   5

K_THREAD_DEFINE(mqtt_tid, MQTT_THREAD_STACK_SIZE, mqtt_thread,
		 NULL, NULL, NULL, MQTT_THREAD_PRIORITY, 0, 0);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void count_ssid(void *cb_arg, const char *ssid, size_t ssid_len)
{
	size_t *count = cb_arg;

	(*count)++;
}

static void event_handler(struct net_mgmt_event_callback *cb,
			  uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		LOG_INF("Wi-Fi connected");
	}
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		LOG_INF("Got an IPv4 address - the MQTT thread can proceed");
		k_sem_give(&net_ready);
	}
}

int main(void)
{
	/* Wi-Fi bring-up only - same as 07-wifi/08-http. The MQTT thread
	 * (started above via K_THREAD_DEFINE) does the rest, once
	 * NET_EVENT_IPV4_ADDR_ADD below releases it.
	 */
	struct net_if *iface = net_if_get_wifi_sta();

	if (iface == NULL) {
		LOG_ERR("No Wi-Fi STA interface found");
		return 0;
	}

	net_mgmt_init_event_callback(&wifi_cb, event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	/* Connect using whatever credential was provisioned over the
	 * shell with `wifi cred add` - see zephyr-m5-fit/07-wifi.
	 */
	size_t cred_count = 0;

	wifi_credentials_for_each_ssid(count_ssid, &cred_count);

	if (cred_count == 0) {
		LOG_ERR("No stored Wi-Fi credential - provision one first, over the shell:");
		LOG_INF("  wifi cred add -s \"<SSID>\" -k 1 -p \"<password>\"");
		return 0;
	}

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);

	if (ret) {
		LOG_ERR("Connect failed: %d", ret);
	}

	return 0;
}
