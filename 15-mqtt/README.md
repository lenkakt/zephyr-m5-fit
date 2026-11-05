# 15-mqtt

Ties Wi-Fi (`07-wifi`) and the button (`02-buttons`) together with
Zephyr's native MQTT client (`zephyr/net/mqtt.h`): connects to a
broker, subscribes to one topic, and publishes `"ping"` to that same
topic on a button press. This is a minimal starting point for
`bi-iot`'s week-7 lab (`tutorials/items/07.adoc`), not the lab's
solution - it shows the MQTT mechanics on a single device, nothing
more.

## Before building: set the broker address

`BROKER_HOST`/`BROKER_PORT` at the top of `src/main.c` are
placeholders (`10.42.0.1`/`1883`) - set them to wherever the
Mosquitto broker is actually running before building. `CLIENT_ID`
also needs to be unique per device: if two boards connect with the
same client ID, the broker disconnects the older one.

## Build, provision, flash

Same as `07-wifi` - this example reuses that exact Wi-Fi setup:

```sh
west build -b m5stack_fire/esp32/procpu -d build 15-mqtt
west flash
```

Then, over the device shell:

```
uart:~$ wifi cred add -s "your-ssid" -k 1 -p "your-password"
```

Reset the board once after provisioning. See `07-wifi/README.md` for
a full explanation of how that works and why.

## Why this works alone, with no second device

The example subscribes to `zephyr-m5-fit/test` and also publishes to
it. A broker delivers a message back to its own publisher as long as
that client is itself subscribed to the topic - so pressing the
button sends `"ping"`, the broker bounces it straight back, and the
log shows both the publish and the receive (printing `pong`) on the
same device. That's the whole publish/subscribe round trip, without
needing a second board or a lab partner to test it.

To try it between two boards instead, give each one a different
`CLIENT_ID` and point them at each other's topic instead of their
own - exactly what the lab's main task asks for.

## What's new here, on top of `07-wifi`

### No broker on the device

As covered in the lecture: Zephyr only provides an MQTT *client*
library. The broker (Mosquitto, here) has to run somewhere else - a
host machine, not this board. `net/mqtt.h` only ever plays the client
role.

### Polling the MQTT socket, instead of a dedicated thread

Zephyr's MQTT client doesn't run a background thread for you - your
application has to call `mqtt_input()` (process anything received)
and `mqtt_live()` (send a keep-alive ping if it's due) regularly
yourself. This example does that the same way `08-http` polls its
color server: a `k_work_delayable` (`mqtt_poll_work`) that reschedules
itself every 500ms, checking the socket with a zero-timeout `poll()`
first so `mqtt_input()` is only called when there's actually something
to read.

### Reading a received payload

An incoming `MQTT_EVT_PUBLISH` event does **not** hand you the
message bytes directly - `mqtt_read_publish_payload()` has to be
called from inside the event handler to pull them off the socket into
your own buffer. Miss that call and the payload is simply lost; it
isn't buffered anywhere else for you to come back to later.

### Why the button handler doesn't just call `mqtt_publish()` directly

Same reasoning as `08-http`'s button handler: `button_input_cb()` runs
in the input subsystem's thread, and `mqtt_publish()` can block on
socket I/O. The callback only does `k_work_submit()`; the actual
publish happens afterwards, on the system workqueue, in
`publish_ping_work_handler()`.
