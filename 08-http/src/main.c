#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(main);

/* Set these to what you're given in class - see README.md */
#define SERVER_HOST "192.0.2.1"
#define SERVER_PORT 5000

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define BRIGHTNESS 0x20
#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }
#define POLL_INTERVAL K_SECONDS(3)

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[STRIP_NUM_PIXELS];

struct color_map {
	const char *name;
	struct led_rgb rgb;
};

static const struct color_map colors[] = {
	{ "WHITE",  RGB(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS) },
	{ "RED",    RGB(BRIGHTNESS, 0x00, 0x00) },
	{ "GREEN",  RGB(0x00, BRIGHTNESS, 0x00) },
	{ "BLUE",   RGB(0x00, 0x00, BRIGHTNESS) },
	{ "YELLOW", RGB(BRIGHTNESS, BRIGHTNESS, 0x00) },
	{ "PURPLE", RGB(BRIGHTNESS, 0x00, BRIGHTNESS) },
};

static void set_strip_color(const char *name, size_t len)
{
	for (size_t i = 0; i < ARRAY_SIZE(colors); i++) {
		if (strlen(colors[i].name) == len &&
		    strncmp(colors[i].name, name, len) == 0) {
			for (size_t p = 0; p < STRIP_NUM_PIXELS; p++) {
				pixels[p] = colors[i].rgb;
			}
			led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
			LOG_INF("LED strip set to %.*s", (int)len, name);
			return;
		}
	}
	LOG_WRN("Unknown color from server: %.*s", (int)len, name);
}

static int connect_to_server(void)
{
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0) {
		LOG_ERR("socket() failed: %d", errno);
		return -1;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
	};

	inet_pton(AF_INET, SERVER_HOST, &addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("connect() to %s:%d failed: %d", SERVER_HOST, SERVER_PORT, errno);
		close(sock);
		return -1;
	}

	return sock;
}

/* Filled in by response_cb() while a request is in flight - only one
 * request is ever outstanding at a time in this example, so a single
 * shared buffer is enough.
 */
static uint8_t recv_buf[512];
static char color_buf[16];
static size_t color_len;

static int response_cb(struct http_response *rsp, enum http_final_call final_data,
		       void *user_data)
{
	if (rsp->body_frag_start != NULL && rsp->body_frag_len > 0) {
		color_len = MIN(rsp->body_frag_len, sizeof(color_buf) - 1);
		memcpy(color_buf, rsp->body_frag_start, color_len);
	}

	return 0;
}

static void poll_color_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(poll_color_work, poll_color_work_handler);

static void poll_color_work_handler(struct k_work *work)
{
	int sock = connect_to_server();

	if (sock >= 0) {
		struct http_request req = {
			.method = HTTP_GET,
			.url = "/color",
			.host = SERVER_HOST,
			.protocol = "HTTP/1.1",
			.response = response_cb,
			.recv_buf = recv_buf,
			.recv_buf_len = sizeof(recv_buf),
		};

		color_len = 0;
		if (http_client_req(sock, &req, 3000, NULL) >= 0 && color_len > 0) {
			set_strip_color(color_buf, color_len);
		}
		close(sock);
	}

	/* Keep polling, connected or not - the server (or the network)
	 * might come back.
	 */
	k_work_schedule(&poll_color_work, POLL_INTERVAL);
}

static void request_change_work_handler(struct k_work *work)
{
	int sock = connect_to_server();

	if (sock < 0) {
		return;
	}

	static const char payload[] = "change";
	struct http_request req = {
		.method = HTTP_POST,
		.url = "/color",
		.host = SERVER_HOST,
		.protocol = "HTTP/1.1",
		.payload = payload,
		.payload_len = sizeof(payload) - 1,
		.response = response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
	};

	color_len = 0;
	if (http_client_req(sock, &req, 3000, NULL) >= 0 && color_len > 0) {
		LOG_INF("Server picked a new color");
		set_strip_color(color_buf, color_len);
	}
	close(sock);
}

static K_WORK_DEFINE(request_change_work, request_change_work_handler);

static void button_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY || evt->value == 0) {
		return; /* react to press only, not release */
	}

	/* Hand off to the system workqueue - never do blocking socket
	 * I/O directly from an input callback.
	 */
	k_work_submit(&request_change_work);
}

INPUT_CALLBACK_DEFINE(NULL, button_input_cb, NULL);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void event_handler(struct net_mgmt_event_callback *cb,
			  uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		LOG_INF("Wi-Fi connected");
	}
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		LOG_INF("Got an IPv4 address - polling the color server");
		k_work_schedule(&poll_color_work, K_NO_WAIT);
	}
}

int main(void)
{
	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return 0;
	}

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
	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);

	if (ret) {
		LOG_ERR("No stored credential, or connect failed: %d", ret);
		LOG_INF("Provision one first, over the shell:");
		LOG_INF("  wifi cred add -s \"<SSID>\" -k 1 -p \"<password>\"");
	}

	return 0;
}
