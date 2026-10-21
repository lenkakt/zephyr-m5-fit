# 08-http

Ties together Wi-Fi (`07-wifi`), the onboard LED strip (`03-led-strip`),
and the buttons (`02-buttons`) with a real HTTP client: polls a small
color server every few seconds and sets the LED strip to match, and
sends a "change" request when a button is pressed. This is the
reference solution behind `bi-iot`'s week-5 lab
(`tutorials/items/05.adoc`) - the server it talks to is
`infra/color-server`.

## Before building: set the server address

`SERVER_HOST`/`SERVER_PORT` at the top of `src/main.c` are placeholders
(`192.0.2.1` is a documentation-only address that will never resolve to
anything real) - set them to wherever `color-server` is actually
running before building.

## Build, provision, flash

Same as `07-wifi` - this example reuses that exact Wi-Fi setup:

```sh
west build -b m5stack_fire/esp32/procpu -d build 08-http
west flash
```

Then, over the device shell:

```
uart:~$ wifi cred add -s "your-ssid" -k 1 -p "your-password"
```

Reset the board once after provisioning. See `07-wifi/README.md` for a
full explanation of how that works and why.

## The color server's contract

```
GET  /color          -> current color, plain text (one word)
POST /color "change" -> server picks a new random color, returns it
```

One word from `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `PURPLE`.

## What's new here, on top of `07-wifi`

`07-wifi` stops at "got an IP address." This example continues from
there: once `NET_EVENT_IPV4_ADDR_ADD` fires, it starts actually talking
to something on the network.

### A fresh socket per request

Each HTTP exchange opens its own short-lived TCP socket
(`socket()`/`connect()`), does exactly one request with
`http_client_req()`, and closes it (`close()`) - there's no long-lived
connection kept open between requests. For how small and infrequent
these requests are (one word, every few seconds), the overhead of a new
TCP handshake each time is irrelevant, and it avoids having to detect
and recover from a connection that silently died while idle.

### Reading the response body

`http_client_req()` doesn't return the response body as a return value -
it calls your `response` callback (here, `response_cb()`) with a
`struct http_response *rsp`, and the body (or a fragment of it, for
larger responses) is at `rsp->body_frag_start`, `rsp->body_frag_len`.
This example's responses are always a few bytes, so one callback call
with the whole body is normal; the callback copies it out into a
`static` buffer, because `rsp->body_frag_start` points into the
request's own `recv_buf`, which stops being valid once
`http_client_req()` returns.

### Why the button handler doesn't just call the HTTP code directly

`button_input_cb()` runs in the input subsystem's own thread context,
not `main()`'s. Blocking it on network I/O (DNS/connect/send/recv, any
of which can take seconds or hang) would delay every other button event
behind it. Instead, the callback does the minimum possible -
`k_work_submit()`, which just queues a work item - and the actual HTTP
request happens later, on the system workqueue, in
`request_change_work_handler()`. This is the standard Zephyr pattern for
"something a fast interrupt-ish callback triggers, but that itself needs
to take its time": don't do the slow part in the callback, hand it to a
work item instead.

### Polling vs. waiting to be told

There's no server-side push here - nothing tells this device when the
color changes except its own 3-second poll (`poll_color_work`,
rescheduling itself with `k_work_schedule()` every time it runs). That
means a color change from the "change" button, or from another device
polling and changing it, shows up here with up to ~3 seconds of delay,
never instantly. Polling is simple and is a completely reasonable choice
here; just notice that you traded immediacy for simplicity - a
server-push mechanism (e.g. a persistent connection with server-sent
events, or MQTT) would remove that delay, at the cost of more moving
parts.
