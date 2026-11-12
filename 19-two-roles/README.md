# 19-two-roles — one project, two device roles

A sketch of the build mechanism for an application made of two
cooperating devices (e.g. the final assignment's Sensor node and
Gateway): one source tree, one `west build` invocation pattern,
`FILE_SUFFIX` picks which device role gets built — the same mechanism
`18-release` uses to pick dev vs. release, repurposed here to pick a
device role instead of a security posture.

Both roles' `main()` do nothing but log which one they are — this is
a sketch of the project structure, not a working application. No
Wi-Fi, no MQTT, no sensors.

## Structure

- **`common.conf`** — Kconfig shared by both roles (`CONFIG_LOG=y`),
  pulled in via `EXTRA_CONF_FILE` in `CMakeLists.txt` so it isn't
  duplicated in `prj_a.conf`/`prj_b.conf` (see `18-release`'s README
  for why `FILE_SUFFIX`-selected `.conf` files replace rather than
  merge, and why `EXTRA_CONF_FILE` is the fix).
- **`prj_a.conf`** / **`prj_b.conf`** — one per role; empty for now,
  this is where role-specific Kconfig would go (e.g. Wi-Fi AP support
  for a gateway role vs. a plain STA client for a sensor role).
- **`src/main_a.c`** / **`src/main_b.c`** — one per role.
  `CMakeLists.txt` picks which one to compile by checking the
  `FILE_SUFFIX` CMake variable directly, the same variable Zephyr's
  own build system already uses to pick the `.conf` file.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr

# Role A
west build -b m5stack_fire/esp32/procpu -d build-a 19-two-roles -- -DFILE_SUFFIX=a

# Role B
west build -b m5stack_fire/esp32/procpu -d build-b 19-two-roles -- -DFILE_SUFFIX=b
```

Verified: both roles build clean, zero warnings, pristine (`-p
always`) rebuilds; each image contains only its own role's string
(confirmed via `strings zephyr.elf`), not both.

## Flash

```sh
west flash -d build-a   # or build-b
```

## Verify

Over the shell (`picocom -b 115200 /dev/ttyACM0`): the boot log shows
either `I am device A` or `I am device B`, depending on which build
directory was flashed.

## What this deliberately doesn't do

No actual sensor, Wi-Fi, MQTT, or feedback-loop logic — that's the
final assignment's job. This example only answers "can one project
build two different devices," not "how do they talk to each other."
