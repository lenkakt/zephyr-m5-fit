/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

#include <zephyr/kernel.h>

#define NO_MAIN_DRIVER
#define WOLFMQTT_BROKER
#define WOLFMQTT_BROKER_CUSTOM_NET
#define WOLFMQTT_STATIC_MEMORY
#define WOLFMQTT_V5

#define WOLFMQTT_BROKER_GET_TIME_S() ((unsigned long)(k_uptime_get() / 1000))
#define BROKER_SLEEP_MS(ms) k_msleep(ms)

/* Small on purpose - a template for a classroom demo, not a
 * production broker. One sensor node plus this app's own loopback
 * uplink client is the expected load.
 */
#define BROKER_MAX_CLIENTS 4
#define BROKER_MAX_SUBS 8
#define BROKER_RX_BUF_SZ 512
#define BROKER_TX_BUF_SZ 512
#define BROKER_MAX_RETAINED 4
#define BROKER_MAX_PAYLOAD_LEN 256
#define BROKER_MAX_CLIENT_ID_LEN 32
#define BROKER_MAX_USERNAME_LEN 32
#define BROKER_MAX_PASSWORD_LEN 32
#define BROKER_MAX_FILTER_LEN 64
#define BROKER_MAX_TOPIC_LEN 64
#define BROKER_MAX_WILL_PAYLOAD_LEN 128
#define BROKER_MAX_STATIC_OFFLINE_MSGS_PER_SUB 4
#define BROKER_MAX_STATIC_OFFLINE_DATA_LEN 128

#endif
