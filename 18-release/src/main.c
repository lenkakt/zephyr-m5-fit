/* Copyright (c) 2026 Lenka Koskova Triskova
 *
 * SPDX-License-Identifier: Apache-2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define SERIAL_KEY "device/serial"

struct serial_result {
	char buf[32];
	bool found;
};

/* Mirrors settings_shell.c's own cmd_read callback: accept only an
 * exact match for SERIAL_KEY, not a descendant key.
 */
static int serial_load_cb(const char *key, size_t len, settings_read_cb read_cb,
			   void *cb_arg, void *param)
{
	struct serial_result *result = param;
	ssize_t n;

	if (settings_name_next(key, NULL) != 0) {
		return 0;
	}

	n = read_cb(cb_arg, result->buf, MIN(len, sizeof(result->buf) - 1));
	if (n > 0) {
		result->buf[n] = '\0';
		result->found = true;
	}

	return 0;
}

int main(void)
{
	uint8_t id[16];
	ssize_t id_len;
	struct serial_result serial = { .found = false };

	/* Fixed in silicon at manufacturing - readable, never assignable. */
	id_len = hwinfo_get_device_id(id, sizeof(id));
	if (id_len > 0) {
		LOG_HEXDUMP_INF(id, id_len,
				"silicon device ID (fixed in hardware, not assignable)");
	} else {
		LOG_WRN("hwinfo_get_device_id failed: %d", (int)id_len);
	}

	/* Assigned once, separately from flashing this firmware - over
	 * the shell, on the line that completes the device.
	 */
	settings_subsys_init();
	settings_load_subtree_direct(SERIAL_KEY, serial_load_cb, &serial);

	if (serial.found) {
		LOG_INF("assigned serial number: %s", serial.buf);
	} else {
		LOG_WRN("no serial number provisioned yet - over the shell, run:");
		LOG_WRN("  settings write string %s \"SN-00042\"", SERIAL_KEY);
		LOG_WRN("then reset the board");
	}

	return 0;
}
