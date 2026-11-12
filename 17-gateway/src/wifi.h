/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef WIFI_H
#define WIFI_H

/* Enables the AP (sensor-side network) unconditionally - doesn't wait
 * on anything, doesn't need a stored credential. See README.md "Why
 * the AP side doesn't wait for anything".
 */
int wifi_ap_enable(void);

/* Starts the STA (uplink) connect using whatever credential was
 * provisioned over the shell with `wifi cred add` - same as
 * 07-wifi/08-http/15-mqtt. Non-blocking: the result arrives later via
 * net_mgmt events (see main.c), same as those examples.
 */
int wifi_sta_connect(void);

#endif
