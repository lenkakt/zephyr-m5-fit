# 07-wifi

Connects the M5Stack Fire to WiFi and reports when it gets an IP address
- with the SSID and password provisioned over the shell at flash time,
never hardcoded in this source file.

## Wi-Fi in Zephyr

Wi-Fi isn't a special case in Zephyr's networking stack - it's one of
several **L2 technologies** (Ethernet is another) that all plug into the
same `net_if` abstraction. Your application code doesn't talk to the
ESP32's Wi-Fi radio directly; the ESP32 Wi-Fi driver implements a
standard driver API, the networking stack sits on top of that, and your
app talks to the networking stack the same way regardless of which L2
technology is actually underneath. That's why the calls you'll see below
are generic `net_mgmt`/`net_if` calls, not anything ESP32-specific.

Wi-Fi operations - scanning, associating with an access point, getting
an IP address - are inherently asynchronous: they involve real radio
communication and protocol exchanges that take an unpredictable amount
of time, and can fail or need a retry. Rather than make every such call
block until it's done, Zephyr's networking stack reports outcomes
through a general-purpose **event system**, `net_mgmt` events: your code
registers a callback for the events it cares about, triggers the
operation, and the stack calls that callback later, whenever something
actually happens. This isn't Wi-Fi-specific either - the same mechanism
reports IPv4/IPv6 address changes, interface up/down transitions, and
more, across all of Zephyr's networking.

On top of this, the Wi-Fi subsystem optionally exposes an interactive
**shell** (`wifi scan`, `wifi connect`, `wifi status`, and the `wifi
cred` commands used below) - useful for poking at the radio and
inspecting connection state by hand, independently of whatever your
application code does.

## How it's done in this example

### Why `main()` can't just "call connect and carry on"

`net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0)` does
**not** mean "block here until connected." Per the async pattern above,
it only *starts* the connection attempt and returns immediately - its
return value just says whether the request itself was accepted (e.g. a
stored credential was found at all), not whether the Wi-Fi handshake
that follows will actually succeed. That's what the two
`net_mgmt_add_event_callback()` registrations are for: instead of
`main()` polling "are we connected yet?" in a loop, it subscribes once,
up front, and the networking stack calls `event_handler()` on its own
whenever something relevant happens.

### The two events, and why both are needed

- `NET_EVENT_WIFI_CONNECT_RESULT` - the radio-level handshake with the
  access point finished (successfully or not). This is "the AP let us
  join its network" - it says nothing about IP connectivity yet.
- `NET_EVENT_IPV4_ADDR_ADD` - a separate, later step: DHCP actually
  handed out a usable IP address. This is the real "ready to talk to a
  server" signal - InfluxDB, or anything else on the network, is
  unreachable before this fires, even though the Wi-Fi association
  itself already succeeded.

