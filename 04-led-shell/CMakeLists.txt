cmake_minimum_required(VERSION 3.20.0)

# Shared with 03-led-strip - see common/boards/ for why: same onboard
# LED strip, same devicetree fixes, one overlay for both.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../common/boards/m5stack_fire_esp32_procpu.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_shell)

target_sources(app PRIVATE src/main.c)
