/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UPLINK_H
#define UPLINK_H

/* No functions to call - uplink_thread starts on its own via
 * K_THREAD_DEFINE in uplink.c. This header exists so main.c doesn't
 * need to know anything about it beyond "it's compiled in".
 */

#endif
