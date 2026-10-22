#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void count_ssid(void *cb_arg, const char *ssid, size_t ssid_len)
{
	size_t *count = cb_arg;

	(*count)++;
}

static void print_wifi_status(struct net_if *iface)
{
	struct wifi_iface_status status = { 0 };

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))) {
		LOG_ERR("Status request failed");
		return;
	}

	LOG_INF("Wi-Fi state: %d, SSID: %.*s", status.state, status.ssid_len, status.ssid);
}

static void event_handler(struct net_mgmt_event_callback *cb,
			  uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		LOG_INF("Wi-Fi connected");
	}
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		LOG_INF("Got an IPv4 address");
		print_wifi_status(iface);
	}
}

int main(void)
{
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

	/* NET_REQUEST_WIFI_CONNECT_STORED always returns 0 when there is
	 * nothing stored to connect to - it just loops over zero saved
	 * credentials and does nothing (the same is true of the shell's
	 * own `wifi cred auto_connect`). So check the credential count
	 * ourselves first instead of trusting that return value.
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
