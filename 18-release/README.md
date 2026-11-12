# 18-release — provisioning a serial number, then switching to production config

Two things, deliberately in one example because they're one workflow
on a real production line: how a device gets its own, assignable
serial number separately from flashing its firmware, and how the
same firmware switches between a development build (shell on, for
provisioning and debugging) and a release build (shell off, for
shipping).

## Two kinds of "device ID" — don't confuse them

- **Silicon ID** (`hwinfo_get_device_id()`) — fixed at manufacturing,
  read-only, the same every time you ask. Printed on every boot here.
  You can't assign it, and it isn't a serial number in the "this unit
  came off the line on this date, batch N" sense.
- **Assigned serial number** — whatever your production process
  decides it should be. Nothing in Zephyr invents this for you; you
  write it in yourself, once, as ordinary data.

## How the serial actually gets written in

The firmware is mass-identical across every unit - `west flash`
writes it into the `image-0` partition only. The serial is assigned
*afterwards*, as a separate step, into the **Settings** subsystem
(NVS-backed, in the board's `storage` partition) - the exact same
mechanism `07-wifi`'s `wifi_credentials` already uses on this board,
just under its own `device/` key namespace instead of
`wifi_credentials`'s own. Settings is a key-value store, not a
single slot, so the two coexist with no conflict.

On the production line, over the shell, once per unit:

```
settings write string device/serial "SN-00042"
```

then reset. That's it - one shell command, not a per-unit firmware
build. This app reads it back at boot with
`settings_load_subtree_direct()`, the same call `wifi_credentials`
itself uses internally.

## One source code, two configurations

Zephyr enables you to keep several different configurations for one project: one source tree, one app, a second `.conf` file picked at build time via `FILE_SUFFIX`.

This can be used to keep 2 configurations: development version with low security and secured production version. 

```sh
# Development (default) - prj.conf: shell on, for provisioning and debugging.
west build -b m5stack_fire/esp32/procpu -d build 18-release

# Release - prj_release.conf, selected via FILE_SUFFIX: shell off.
west build -b m5stack_fire/esp32/procpu -d build-release 18-release -- -DFILE_SUFFIX=release
```

**`FILE_SUFFIX` replaces, it doesn't merge**: it picks exactly one of
`prj.conf`/`prj_release.conf` to fill the `CONF_FILE` slot (see
`cmake/modules/configuration_files.cmake`'s
`zephyr_file(CONF_FILES ... SUFFIX)`) - whichever one is *not*
selected contributes nothing. An early version of this example put
every option in each of those two files (duplicated in both), which
is how a first, broken attempt at `prj_release.conf` that only
listed the lines that should *change* failed with undefined
references to `hwinfo`/`settings` functions - `prj.conf` was never
included at all for that build.

The fix actually used here: **`common.conf`** holds everything shared
between the two builds (`CONFIG_LOG`, `CONFIG_HWINFO`, `CONFIG_SETTINGS`
and friends), and `CMakeLists.txt` pulls it in via
`set(EXTRA_CONF_FILE common.conf)` - a second config input that is
always merged on top of whatever `CONF_FILE` resolves to,
**regardless of `FILE_SUFFIX`** (confirmed in the same file: unlike
`CONF_FILE`, `EXTRA_CONF_FILE` is never passed through
`zephyr_file(... SUFFIX ...)`). That leaves `prj.conf` and
`prj_release.conf` holding only what actually differs between the two
builds - `CONFIG_SHELL`/`CONFIG_SETTINGS_SHELL` - instead of
duplicating the shared options in both.

The actual sequence on a real line, then: flash → provision the
serial over the shell (dev config, shell on) → reset into the
release build (shell off) for shipping. Verified: both configs build
clean against this workspace's `~/zephyrproject`, zero warnings,
pristine (`-p always`) rebuilds. The flash footprint drops by about
23 KB with the shell stripped out, confirming the config actually
changed something.

## What this deliberately doesn't do

A real product's release config also adds `CONFIG_BOOTLOADER_MCUBOOT=y`
signed with a real production key (not the debug/test key used
during development), and `CONFIG_ESP_FLASH_ENCRYPTION=y`. Both are
covered conceptually in the security talk and deliberately not built
here - flash encryption makes an irreversible eFuse change, not
something to run on shared lab hardware.

## Build

```sh
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -b m5stack_fire/esp32/procpu -d build 18-release
```

## Flash

```sh
west flash
```

## Verify

Over the shell (`picocom -b 115200 /dev/ttyACM0`):

- On first boot, the log shows the silicon device ID (hexdump) and a
  warning that no serial is provisioned yet, with the exact command
  to run.
- Run `settings write string device/serial "SN-00042"`, then `kernel
  reboot cold` (or power-cycle). The log now shows `assigned serial
  number: SN-00042`.
- Rebuild with `-DFILE_SUFFIX=release` into a different build dir,
  reflash, and confirm the shell is gone (no prompt at all) while the
  serial is still read back correctly on boot.
