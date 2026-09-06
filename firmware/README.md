# E-Ink Dashboard Mirror (firmware)

The panel side of the pair: a small, persistent ESP32 firmware that fetches a pre-rendered image over plain WiFi and displays it. No ESPHome, no on-device rendering, no runtime image decoding — the `../addon` half does all the work of screenshotting and converting; this just downloads a file and paints it.

Built on the confirmed-working Waveshare 7.5" e-Paper V2 driver from [waveshare-7in5-v2-esp32-fix](https://github.com/DSchreyer/waveshare-7in5-v2-esp32-fix) — see that repo if you're chasing a display-quality issue (speckled/grainy/faint images); this repo assumes that driver is already correct and focuses on the dashboard-mirroring pipeline built on top of it.

## What it does

Every 60 seconds: connects to WiFi (if not already), fetches `http://<your-ha-host>:8123/local/eink_dashboard_<DEVICE_NAME>.bin`, and — only if the content actually changed since the last successful display (a cheap hash check, not a full redraw every cycle) — refreshes the panel. Polling this often is deliberate and cheap: it's a small HTTP GET, and the panel itself never visibly refreshes unless something real changed, so this is what makes the add-on's "Refresh now" button feel responsive instead of needing a manual power-cycle to notice a new frame.

That's the default, always-on mode — good for a USB-powered desk panel. There's also a battery mode; see below.

## Battery power (deep sleep)

Set `DEEP_SLEEP_ENABLED = true` in `main.cpp` and reflash for a battery-powered panel instead of a USB-powered one. Each cycle then becomes: wake from a full reset, reconnect WiFi, fetch, redraw only if changed, then go straight into deep sleep — cutting power to essentially everything until the next cycle, instead of staying WiFi-associated the whole time (~80-150mA continuously in the default mode). This trades "checks for new content every 60s" for "checks once per interval," in exchange for multi-day/week battery life instead of roughly a day.

The sleep duration isn't a second hardcoded constant to keep in sync by hand — each wake fetches the add-on's current `interval_minutes` for this device (a small text file next to the `.bin`) and sleeps for that long, so changing the refresh interval in the add-on's web UI takes effect on the panel's very next wake, no reflash needed. `DEEP_SLEEP_FALLBACK_MINUTES` in `main.cpp` only matters if that fetch fails (a brand-new device that's never rendered yet, or a network hiccup).

## Wiring

Matches an all-in-one ESP32 driver board (SPI wired to non-default pins) — see `src/epdif.h` if your board differs.

| Signal | GPIO |
|---|---|
| SCK | 13 |
| MOSI | 14 |
| CS | 15 |
| DC | 27 |
| BUSY | 25 |
| RST | 26 |

## Flashing

1. Open `src/main.cpp` and fill in the `-- Config --` section near the top:
   - `WIFI_SSID` / `WIFI_PASSWORD` — your network.
   - `DEVICE_NAME` — must match a device name you've configured in the add-on's web UI.
   - `IMAGE_URL_BASE` — your Home Assistant host.
2. Build and flash with [PlatformIO](https://platformio.org/):
   ```bash
   pio run -t upload --upload-port /dev/YOUR_SERIAL_PORT
   ```

## Multiple panels

Each physical ESP32 is permanently tied to one device by whatever `DEVICE_NAME` was in `main.cpp` when you flashed it — there's no runtime negotiation or "who am I" handshake, deliberately: it's the simplest thing that works, matching how flashing already works. To add a second panel: create a new device in the add-on's UI, then reflash a second ESP32 with `DEVICE_NAME` changed to match.

## A gotcha worth knowing if you're debugging this yourself

Opening a serial connection to most ESP32 boards (including via `pyserial`, `screen`, the Arduino IDE monitor, etc.) auto-resets the chip — the DTR/RTS-toggling bootloader circuit most boards have. That silently re-triggers a full `setup()`/fetch/display cycle. If you're debugging and see the panel refresh right when you open a serial monitor, that's why — not a bug in the fetch loop.
