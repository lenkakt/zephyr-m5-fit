# 02-buttons

Prints a message every time one of the M5Stack Fire's four physical
buttons is pressed or released.

## Why no devicetree overlay is needed here

Unlike the LED strip, the buttons are already fully described in Zephyr's
mainline `m5stack_fire` devicetree — four `gpio-keys` nodes (`button0`
through `button3`), each with a `zephyr,code` (`INPUT_KEY_0`..`INPUT_KEY_3`)
and a `label` ("button left", "button middle", "button right", "button 2").
That's exactly the shape Zephyr's **input subsystem** expects, so this
example just enables it rather than writing any devicetree of its own.

## How the code works

Zephyr has two different ways to react to a button: read a GPIO pin
directly yourself (polling or a raw interrupt), or let the **input
subsystem** turn button presses into a stream of generic events your code
subscribes to. Since the board already describes its buttons as
`gpio-keys` — a devicetree convention specifically meant for the input
subsystem — that's the natural fit here, and it means the code below has
no idea which GPIO pin any button is even wired to; it only deals with
button *identities* (`INPUT_KEY_0`, etc.).

### Subscribing to button events

Rather than asking "is this specific pin high or low right now?", the
code registers a callback once, and Zephyr calls it whenever *any*
registered input device reports an event — a press, a release, or other
input types this same subsystem also handles (like mouse movement, on
boards that have one).

*Programmer's detail:* this is `INPUT_CALLBACK_DEFINE`, which takes the
input device to listen to (`NULL` here means "all of them" — fine, since
this board only has the one gpio-keys input device) and the callback
function itself:

```c
INPUT_CALLBACK_DEFINE(NULL, button_input_cb, NULL);
```

### What's inside an event

Each event carries a `type` (we only care about `INPUT_EV_KEY`, i.e. a
button — the subsystem is generic enough to also carry things like
relative-motion events), a `code` identifying *which* button
(`INPUT_KEY_0`..`INPUT_KEY_3`, matching the `zephyr,code` values in the
devicetree), and a `value` (1 for pressed, 0 for released).

```c
static void button_input_cb(struct input_event *evt, void *user_data)
{
	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	LOG_INF("Button %s (code %d) %s", button_name(evt->code), evt->code,
		evt->value ? "pressed" : "released");
}
```

`button_name()` is just a small local lookup table translating the four
`INPUT_KEY_*` codes into the same left/middle/right/2 names printed on the
board's own silkscreen — Zephyr doesn't know those names, only the
generic numeric codes.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 02-buttons
```

## Flash

```sh
west flash
```

(add `--esp-device /dev/ttyACM0` if autodetection picks the wrong port —
see [../01-hello/README.md](../01-hello/README.md)).

## Verify

Open a serial console:

```sh
picocom -b 115200 /dev/ttyACM0
```

Press any of the four buttons — you should see a log line per press and
per release, e.g.:

```
[00:00:04.210,000] <inf> main: Button left (code 0) pressed
[00:00:04.310,000] <inf> main: Button left (code 0) released
```

`CONFIG_INPUT_SHELL=y` is also enabled, so at the `uart:~$` prompt you can
run `input dump on` to see the same events as raw input-subsystem
notifications, independent of this app's own logging — a good way to
confirm the *devicetree* side of things is correct even before trusting
the app's own code.
