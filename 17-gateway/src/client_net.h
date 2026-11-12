/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include "wolfmqtt/mqtt_client.h"

/* Zephyr/zsock glue for wolfMQTT's *client* API (MqttNet) - the
 * counterpart to broker_net.c, which does the same for the broker's
 * own MqttBrokerNet. Two different callback structs in the library,
 * so two small glue files, same socket primitives underneath.
 */
void client_net_init(MqttNet *net);

#endif
