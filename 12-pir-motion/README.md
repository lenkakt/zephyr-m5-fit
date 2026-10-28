# 12-pir-motion

Reads an M5Stack PIR Motion Unit plugged into Grove Port B, and logs
"DETECTED"/"clear" to the shell whenever it changes. Same template as
[`09-gyroscope`](../09-gyroscope), [`10-humidity`](../10-humidity) and
[`11-light`](../11-light): a dedicated sampling thread publishes onto a
zbus channel, a subscriber with its own thread logs it, and you extend
by adding your own subscriber rather than touching the sampling thread.

## Wiring

Plug the PIR Motion Unit's Grove cable into **Port B** (the middle
Grove port on the M5Stack Fire's bottom edge, labelled `B`) before
flashing.

This uses Port B's D0 pin (GPIO36) - confirmed against multiple
independent vendor/retailer guides for this specific unit, since
there's no official M5Stack datasheet readily available. That's the
same pin `10-humidity`'s probe and `11-light`'s sensor use (not the D1
pin `10-humidity`'s threshold signal uses) - so this is mutually
exclusive with those two, same Grove port either way.

## The hardware

A PIR motion sensor is a plain digital output - no ADC, no I2C, nothing
to calibrate: the pin reads HIGH while motion is detected, LOW
otherwise. See
[`boards/m5stack_fire_esp32_procpu.overlay`](boards/m5stack_fire_esp32_procpu.overlay) -
same `"gpio-keys"`-wrapper gotcha as `10-humidity`'s threshold pin
(a bare devicetree node with no `compatible` doesn't get its `gpios`
property recognized as a phandle-array, so `GPIO_DT_SPEC_GET()` fails
to compile without it), with the input-subsystem semantics it normally
implies unused here too.

Because this is a plain GPIO, not the ADC, none of `10-humidity`'s or
`11-light`'s speaker-noise concern applies here - poll it as often as
you like.

## The design

Same template, same reasoning, as `09-gyroscope`'s README — a dedicated
sampling thread so this sensor's poll rate is never affected by
anything else the application does, and a zbus **subscriber** (own
thread, own message queue) rather than a listener, so a slow or
blocking reaction can never stall sampling.

One real difference from the other three: `motion_sampling_thread()`
only publishes when the state actually **changes**, not on every poll.
Motion is naturally event-shaped - polled every 20ms but changing far
less often - unlike the continuously-varying readings the other three
examples publish unconditionally every sample.

## Extending this

Add your own logic as another subscriber of `motion_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to
touch `motion_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 12-pir-motion
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell and watch the log output. Wave a hand in front of the
sensor — a `motion: DETECTED` line should appear, followed by
`motion: clear` a few seconds after you stop moving (this specific
unit's own hardware holds its output high for about 2-3 seconds after
the last detected motion, per vendor documentation - not something
this example's polling rate controls).

### Why it may seem to only catch the first movement

PIR sensors detect *change* in infrared signature, not continuous
presence, and real modules (this one included) have a hardware
blind/reset time after each trigger before they reliably detect again -
on top of the ~2-3 second output-hold time above. If it looks like only
your first movement registered, wait a few seconds after `clear` before
moving again; this example already polls far faster (20ms) than the
sensor itself can usefully change state, so a slower-feeling reaction
is the sensor's own hardware characteristic, not something to fix by
polling faster.
