# 04-led-shell

Same LED strip cycle as [`03-led-strip`](../03-led-strip), plus two shell
commands to switch it off and back on — `led off` / `led on`.

## Why this needs its own commands at all

WS2812/SK6812 pixels have no "off" state of their own. Each pixel just
keeps displaying whatever color it last received — forever, until a new
frame arrives, or the strip loses power. Flashing a different app that
never touches the strip won't clear it; the LEDs don't know or care what
firmware is running. The only way to turn them off is to explicitly send
one more update with every pixel set to black.

## How the code works

The color-cycling loop from `03-led-strip` is unchanged. What's new is a
small custom shell command and a flag that the loop checks before writing
a color.

### Registering a custom command

Zephyr's shell lets an app add its own commands, discoverable at the
`uart:~$` prompt like any built-in one. Here, `led` is registered with two
subcommands, `off` and `on`, each just a plain C function:

```c
SHELL_STATIC_SUBCMD_SET_CREATE(led_cmds,
	SHELL_CMD(off, NULL, "Turn the LED strip off", cmd_led_off),
	SHELL_CMD(on, NULL, "Resume the color cycle", cmd_led_on),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &led_cmds, "Control the onboard LED strip", NULL);
```

### Why a flag, not just "clear once and be done"

Typing `led off` clears the strip immediately — but the main loop is still
running once a second, and without anything stopping it, it would simply
paint the next color back on within a second, undoing the command almost
instantly. A `bool strip_on` flag is how the shell command and the main
loop stay in sync: `led off` clears the strip *and* sets the flag so the
loop skips writing new colors until `led on` sets it back.

```c
static bool strip_on = true;
```

The main loop's write is now conditional on it:

```c
while (1) {
	if (strip_on) {
		/* ... build and send the next color, same as before ... */
	}
	k_sleep(K_SECONDS(1));
}
```

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 04-led-shell
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

The strip should start cycling red → green → blue → white, same as
`03-led-strip`. At the `uart:~$` prompt:

```
uart:~$ led off
LED strip off
uart:~$ led on
LED strip resuming color cycle
```

`led off` should turn the strip dark immediately; `led on` should resume
the cycle from wherever it left off.
