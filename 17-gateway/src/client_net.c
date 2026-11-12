/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <string.h>
#include <zephyr/net/socket.h>

#include "client_net.h"

/* Always loopback (127.0.0.1) in this app - the gateway only ever uses
 * this to subscribe to its own, local broker - so no DNS/getaddrinfo
 * needed, just a fixed address.
 */

struct client_net_ctx {
	int fd;
};

static struct client_net_ctx ctx = {.fd = -1};

static int wait_for(int fd, short events, int timeout_ms)
{
	struct zsock_pollfd pfd = {.fd = fd, .events = events};
	int rc = zsock_poll(&pfd, 1, timeout_ms);

	if (rc == 0) {
		return MQTT_CODE_ERROR_TIMEOUT;
	}
	return rc < 0 ? MQTT_CODE_ERROR_NETWORK : MQTT_CODE_SUCCESS;
}

static int client_connect(void *context, const char *host, word16 port, int timeout_ms)
{
	struct client_net_ctx *c = context;
	struct net_sockaddr_in addr = {0};
	int fd = zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);

	ARG_UNUSED(timeout_ms);

	if (fd < 0) {
		return MQTT_CODE_ERROR_NETWORK;
	}

	addr.sin_family = NET_AF_INET;
	addr.sin_port = net_htons(port);
	if (zsock_inet_pton(NET_AF_INET, host, &addr.sin_addr) != 1) {
		zsock_close(fd);
		return MQTT_CODE_ERROR_NETWORK;
	}

	if (zsock_connect(fd, (struct net_sockaddr *)&addr, sizeof(addr)) < 0) {
		zsock_close(fd);
		return MQTT_CODE_ERROR_NETWORK;
	}

	c->fd = fd;
	return MQTT_CODE_SUCCESS;
}

static int client_read(void *context, byte *buf, int buf_len, int timeout_ms)
{
	struct client_net_ctx *c = context;
	int rc;

	if (timeout_ms > 0) {
		rc = wait_for(c->fd, ZSOCK_POLLIN, timeout_ms);
		if (rc != MQTT_CODE_SUCCESS) {
			return rc;
		}
	}

	rc = zsock_recv(c->fd, buf, buf_len, 0);
	if (rc == 0) {
		return MQTT_CODE_ERROR_NETWORK; /* broker closed the connection */
	}
	if (rc < 0) {
		return (errno == EAGAIN || errno == EWOULDBLOCK)
			       ? MQTT_CODE_ERROR_TIMEOUT
			       : MQTT_CODE_ERROR_NETWORK;
	}
	return rc;
}

static int client_write(void *context, const byte *buf, int buf_len, int timeout_ms)
{
	struct client_net_ctx *c = context;
	int rc;

	if (timeout_ms > 0) {
		rc = wait_for(c->fd, ZSOCK_POLLOUT, timeout_ms);
		if (rc != MQTT_CODE_SUCCESS) {
			return rc;
		}
	}

	rc = zsock_send(c->fd, buf, buf_len, 0);
	return rc < 0 ? MQTT_CODE_ERROR_NETWORK : rc;
}

static int client_disconnect(void *context)
{
	struct client_net_ctx *c = context;

	if (c->fd >= 0) {
		zsock_close(c->fd);
		c->fd = -1;
	}
	return MQTT_CODE_SUCCESS;
}

void client_net_init(MqttNet *net)
{
	memset(net, 0, sizeof(*net));
	net->context = &ctx;
	net->connect = client_connect;
	net->read = client_read;
	net->write = client_write;
	net->disconnect = client_disconnect;
}
