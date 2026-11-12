/* Copyright (c) 2026 Lenka Koskova Triskova
 *
 * SPDX-License-Identifier: Apache-2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
	LOG_INF("I am device B");
	return 0;
}
