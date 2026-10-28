# 11-light

Reads an M5Stack "Light" analog ambient-light sensor (a photoresistor)
plugged into Grove Port B, and logs the reading to the shell about once
every 2 seconds. Same template as [`09-gyroscope`](../09-gyroscope) and
[`10-humidity`](../10-humidity): a dedicated sampling thread publishes
onto a zbus channel, a subscriber with its own thread logs it, and you
extend by adding your own subscriber rather than touching the sampling
thread.

## Wiring

Plug the Light sensor's Grove cable into **Port B** (the middle Grove
port on the M5Stack Fire's bottom edge, labelled `B`) before flashing.

This uses the exact same physical pin as `10-humidity`'s probe (Port
B's D0 line) - only one of the two can be plugged in at a time.

## The hardware

A photoresistor is not a smart sensor chip - there is no Zephyr sensor
driver for it, so (same as `10-humidity`) this reads the ADC directly
(`CONFIG_ADC`, not `CONFIG_SENSOR`). Port B's D0 pin is GPIO36, which is
ADC1 **channel 0** - confirmed against M5Stack's own docs and ESP-IDF's
`adc_channel.h` (`ADC1_CHANNEL_0_GPIO_NUM` `36`). See
[`boards/m5stack_fire_esp32_procpu.overlay`](boards/m5stack_fire_esp32_procpu.overlay)
for the devicetree setup.

There is deliberately no lux conversion anywhere in this example. A bare
photoresistor's raw ADC count doesn't map to a physical light-level unit
without calibrating against a real light meter, which nobody has done
for this specific probe - so `src/main.c` reports the raw count and
leaves deciding what "bright" or "dark" means for your own project up
to you.

### Why this samples only once every 2 seconds

The ESP32 ADC driver re-applies its hardware calibration code on
**every single read** (`adc_set_hw_calibration_code()`, called
unconditionally from `adc_esp32_read()` in `drivers/adc/adc_esp32.c`),
and that is audible on this board's onboard speaker - confirmed on real
hardware to scale with how often you call `adc_read_dt()`, not just
happen once at boot. Sampling a few times a second is audible as
constant clicking; reading once every couple of seconds is not. If you
need faster sampling for your own project, expect it to get noisier -
there is no Kconfig or driver option to disable the per-read
calibration call.

## The design

Same template, same reasoning, as `09-gyroscope`'s README — a dedicated
sampling thread so this sensor's sample rate is never affected by
anything else the application does, and a zbus **subscriber** (own
thread, own message queue) rather than a listener, so a slow or
blocking reaction can never stall sampling.

## Extending this

Add your own logic as another subscriber of `light_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to
touch `light_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 11-light
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell and watch the log output — a `light: raw=NNNN` line
every 2 seconds. Cover the sensor or shine a light directly on it and
the number should move accordingly.
