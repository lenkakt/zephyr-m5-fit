# 14-keyboard

Reads an M5Stack CardKB Mini Keyboard Unit plugged into Grove Port A,
and logs each key press/release to the shell. Same template as
[`09-gyroscope`](../09-gyroscope), [`10-humidity`](../10-humidity),
[`11-light`](../11-light), [`12-pir-motion`](../12-pir-motion) and
[`13-angle`](../13-angle): a dedicated sampling thread publishes onto a
zbus channel, a subscriber with its own thread logs it, and you extend
by adding your own subscriber rather than touching the sampling
thread.

## Wiring

Plug the CardKB's Grove cable into **Port A** (I2C - SDA=GPIO21,
SCL=GPIO22), the same bus the onboard MPU6886 (`09-gyroscope`) is
already on. This is a different port than `10-humidity`, `11-light`,
`12-pir-motion` and `13-angle` (all Port B) - none of those conflict
with this one.

## The hardware

The CardKB is not a generic sensor - there's no Zephyr driver for it,
so this reads it directly over I2C (`CONFIG_I2C`, not `CONFIG_SENSOR`).
Confirmed against M5Stack's own docs: I2C address **0x5F**. Its
register-level protocol isn't in M5Stack's own documentation beyond
the address, but third-party write-ups agree it's about as simple as
I2C gets: a plain read with no register address to write first - the
returned byte is `0` when no key is pressed, otherwise the ASCII code
of the key. See
[`boards/m5stack_fire_esp32_procpu.overlay`](boards/m5stack_fire_esp32_procpu.overlay).

Unlike `10-humidity`/`11-light`/`13-angle`, this doesn't touch the ADC
at all, so none of the speaker-noise concern from those examples
applies here - poll it as often as you like.

## The design

Same template, same reasoning, as `09-gyroscope`'s README — a dedicated
sampling thread so this device's poll rate is never affected by
anything else the application does, and a zbus **subscriber** (own
thread, own message queue) rather than a listener, so a slow or
blocking reaction can never stall sampling.

Same difference from `09-gyroscope`/`10-humidity`/`11-light`/`13-angle`
as `12-pir-motion`: `key_sampling_thread()` only publishes when the
pressed/released state actually changes, not on every poll - a
keypress is naturally event-shaped, not a continuously-varying reading.

## Extending this

Add your own logic as another subscriber of `key_chan` — a second
`ZBUS_SUBSCRIBER_DEFINE` plus its own `K_THREAD_DEFINE`, added to the
`ZBUS_OBSERVERS(...)` list in `src/main.c`. You should not need to
touch `key_sampling_thread()` at all.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 14-keyboard
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open the shell and watch the log output. Press a key on the CardKB — a
`key: 'x' (0x78) pressed` line should appear (showing the character for
printable keys, just the hex code otherwise), followed by
`key: released` when you let go.
