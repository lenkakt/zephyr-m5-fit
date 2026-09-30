# zephyr-m5-fit

Code examples for the BI-IOT/BIE-IOT tutorials, targeting the **real**
M5Stack Fire (ESP32) — the hands-on counterpart to
[zephyr4squirrels](https://github.com/lenkakt/zephyr4squirrels), which
covers the same Zephyr fundamentals but in QEMU, no hardware required.
Everything here is meant to be built and flashed to a physical board.

## How this fits together

This repo doesn't set up its own Zephyr workspace — it's a plain,
freestanding collection of apps that all point at whichever Zephyr
workspace you already have (`~/zephyrproject` on this machine), via the
`ZEPHYR_BASE` environment variable. That workspace is shared with
`zephyr4squirrels`; nothing here needs its own `west init`/`west update`.

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
```

Each numbered directory is a standalone Zephyr app (`CMakeLists.txt`,
`prj.conf`, `src/main.c`) with its own README covering build, flash, and
how to verify it actually worked.

[`common/`](common) holds code and a devicetree overlay shared by more
than one example — the LED strip overlay, the bitmap-font text renderer,
the 16-color palette — specifically so a later, combined application can
pull in more than one example's pieces at once instead of copy-pasting
between numbered directories. An example that needs something from here
points its `CMakeLists.txt` at it explicitly (`target_sources`,
`target_include_directories`, or `DTC_OVERLAY_FILE` for the overlay) —
see any of `03-led-strip`, `04-led-shell`, or `06-text`'s `CMakeLists.txt`
for the pattern.

## Examples

| Dir | What it shows |
|---|---|
| [01-hello](01-hello) | Minimal app: prints a message, enables the interactive shell. Start here. |
| [02-buttons](02-buttons) | Reacts to the four physical buttons via Zephyr's input subsystem — no devicetree overlay needed, the board already describes them. |
| [03-led-strip](03-led-strip) | Drives the M5Stack Fire's onboard 10-pixel RGB LED strip — and why that needs a devicetree overlay at all (Zephyr doesn't know about this peripheral out of the box). |
| [04-led-shell](04-led-shell) | Same LED strip, plus `led off`/`led on` shell commands — and why turning WS2812 LEDs off needs an explicit command, not just a different app. |
| [05-display](05-display) | Fills the built-in 320×240 color screen with cycling colors via Zephyr's `CONFIG_DISPLAY` driver API — and why the higher-level Character Framebuffer subsystem *doesn't* work here (it's mono-only). |
| [06-text](06-text) | Renders real text on the screen without LVGL — a hand-rolled bitmap-font renderer in `common/`, and why Zephyr's own CFB text subsystem can't drive this panel's color output either. |

[`experimental-lvgl`](experimental-lvgl) also exists — an LVGL-based demo
with button-switched modes — but it's parked: real hardware testing hit a
reproducible crash-reboot loop that hasn't been root-caused yet (see its
own README). Not part of the main numbered sequence until that's
resolved.

## Build, flash, verify — the general pattern

```sh
west build -b m5stack_fire/esp32/procpu -d build <example-dir>
west flash
```

If port autodetection fails: `west flash --esp-device /dev/ttyACM0` (or
`/dev/ttyUSB0` — check `dmesg` right after plugging the board in).

To verify a device is actually seen by Zephyr, open a serial console —

```sh
picocom -b 115200 /dev/ttyACM0
```

— and, on any example with the shell enabled, run `device list` at the
`uart:~$` prompt. Each example's own README has the specifics.
