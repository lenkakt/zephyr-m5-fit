# 10-humidity

Reads an M5Stack "Unit Earth" analog soil/humidity probe plugged into
Grove Port B, and logs the reading to the shell every 2 seconds. Same
template as [`09-gyroscope`](../09-gyroscope): a dedicated sampling
thread publishes onto a zbus channel, a subscriber with its own thread
logs it, and you extend by adding your own subscriber rather than
touching the sampling thread.

## Wiring

Plug the Unit Earth probe's Grove cable into **Port B** (the middle
Grove port on the M5Stack Fire's bottom edge, labelled `B`) before
flashing. It will not read anything meaningful plugged into Port A or C
— see "The hardware" below for why.

## The hardware

Unlike the onboard MPU6886 gyroscope, this probe is not a smart sensor
chip with its own Zephyr driver — it's a resistive probe wired to a
small breakout board with a comparator, giving you two signals:

- an **analog** voltage proportional to moisture, read via the ESP32's
  ADC (ADC1 channel 0 — Port B's D0 pin, GPIO36),
- a **digital** wet/dry threshold output, read as a plain GPIO
  (Port B's D1 pin, `&gpio0 26`).

Because there's no sensor driver for it, this example reads the ADC and
GPIO directly (`CONFIG_ADC`, not `CONFIG_SENSOR`) — see
[`boards/m5stack_fire_esp32_procpu.overlay`](boards/m5stack_fire_esp32_procpu.overlay)
for both signals' devicetree setup, and its comments for the gotchas
this hit along the way:

- Port B's D0 pin is GPIO36, which is ADC1 **channel 0** — confirmed
  against M5Stack's own docs and ESP-IDF's `adc_channel.h`
  (`ADC1_CHANNEL_0_GPIO_NUM` `36`). Reading the wrong channel here reads
  an unrelated, effectively floating pin, and reports a constant ~100%
  regardless of the probe;
- the ESP32 GPIO controller takes a **single** flags cell
  (`#gpio-cells = <2>`: pin, flags) — `GPIO_ACTIVE_LOW` and
  `GPIO_PULL_DOWN` must be OR'd into one cell, not listed as two;
- a bare devicetree node with no `compatible` does not get its `gpios`
  property recognized as a phandle array at all, so `GPIO_DT_SPEC_GET()`
  fails to compile — wrapping it in a `"gpio-keys"` node (the binding
  whose child-binding declares `gpios` as a phandle-array) fixes that,
  even though this example never touches the input subsystem the
  `"gpio-keys"` compatible normally implies.

The ADC-to-percent conversion (`ADC_MIN_VALUE`/`ADC_MAX_VALUE` in
`src/main.c`) needs calibrating against your own probe: the log output
includes the raw ADC count alongside the computed percentage specifically
so you can read off real numbers in open air and in wet soil, and set
`ADC_MIN_VALUE`/`ADC_MAX_VALUE` to match.

The ESP32 ADC driver also re-applies its hardware calibration code on
**every single read** (`adc_set_hw_calibration_code()`, called
unconditionally from `adc_esp32_read()` in `drivers/adc/adc_esp32.c`),
which is audible as a faint click on this board's onboard speaker -
confirmed on real hardware to scale with how often you call
`adc_read_dt()`. This example already samples at a slow 2-second
interval specifically to keep that click infrequent rather than
constant; there is no Kconfig or driver option to disable the
calibration call itself.

## The design

Same template, same reasoning, as `09-gyroscope`'s README — a dedicated
sampling thread so this probe's sample rate is never affected by
anything else the application does, and a zbus **subscriber** (own
thread, own message queue) rather than a listener, so a slow or
blocking reaction can never stall sampling.

## Extending this

Add your own logic as another subscriber of `humidity_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to
touch `humidity_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 10-humidity
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell and watch the log output — a
`humidity: NN% (raw=NNNN, threshold LOW/HIGH)` line every 2 seconds.
Squeeze or wet the probe tip and the percentage should rise; dry air
should bring it back down.

If the percentage stays pinned at 0% or 100% regardless of the probe,
look at the `raw` value instead: if it never moves either, double-check
the probe is actually plugged into Grove Port B and that Port B's D0
pin really is reaching the ADC on your specific unit. If `raw` does
change but the percentage doesn't track it, `ADC_MIN_VALUE`/
`ADC_MAX_VALUE` need recalibrating to whatever range `raw` actually
covers on your hardware.
