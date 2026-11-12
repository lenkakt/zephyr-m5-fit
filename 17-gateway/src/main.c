/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Template for the final-assignment gateway: hosts the sensor node's
 * Wi-Fi network (AP), joins an internet-connected one (STA), and is
 * both an MQTT broker (for the sensor) and an MQTT client (its own
 * uplink thread, src/uplink.c) at once. See README.md for the full
 * picture and what's deliberately left out of this template.
 */
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>

#include "broker_net.h"
#include "wifi.h"
#include "uplink.h"

LOG_MODULE_REGISTER(main);

static MqttBroker broker;
static MqttBrokerNet net;

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			   struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		LOG_INF("Uplink Wi-Fi connected");
	}
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		LOG_INF("Uplink got an IPv4 address");
	}
}

int main(void)
{
	int rc;

	LOG_INF("MqttBroker uses %zu bytes of RAM", sizeof(broker));

	net_mgmt_init_event_callback(&wifi_cb, event_handler, NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);
	net_mgmt_init_event_callback(&ipv4_cb, event_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	/* AP first - required for the ESP32 driver to settle into AP+STA
	 * combo mode rather than switching between the two - and it's
	 * unconditional: the sensor-facing network and the broker on it
	 * must come up with no dependency on the uplink even existing.
	 * That's the whole point of this design - see README.md.
	 */
	rc = wifi_ap_enable();
	if (rc != 0) {
		return rc;
	}

	/* Best-effort and non-blocking - connects in the background if a
	 * credential is stored, logs and moves on if not. Either way,
	 * nothing below this point depends on it.
	 */
	wifi_sta_connect();

	/* uplink_thread (src/uplink.c) is already running by now via its
	 * own K_THREAD_DEFINE, independently of everything here.
	 */

	broker_net_init(&net);

	rc = MqttBroker_Init(&broker, &net);
	if (rc != MQTT_CODE_SUCCESS) {
		LOG_ERR("Broker init failed: %d", rc);
		return rc;
	}

	rc = MqttBroker_Run(&broker);
	LOG_INF("Broker stopped: %d", rc);
	return rc;
}
