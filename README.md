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
| [07-wifi](07-wifi) | Connects to Wi-Fi using a credential provisioned once over the shell (`wifi cred add`) — never hardcoded in source — and why Zephyr's `net_mgmt` events, not a blocking connect call, are how you find out the result. |
| [08-http](08-http) | Polls a small HTTP color server and drives the LED strip from the response, with a button press triggering a `POST` — ties `07-wifi` together with `02-buttons` and `03-led-strip`, plus Zephyr's HTTP client API. |
| [09-gyroscope](09-gyroscope) | Reads the onboard MPU6886 gyroscope from a dedicated sampling thread and publishes each reading on a zbus channel, with a subscriber that logs it — the template for every sensor/actuator example to follow. |
| [10-humidity](10-humidity) | Same template as `09-gyroscope`, applied to an M5Stack "Unit Earth" analog soil/humidity probe on Grove Port B — read via raw ADC/GPIO rather than a sensor driver, since none exists for this probe. |
| [11-light](11-light) | Same template again, applied to an M5Stack "Light" analog photoresistor — also on Grove Port B (same pin as `10-humidity`, so only one of the two can be plugged in at a time). Reports raw ADC counts, no invented lux formula. |
| [12-pir-motion](12-pir-motion) | Same template once more, applied to an M5Stack PIR Motion Unit — plain digital GPIO on Grove Port B, no ADC involved. Publishes only on a state change, not every poll, since motion is event-shaped rather than continuously varying. |
| [13-angle](13-angle) | Same template again, applied to an M5Stack Angle Unit (a potentiometer) — back on Grove Port B's ADC pin (same as `10-humidity`/`11-light`). Reports raw counts plus a left/right/none turn direction, no invented degrees formula. |

## Which examples can be combined

| Grove Port B pin | Sensor examples there |
|---|---|
| D0 — GPIO36 (analog/digital) | `10-humidity` (analog reading), `11-light`, `12-pir-motion`, `13-angle` |
| D1 — GPIO26 (digital) | `10-humidity` (wet/dry threshold signal) |

Only one Grove cable fits Port B at a time, so `10-humidity`,
`11-light`, `12-pir-motion`, and `13-angle` are mutually exclusive —
whichever of the four is physically plugged in is the only one that
will read anything meaningful, regardless of which one you flash.

`09-gyroscope` doesn't use Port B at all (onboard I2C) and can run
alongside any one of the four. Everything from `01-hello` through
`08-http` doesn't touch Port B either. If you want two Port-B-style
sensors active simultaneously in your own project, you'd need to wire
one of them to a different port/pin yourself — none of these examples
do that for you.

[`experimental-lvgl`](experimental-lvgl) also exists — an LVGL-based demo
with button-switched modes — but it's parked: real hardware testing hit a
reproducible crash-reboot loop that hasn't been root-caused yet (see its
own README). Not part of the main numbered sequence until that's
resolved.

## Known issue: using the ADC makes a sound

Yes — on real hardware, reading the ADC makes the onboard speaker
audibly click, and it's hard to get rid of for now. The ESP32 ADC
driver re-applies a hardware calibration code on every single read
(`adc_set_hw_calibration_code()`, unconditional, in
`drivers/adc/adc_esp32.c`), and that's what couples into the speaker —
not something any example here does wrong, and there's no Kconfig or
driver option to turn it off.

It scales with how often you call `adc_read_dt()`: sampling a few times
a second is audible as constant clicking, sampling once every couple of
seconds is just an occasional faint click. `10-humidity` and `11-light`
both sample at a 2-second interval specifically for this reason — if you
write your own ADC-based example, keep your read rate low for the same
reason. `solutions/04-mic` is the exception: it needs 500Hz sampling to
detect loudness at all, so slowing it down isn't an option there, and
its noise is an open, unresolved limitation, documented in its own
README rather than fixed.

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
