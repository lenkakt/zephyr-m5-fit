# 13-angle

Reads an M5Stack Angle Unit (a potentiometer) plugged into Grove
Port B, and logs the raw reading plus a left/right/none turn direction
to the shell every 2 seconds. Same template as
[`09-gyroscope`](../09-gyroscope), [`10-humidity`](../10-humidity),
[`11-light`](../11-light) and [`12-pir-motion`](../12-pir-motion): a
dedicated sampling thread publishes onto a zbus channel, a subscriber
with its own thread logs it, and you extend by adding your own
subscriber rather than touching the sampling thread.

## Wiring

Plug the Angle Unit's Grove cable into **Port B** (the middle Grove
port on the M5Stack Fire's bottom edge, labelled `B`) before flashing.

This uses the same physical pin as `10-humidity`'s probe and
`11-light`'s sensor (Port B's D0 line, GPIO36) - only one of the three
can be plugged in at a time.

## The hardware

A potentiometer is not a smart sensor chip - there is no Zephyr sensor
driver for it, so (same as `10-humidity`/`11-light`) this reads the ADC
directly (`CONFIG_ADC`, not `CONFIG_SENSOR`). Port B's D0 pin is
GPIO36, which is ADC1 **channel 0** - confirmed against M5Stack's own
docs and ESP-IDF's `adc_channel.h` (`ADC1_CHANNEL_0_GPIO_NUM` `36`).
See
[`boards/m5stack_fire_esp32_procpu.overlay`](boards/m5stack_fire_esp32_procpu.overlay)
for the devicetree setup.

There is deliberately no degrees conversion anywhere in this example -
same reasoning as `11-light`: a bare potentiometer's raw ADC count
doesn't map to a physical angle without calibrating against this
specific unit's real rotation range, which nobody has done. Instead,
`src/main.c` reports the raw count and a simple left/right/none turn
direction, derived by comparing consecutive readings against a
threshold (`ANGLE_THRESHOLD`) - not a calibrated unit, just "did it
move enough to count as a turn."

### Why this samples only once every 2 seconds

Same as `10-humidity`/`11-light`: the ESP32 ADC driver re-applies its
hardware calibration code on every single read, which is audible on
this board's onboard speaker and scales with read frequency - see the
main `zephyr-m5-fit` README's "Known issue: using the ADC makes a
sound" section. Sampling faster would make the direction tracking feel
more responsive, but would also make it noisy again.

## The design

Same template, same reasoning, as `09-gyroscope`'s README — a dedicated
sampling thread so this sensor's sample rate is never affected by
anything else the application does, and a zbus **subscriber** (own
thread, own message queue) rather than a listener, so a slow or
blocking reaction can never stall sampling.

## Extending this

Add your own logic as another subscriber of `angle_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to
touch `angle_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 13-angle
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell and watch the log output — a
`angle: raw=NNNN direction=L/R/N` line every 2 seconds. Turn the knob
and `raw` should change, with `direction` showing `R` or `L` depending
on which way you turned it since the last sample.