Both callbacks have to be registered *before* the connect request is
issued (that's why `main()` does it in that order) - if registration
happened after, an event that fires quickly could be missed entirely.

### Step by step

1. `net_if_get_wifi_sta()` - get the Wi-Fi station interface. This can
   return `NULL` if no Wi-Fi STA interface is registered at all, so it's
   checked before use.
2. Register for the two events described above.
3. Issue the connect request using whatever credential was provisioned
   earlier (see "Credentials" below) - no SSID or password is a function
   argument here.
4. Whenever `NET_EVENT_IPV4_ADDR_ADD` eventually fires,
   `print_wifi_status()` queries `NET_REQUEST_WIFI_IFACE_STATUS` - the
   same request the `wifi status` shell command itself issues - and logs
   the connection state and SSID. Useful both as a sanity check and as a
   template for checking Wi-Fi status from application code instead of
   only from the shell.

## Build and flash

```sh
west build -b m5stack_fire/esp32/procpu -d build 07-wifi
west flash
```

Flashing alone isn't enough to connect to anything yet - without a
credential provisioned (next section), `main()` will log that no stored
credential was found and stop there.

## Credentials

### Why not just `#define WIFI_SSID "..."` in the source?

That's the obvious first approach: a `#define WIFI_SSID "..."` /
`#define WIFI_PSK "..."` pair in a header, kept out of git by hand. It
works, but:

- The password lives in a plaintext file on disk, which is easy to
  accidentally commit (that header has to be manually `.gitignore`'d
  and everyone has to remember that).
- Every device needs the same source rebuilt/reflashed if the password
  ever changes, and the password is baked into the `.bin`/`.elf` itself -
  readable by anyone who gets the firmware image.
- If you use the same source tree for multiple students/networks,
  everyone's password has to pass through the same file at some point.

Zephyr's **Wi-Fi credentials subsystem**
(`include/zephyr/net/wifi_credentials.h`) exists specifically to avoid
this - the credential never appears in any file in this project's source
tree, and isn't rebuilt into the binary at all. Instead, it's provisioned
separately, interactively, on the already-flashed device, over the
**shell**: a small command-line interpreter Zephyr runs on the target
itself (enabled here via `CONFIG_SHELL=y`), reachable over the same
USB-serial connection already used for flashing and log output. Open it
with `west espressif monitor`, or any serial terminal pointed at the
board's port, and you get a `uart:~$` prompt you can type commands into
live, while the application is running.

### Adding the network (do this once per network)

```
uart:~$ wifi cred add -s "your-ssid" -k 1 -p "your-password"
```

`-k` selects the **security/key-management type** - how the password is
actually used to authenticate, not just "is there a password." `1` is
WPA2-PSK, the pre-shared-key scheme almost every home router and phone
hotspot uses: one shared password, known to the router and typed into
every device that joins. (Other values exist for WPA3, enterprise
networks with per-user logins, and open/no-password networks - `wifi
cred add -h` lists them all; `-k 1` is the right default here.)

Two more commands round this out: `wifi cred list` shows what's
currently stored (SSID only - never the password, see below), and `wifi
cred delete` removes an entry, e.g. if you mistyped the password or want
to switch networks.

The command above doesn't connect to anything by itself - it only writes
the credential to flash. The actual connection attempt happens the
*next* time `main()` runs, because that's where the connect call lives -
so reset the board (or power-cycle it) once after provisioning, and
watch it connect on that boot and every boot after, with no further
shell interaction needed.

### How `wifi cred add` actually works

1. The shell command packs what you typed (SSID, security type, password,
   flags) into a `struct wifi_credentials_personal`
   (`wifi_credentials_set_personal()` in
   `subsys/net/lib/wifi_credentials/wifi_credentials.c`).
2. That struct is handed to the configured **storage backend** - by
   default, Zephyr's own settings subsystem
   (`CONFIG_WIFI_CREDENTIALS_BACKEND_SETTINGS=y`, the default whenever
   `CONFIG_SETTINGS=y` is set, as in this app's `prj.conf`). The settings
   backend writes it under a key like `wifi_cred/0`
   (`wifi_credentials_backend_settings.c`, via `settings_save_one()`) -
   this is the same subsystem Zephyr uses for all kinds of persisted
   config, backed here by NVS on the board's internal flash. Up to
   `CONFIG_WIFI_CREDENTIALS_MAX_ENTRIES` networks can be stored this way
   (default 2; several numbered slots, not just one).
3. Because it's flash, not RAM, it survives a reset or power-cycle - but
   not a full flash erase, and not `west flash` of a *different*
   application that reuses/overwrites that flash region.
4. On the *read* side, this app's `net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED,
   ...)` call walks those stored slots and tries each one - the exact
   same request the `wifi cred auto_connect` shell command issues
   (`wifi_credentials_shell.c`'s `cmd_auto_connect()`). The password is
   decoded from flash into RAM only for the instant it's handed to the
   Wi-Fi driver to authenticate - it's never printed, logged, or visible
   from the shell again (`wifi cred list` shows the SSID, not the
   password).
