# E-Ink Dashboard Mirror (add-on)

A Home Assistant add-on that screenshots one or more real Lovelace dashboards, converts each to pure black/white (or 4-level grayscale, if your panel supports it) at that panel's own configured resolution, and writes it as a raw file for each e-ink panel's own firmware to fetch over WiFi.

No ESPHome, no compile-per-update, no `online_image` decode step on the ESP32 — this add-on does all the rendering; each panel just downloads a small file and paints it, using the OTP-waveform driver confirmed clean in [waveshare-7in5-v2-esp32-fix](https://github.com/DSchreyer/waveshare-7in5-v2-esp32-fix).

## How it works

```
[HA dashboard URL] --(headless Chromium)--> screenshot.png
                                                  |
                                  (fit + threshold/quantize + invert)
                                                  v
                                   eink_dashboard_<device>.bin
                                                  |
                                    /config/www/  (served at /local/...)
                                                  |
                                    (ESP32 fetches over WiFi, on its own timer)
                                                  v
                                            [e-ink panel]
```

Every device you configure (see below) is rendered and scheduled independently, each with its own dashboard URL, capture size, fit mode, threshold, and refresh interval.

## Trusted Networks (do this first)

A stateless headless browser has no saved login session, so it needs to reach your dashboard without hitting the login page. Home Assistant's built-in `trusted_networks` auth provider solves this with no credentials involved at all. Add to your HA `configuration.yaml`:

```yaml
homeassistant:
  auth_providers:
    - type: trusted_networks
      trusted_networks:
        - 192.168.1.0/24        # your LAN
        - 172.30.32.0/23        # the Supervisor's internal add-on network -- the add-on itself runs here, not your LAN
      trusted_users:
        192.168.1.0/24: YOUR_USER_ID
        172.30.32.0/23: YOUR_USER_ID
      allow_bypass_login: true
    - type: homeassistant        # keeps normal login working for everyone else
```

Restart Home Assistant after adding this.

Two things worth knowing:
- **Both subnets are needed.** It's easy to only add your LAN and assume that covers it — but the add-on's headless browser runs *inside* HA's Supervisor Docker network, which is a completely separate subnet from your LAN even though it's the same physical host. Find your Supervisor's actual internal subnet with **Settings → System → Network** (Supervisor tab) if `172.30.32.0/23` isn't right for your setup.
- **`trusted_users` matters if you have more than one HA user.** With multiple accounts, plain `allow_bypass_login` shows an account picker instead of skipping straight through. Restricting the trusted network to one user ID fixes that — find your user ID in `.storage/auth` on the HA host, or via **Settings → People**.

## Installing

This is a **local add-on** (not published to a store). To install it:

1. Copy the `addon/` folder to `/addons/local/eink-dashboard-mirror/` on your HA host (via Samba, the File Editor/SSH add-on, or however you normally reach HA's filesystem).
2. In HA: **Settings → Add-ons → Add-on Store → ⋮ (top right) → Check for updates** (or restart the Supervisor) so it picks up the new local add-on.
3. It'll appear under **Local add-ons**. Install and start it.
4. Open its **Web UI** (the ingress panel — a button on the add-on's page, or find it in the sidebar) to add and configure devices: dashboard URL, capture size, fit mode, threshold, invert, refresh interval. Click **Preview** to see the black/white result before committing, **Save** to make it live (applies immediately, no restart needed), **Refresh now** to push it to the panel right away instead of waiting for the schedule.
5. Confirm the output file is reachable: `http://<your-ha-host>:8123/local/eink_dashboard_<device-name>.bin` (and `eink_dashboard_<device-name>_preview.png` for a normal image you can view in a browser).

## Adding a second panel

Click **+ Add device** in the web UI, give it a name (e.g. `kitchen`), configure and save it — then flash that physical panel with `firmware/`, setting `DEVICE_NAME` to match. See the firmware README.

## Fit modes

- **Letterbox** (default) — preserves the whole dashboard, scaled to fit, with white borders if the aspect ratio doesn't match. Use this if your dashboard is designed for a different screen size than the panel.
- **Crop** — fills the panel exactly, cropping anything outside the matched aspect ratio. No borders.
- **Stretch** — fills exactly by distorting proportions. Rarely what you want, included for completeness.

## Full page capture

If your dashboard's real content is taller than `capture_height` (e.g. it was designed for a scrolling tablet view rather than a fixed panel), a plain screenshot silently crops it there — the same as scrolling a browser window and screenshotting only what's visible. Turning on **Full page** in the device's settings instead captures the entire scrollable page at its true height, then lets **Fit mode** (use Letterbox here) scale that whole, taller image down to fit the panel — nothing gets lost. Leave it off if `capture_width`/`capture_height` already match your dashboard's real rendered size.

## Panel size and other hardware

Every device has its own **Panel width/height** — this is the *final* output resolution (after fit/crop/stretch), separate from Capture width/height (the browser screenshot size). It defaults to 800×480 (this add-on's own proven hardware, a Waveshare 7.5" V2), but any device can be pointed at different panel dimensions. Whatever you set here **must match your firmware's own buffer size exactly** — a mismatch silently breaks the fixed-size download/decode logic on the ESP32 side. Written for one specific panel model (see the driver-fix repo linked above); a *different* panel model needs its own firmware driver, not just a resolution change here — see that repo's README if you're bringing up new hardware.

There's also a **Colors output** setting: `bw` (default, 1-bit black/white) or `gray4` (4-level grayscale). Grayscale only does something useful if your panel's actual hardware waveform supports more than 2 levels — many e-ink panels don't, so check your panel's datasheet first. When `gray4` is on, the packed output is 2 bits/pixel instead of 1, and `threshold` is ignored (grayscale quantizes to 4 fixed levels instead of one black/white cutoff).
