/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/logging/log.h>

#include "wifi.h"

LOG_MODULE_REGISTER(wifi_gw);

int wifi_ap_enable(void)
{
	struct net_if *ap_iface = net_if_get_wifi_sap();
	struct wifi_connect_req_params ap_config = {0};
	struct net_in_addr addr, netmask;
	int rc;

	if (ap_iface == NULL) {
		LOG_ERR("No Wi-Fi AP interface (check the devicetree overlay)");
		return -ENODEV;
	}

	ap_config.ssid = (const uint8_t *)CONFIG_APP_AP_SSID;
	ap_config.ssid_length = strlen(CONFIG_APP_AP_SSID);
	ap_config.psk = (const uint8_t *)CONFIG_APP_AP_PSK;
	ap_config.psk_length = strlen(CONFIG_APP_AP_PSK);
	ap_config.security = strlen(CONFIG_APP_AP_PSK) > 0 ? WIFI_SECURITY_TYPE_PSK
							    : WIFI_SECURITY_TYPE_NONE;
	ap_config.channel = WIFI_CHANNEL_ANY;
	ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	/* Static IP + DHCP pool for the sensor node joining the AP, same
	 * approach as Zephyr's samples/net/wifi/apsta_mode.
	 */
	net_addr_pton(NET_AF_INET, CONFIG_APP_AP_IP_ADDRESS, &addr);
	net_addr_pton(NET_AF_INET, "255.255.255.0", &netmask);
	net_if_ipv4_set_gw(ap_iface, &addr);
	net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmask);

	addr.s4_addr[3] += 10; /* pool starts 10 above the AP's own address */
	rc = net_dhcpv4_server_start(ap_iface, &addr);
	if (rc != 0) {
		LOG_ERR("DHCPv4 server start failed: %d", rc);
		return rc;
	}

	rc = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &ap_config, sizeof(ap_config));
	if (rc != 0) {
		LOG_ERR("AP enable failed: %d", rc);
		return rc;
	}

	LOG_INF("AP up: %s on %s", CONFIG_APP_AP_SSID, CONFIG_APP_AP_IP_ADDRESS);
	return 0;
}

static void count_ssid(void *cb_arg, const char *ssid, size_t ssid_len)
{
	size_t *count = cb_arg;

	(*count)++;
}

int wifi_sta_connect(void)
{
	struct net_if *sta_iface = net_if_get_wifi_sta();
	size_t cred_count = 0;

	if (sta_iface == NULL) {
		LOG_ERR("No Wi-Fi STA interface found");
		return -ENODEV;
	}

	wifi_credentials_for_each_ssid(count_ssid, &cred_count);
	if (cred_count == 0) {
		LOG_WRN("No stored uplink Wi-Fi credential - the AP/broker side still "
			"works without it. Provision one over the shell when you want "
			"the uplink too:");
		LOG_INF("  wifi cred add -s \"<SSID>\" -k 1 -p \"<password>\"");
		return 0;
	}

	int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, sta_iface, NULL, 0);

	if (rc != 0) {
		LOG_ERR("Uplink connect failed: %d", rc);
	}
	return rc;
}
