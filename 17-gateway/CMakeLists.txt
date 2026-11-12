# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unlike every other example in this repo, this one links wolfMQTT
# (GPL-3.0-or-later) to get an MQTT broker - see README.md for why that
# makes this specific subdirectory GPL, not Apache-2.0 like the rest.

cmake_minimum_required(VERSION 3.20.0)

# wolfMQTT isn't a west-managed module here (this workspace's west
# manifest is shared with every other example and zephyr4squirrels) -
# fetch it locally instead; see README.md. Same "manual copy" escape
# hatch wolfMQTT's own Zephyr module docs describe for workspaces where
# west doesn't provide it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/wolfmqtt)
  list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/deps/wolfmqtt)
endif()

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/m5stack_fire_esp32_procpu.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gateway)

zephyr_compile_definitions(WOLFMQTT_USER_SETTINGS)

target_sources(app PRIVATE
	src/main.c
	src/wifi.c
	src/broker_net.c
	src/uplink.c
	src/client_net.c
)
target_include_directories(app PRIVATE src)
