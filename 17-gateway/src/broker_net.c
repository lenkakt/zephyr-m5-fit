/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Zephyr/zsock glue for wolfMQTT's MqttBrokerNet callbacks - the
 * broker library calls these, it never touches sockets directly.
 */
#include <errno.h>
#include <string.h>
#include <zephyr/net/socket.h>

#include "broker_net.h"

static int net_listen(void *ctx, BROKER_SOCKET_T *sock, word16 port, int backlog)
{
	struct net_sockaddr_in addr = {0};
	int opt = 1;
	int fd = zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);

	ARG_UNUSED(ctx);

	if (fd < 0) {
		return MQTT_CODE_ERROR_NETWORK;
	}

	zsock_setsockopt(fd, ZSOCK_SOL_SOCKET, ZSOCK_SO_REUSEADDR, &opt, sizeof(opt));

	addr.sin_family = NET_AF_INET;
	addr.sin_port = net_htons(port);

	if (zsock_bind(fd, (struct net_sockaddr *)&addr, sizeof(addr)) < 0 ||
	    zsock_listen(fd, backlog) < 0) {
		zsock_close(fd);
		return MQTT_CODE_ERROR_NETWORK;
	}

	*sock = fd;
	return MQTT_CODE_SUCCESS;
}

static int wait_for(int fd, short events, int timeout_ms)
{
	struct zsock_pollfd pfd = {.fd = fd, .events = events};
	int rc = zsock_poll(&pfd, 1, timeout_ms);

	if (rc == 0) {
		return MQTT_CODE_ERROR_TIMEOUT;
	}
	return rc < 0 ? MQTT_CODE_ERROR_NETWORK : MQTT_CODE_SUCCESS;
}

static int net_accept(void *ctx, BROKER_SOCKET_T listen_sock, BROKER_SOCKET_T *client_sock)
{
	int fd;

	ARG_UNUSED(ctx);

	if (wait_for(listen_sock, ZSOCK_POLLIN, 5) != MQTT_CODE_SUCCESS) {
		return MQTT_CODE_CONTINUE;
	}

	fd = zsock_accept(listen_sock, NULL, NULL);
	if (fd < 0) {
		return MQTT_CODE_ERROR_NETWORK;
	}

	*client_sock = fd;
	return MQTT_CODE_SUCCESS;
}

static int transfer_result(int rc)
{
	if (rc > 0) {
		return rc;
	}
	if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return MQTT_CODE_CONTINUE;
	}
	return MQTT_CODE_ERROR_NETWORK;
}

static int net_read(void *ctx, BROKER_SOCKET_T sock, byte *buf, int buf_len, int timeout_ms)
{
	int rc;

	ARG_UNUSED(ctx);

	if (buf == NULL || buf_len <= 0) {
		return MQTT_CODE_ERROR_BAD_ARG;
	}
	if (timeout_ms > 0) {
		rc = wait_for(sock, ZSOCK_POLLIN, timeout_ms);
		if (rc != MQTT_CODE_SUCCESS) {
			return rc;
		}
	}
	return transfer_result(zsock_recv(sock, buf, buf_len, ZSOCK_MSG_DONTWAIT));
}

static int net_write(void *ctx, BROKER_SOCKET_T sock, const byte *buf, int buf_len, int timeout_ms)
{
	int rc;

	ARG_UNUSED(ctx);

	if (buf == NULL || buf_len <= 0) {
		return MQTT_CODE_ERROR_BAD_ARG;
	}
	if (timeout_ms > 0) {
		rc = wait_for(sock, ZSOCK_POLLOUT, timeout_ms);
		if (rc != MQTT_CODE_SUCCESS) {
			return rc;
		}
	}
	return transfer_result(zsock_send(sock, buf, buf_len, ZSOCK_MSG_DONTWAIT));
}

static int net_close(void *ctx, BROKER_SOCKET_T sock)
{
	ARG_UNUSED(ctx);

	if (sock != BROKER_SOCKET_INVALID) {
		zsock_close(sock);
	}
	return MQTT_CODE_SUCCESS;
}

void broker_net_init(MqttBrokerNet *net)
{
	memset(net, 0, sizeof(*net));
	net->listen = net_listen;
	net->accept = net_accept;
	net->read = net_read;
	net->write = net_write;
	net->close = net_close;
}
