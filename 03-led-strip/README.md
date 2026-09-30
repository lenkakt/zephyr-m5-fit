# 03-led-strip

Lights up the M5Stack Fire's onboard RGB LED strip (10× SK6812/WS2812-
compatible pixels), cycling red → green → blue → white every second.

Zephyr's mainline `m5stack_fire` board file doesn't define this LED strip
at all — it's not in the upstream devicetree. 

So [`common/boards/m5stack_fire_esp32_procpu.overlay`](../common/boards/m5stack_fire_esp32_procpu.overlay)
drives it over SPI — the same approach Zephyr's own
`adafruit_feather_esp32` board (same ESP32 chip) uses for its onboard
WS2812. It lives in `common/` (pointed at explicitly via `DTC_OVERLAY_FILE`
in this example's `CMakeLists.txt`, rather than the usual auto-discovered
`boards/<board>.overlay` in the app directory) because `04-led-shell` needs
the exact same overlay — one file instead of two identical copies:

- The strip's single data line is on **GPIO15** (as described in M5Stack's own
  documentation/community examples).
- **SPI2** is free on this board (SPI3 is already used for the SD card),
  so its MOSI line is routed to GPIO15 via the ESP32 GPIO matrix. SCLK/MISO
  are left unconnected — WS2812 only needs the one data line, SPI is just
  used as a convenient way to generate its bit timing.


## How the code works (for absolute Zephyr beginners)

Take a look at [`src/main.c`](src/main.c). A few things look different from
"plain" C, because Zephyr has its own way of letting a program discover
what hardware it actually has available.

### Devicetree in two sentences

**Devicetree** is a text description of hardware (see the overlay file
above) — which peripheral lives where, on which pin, with what settings.
Zephyr processes it **at compile time**, not at runtime — so "discovering"
hardware costs nothing while the program is actually running; it's just
text that gets turned into numbers before the program ever starts.

### Finding the right devicetree node

The overlay file defines an alias, a short, generic name pointing at our
specific `ws2812@0` node on SPI2:

```dts
aliases {
	led-strip = &led_strip;
};
```

Why an alias instead of referencing the node directly from the code? Because
the name `led-strip` is generic — if the same app ran on a different board
with the LED strip wired completely differently (different SPI, different
GPIO), you'd only need to rewrite the overlay; `main.c` wouldn't need to
change at all. The code asks for "whatever this board calls its LED
strip," not for a specific SPI peripheral.

*Programmer's detail:* `DT_ALIAS(led_strip)` is the macro that resolves
this, entirely at compile time:

```c
#define STRIP_NODE DT_ALIAS(led_strip)
```

### What a "device" actually is in Zephyr

Devicetree macros (`DT_...`) only run at compile time — they just tell you
*what* is connected where, as plain numbers baked into the binary. To
actually *do* something with a peripheral while the program is running,
you need a runtime handle to it. In Zephyr, every driver instance — an LED
strip, a UART, a sensor, anything — is represented at runtime by a
`struct device`.

*Programmer's detail:* `DEVICE_DT_GET(STRIP_NODE)` is the bridge between
the two: it takes the devicetree node (resolved at compile time) and
produces a pointer to the matching `struct device`, which you then use at
runtime to call that driver's functions — in our case,
`led_strip_update_rgb()` further down.

```c
static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
```

### Don't touch the hardware until it's ready

Drivers initialize during system startup, and not all of them finish at
the same instant (some wait on another bus, for example). So before using
any device, you check that it's actually ready — if it isn't (e.g. because
of a devicetree mistake), you get a clear error instead of the program
crashing or behaving strangely.

*Programmer's detail:* this is `device_is_ready()`, a standard Zephyr
idiom that belongs before any other call on a device:

```c
if (!device_is_ready(strip)) { ... }
```

### Reading values straight out of the devicetree

The overlay says this strip has 10 pixels (`chain-length = <10>;`). Rather
than hardcoding `10` again in `main.c`, the code reads that number
directly from the devicetree — so if you ever wired up a strip with a
different pixel count, changing one number in the overlay is enough;
`main.c` adapts automatically, with no code change needed.

*Programmer's detail:* `DT_PROP` reads a devicetree property, at compile
time, into a plain constant:

```c
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
```

This sizes the `pixels[STRIP_NUM_PIXELS]` array correctly, automatically.

### The actual lighting-up

Everything above was about finding and preparing the device. Making it
light up is a single, plain runtime function call: it sends the array of
colors (one `struct led_rgb` per pixel) to the SPI driver, which encodes
it into the exact timing the WS2812/SK6812 chip expects — that's
`led_strip_update_rgb()`, called from the main loop.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -p always -b m5stack_fire/esp32/procpu -d build 03-led-strip
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Watch the strip on the underside of the device — it should cycle red,
green, blue, white, one second each, all 10 pixels at once.

The shell is enabled here too, same as in `01-hello`. At the `uart:~$`
prompt, `device list` should show a `ws2812@0` entry (on `spi@3ff64000`,
i.e. SPI2) as `READY` — confirming Zephyr actually found and initialized
the strip, independent of whether it visibly lights up correctly.
