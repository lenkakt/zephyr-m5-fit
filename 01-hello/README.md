# 01-hello

Minimal Zephyr app for the M5Stack Fire — prints a hello
message and enables the interactive shell.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 01-hello
```

Run from the repo root. If you're already inside `01-hello`, drop the
trailing argument and build from the repo root instead — `west` needs the
app's `CMakeLists.txt` path relative to where you run it.

## Flash

Connect the M5Stack Fire via USB, then:

```sh
west flash
```

If autodetection picks the wrong port (or none), specify it explicitly:

```sh
west flash --esp-device /dev/ttyACM0
```

(port may be `/dev/ttyUSB0` instead — same caveat as below).

## Verify the shell

Open a serial terminal at 115200 baud on the board's USB port:

```sh
picocom -b 115200 /dev/ttyACM0
```

The port may show up as `/dev/ttyUSB0` instead, depending on your system —
check `dmesg` right after plugging the board in if `/dev/ttyACM0` isn't
there.

You should see:

```
Hello, M5Stack Fire! Zephyr is alive.
uart:~$
```

Press Tab at the empty `uart:~$` prompt to list the available shell
commands.
