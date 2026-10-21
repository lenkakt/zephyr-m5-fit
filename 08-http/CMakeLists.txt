cmake_minimum_required(VERSION 3.20.0)

# Reuses the LED strip overlay from 03-led-strip/04-led-shell - see
# common/boards/ for why: same onboard LED strip, same devicetree fixes.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../common/boards/m5stack_fire_esp32_procpu.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(http_color)

target_sources(app PRIVATE src/main.c)
