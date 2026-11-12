/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * The broker library has no app-level "notify me when something is
 * published" hook, so this is what "the broker also subscribes" has
 * to mean in practice: an ordinary wolfMQTT *client*, in its own
 * thread, connecting to this same device's broker over loopback
 * exactly like any remote subscriber would.
 *
 * This is the template, not the full solution: it only logs what it
 * receives. Forwarding those readings to InfluxDB/Grafana over the
 * STA/uplink Wi-Fi leg - the actual "connects to the internet" job -
 * is deliberately left out; that's where the real solution's logic
 * goes, in forward_reading() below.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "wolfmqtt/mqtt_client.h"
#include "client_net.h"
#include "uplink.h"

#define BROKER_HOST    "127.0.0.1"
#define BROKER_PORT    1883
#define CLIENT_ID      "gateway-uplink"
#define CMD_TIMEOUT_MS 5000
#define KEEP_ALIVE_SEC 60

static MqttClient client;
static MqttNet net;
static byte tx_buf[256];
static byte rx_buf[256];

static void forward_reading(const char *topic, const byte *payload, word32 payload_len)
{
	/* The full solution forwards this to InfluxDB/Grafana over the
	 * STA/uplink interface here. This template just proves the
	 * mechanism: the gateway received, on its own broker, something
	 * a sensor node published to it.
	 */
	printk("uplink: received on '%s': %.*s\n", topic, (int)payload_len, payload);
}

static int msg_cb(MqttClient *c, MqttMessage *msg, byte msg_new, byte msg_done)
{
	ARG_UNUSED(c);

	/* Sensor payloads are tiny - always one chunk. */
	if (msg_new && msg_done) {
		forward_reading(msg->topic_name, msg->buffer, msg->buffer_len);
	}
	return MQTT_CODE_SUCCESS;
}

static int uplink_connect_once(void)
{
	MqttConnect mc = {0};
	MqttSubscribe sub = {0};
	MqttTopic topic = {0};
	int rc;

	client_net_init(&net);

	rc = MqttClient_Init(&client, &net, msg_cb, tx_buf, sizeof(tx_buf), rx_buf,
			      sizeof(rx_buf), CMD_TIMEOUT_MS);
	if (rc != MQTT_CODE_SUCCESS) {
		return rc;
	}

	rc = MqttClient_NetConnect(&client, BROKER_HOST, BROKER_PORT, CMD_TIMEOUT_MS, 0, NULL);
	if (rc != MQTT_CODE_SUCCESS) {
		return rc;
	}

	mc.keep_alive_sec = KEEP_ALIVE_SEC;
	mc.clean_session = 1;
	mc.client_id = CLIENT_ID;
	rc = MqttClient_Connect(&client, &mc);
	if (rc != MQTT_CODE_SUCCESS) {
		return rc;
	}

	topic.topic_filter = CONFIG_APP_UPLINK_TOPIC_FILTER;
	topic.qos = MQTT_QOS_0;
	sub.topic_count = 1;
	sub.topics = &topic;
	rc = MqttClient_Subscribe(&client, &sub);
	if (rc != MQTT_CODE_SUCCESS) {
		return rc;
	}

	printk("uplink: subscribed to '%s' on our own broker\n",
	       CONFIG_APP_UPLINK_TOPIC_FILTER);
	return MQTT_CODE_SUCCESS;
}

static void uplink_thread(void *p1, void *p2, void *p3)
{
	int64_t next_ping;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* The broker (main.c, src/broker_net.c) runs on main's thread.
	 * It may not be listening in the first instant - retry the
	 * connect rather than trying to synchronize on it directly.
	 */
	while (uplink_connect_once() != MQTT_CODE_SUCCESS) {
		printk("uplink: broker not ready yet, retrying\n");
		k_sleep(K_SECONDS(2));
	}

	next_ping = k_uptime_get() + (KEEP_ALIVE_SEC / 2) * MSEC_PER_SEC;

	while (true) {
		int rc = MqttClient_WaitMessage(&client, 1000);

		if (rc != MQTT_CODE_SUCCESS && rc != MQTT_CODE_ERROR_TIMEOUT) {
			printk("uplink: connection lost (%d), reconnecting\n", rc);
			MqttClient_NetDisconnect(&client);
			while (uplink_connect_once() != MQTT_CODE_SUCCESS) {
				k_sleep(K_SECONDS(2));
			}
			next_ping = k_uptime_get() + (KEEP_ALIVE_SEC / 2) * MSEC_PER_SEC;
			continue;
		}

		if (k_uptime_get() >= next_ping) {
			MqttClient_Ping(&client);
			next_ping = k_uptime_get() + (KEEP_ALIVE_SEC / 2) * MSEC_PER_SEC;
		}
	}
}

K_THREAD_DEFINE(uplink_tid, 4096, uplink_thread, NULL, NULL, NULL, 7, 0, 0);
