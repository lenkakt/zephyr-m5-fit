# 09-gyroscope

Reads the M5Stack Fire's onboard gyroscope and logs the values to the
shell, once every 500ms. That's it — no logic beyond printing, on
purpose. This is meant as a clean starting point: a dedicated sampling
thread publishing onto a zbus channel, which you extend with your own
logic (a subscriber of your own) rather than editing the sampling
thread itself.

## The hardware

The Fire has an onboard **MPU6886** six-axis motion sensor (accelerometer
+ gyroscope), on the internal I2C bus at address `0x68`. It's already
defined and enabled in Zephyr's own `m5stack_fire` board devicetree —
nothing to add, no overlay needed for this example.

Zephyr's driver for it is generic: it identifies as `invensense,mpu6050`
(the MPU6886 is pin- and register-compatible enough with the MPU6050
family that the same driver covers it), and exposes it through the
**generic sensor API** — the same API any other sensor (temperature,
humidity, light...) uses. That's worth noticing: once you know this
pattern, reading a different sensor later is the same four calls with a
different channel name.

## The design: one sampling thread, one zbus channel

This is the template meant for every sensor/actuator example in this
course, not just this one:

```
[sampling/actuating thread]  --publish-->  [zbus channel]  --notify-->  [subscriber(s), own thread each]
```

**Why a dedicated thread, not just a loop in `main()`?** `main()` is just
the first thread Zephyr starts — nothing special about it. Giving the
sensor its own thread (`K_THREAD_DEFINE`, near the bottom of
[`src/main.c`](src/main.c)) means its sampling rate is independent of
whatever else your application ends up doing later (driving the display,
talking to a server, reacting to buttons...) — none of that can
accidentally delay or skip a sample.

**Why zbus, and not just a shared variable or a message queue?** A shared
variable needs you to invent your own locking and "is this fresh"
tracking; a message queue is consumed by exactly one reader. What we
actually want is: the sampling thread publishes a reading, and *any
number* of other parts of the program — today just one subscriber that
logs it, tomorrow maybe a display update, a networking thread, a
threshold alarm — can react to it, without the sampling thread knowing
or caring how many there are, or being slowed down if one of them is
slow. That's exactly what zbus's publish/subscribe model gives you, and
it's the same `zbus` you already used in week 2.

`ZBUS_CHAN_DEFINE` declares the channel and its message type
(`struct gyro_msg`); `ZBUS_SUBSCRIBER_DEFINE` declares `gyro_log_sub`, an
observer with **its own thread and message queue** (`gyro_log_thread`,
waiting on `zbus_sub_wait()`). That's the default for *every* observer
in this course, sensors and actuators alike — not just because it's more
robust, but because it removes a judgment call: zbus also offers
`ZBUS_LISTENER_DEFINE`, a callback that runs *synchronously, inside the
publisher's own thread*, which is marginally simpler but means a slow or
blocking reaction silently stalls whatever published the data. A
subscriber can never do that, by construction, so you don't have to
reason case by case about whether your reaction "counts as slow enough"
to need the safer option. See `samples/subsys/zbus/hello_world` in the
Zephyr tree if you want to see a listener alongside a subscriber for
comparison.

## Extending this

Add your own logic as another subscriber of `gyro_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to touch
`gyro_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 09-gyroscope
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell (`picocom`, or whatever you've used for the other
examples) and watch the log output — a `gyro (rad/s): x=... y=... z=...`
line every half second. Tilt or rotate the device and the values should
change accordingly.
