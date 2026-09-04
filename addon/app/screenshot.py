"""Headless screenshot of a live URL -- no HTML templating, no HA API calls:
this literally screenshots the real Lovelace dashboard page, at exactly the
panel's resolution, so what's on the panel matches what's on the URL.

Uses Playwright (pointed at the system-installed `chromium` binary, not a
separately-downloaded one) rather than Chromium's bare `--screenshot` CLI
flag: HA's frontend loads live entity state over a WebSocket after the
page's `load` event fires, and Chromium's `--virtual-time-budget` (the
only way to add a wait with the CLI-only approach) doesn't reliably let
that WebSocket data arrive -- confirmed during bring-up, it reproducibly
screenshots the "Loading data" splash instead of the actual dashboard.
Playwright's `wait_until="networkidle"` plus a real (non-virtual) settle
delay waits for actual wall-clock time, which the WebSocket connection
needs.
"""
import time

from playwright.sync_api import sync_playwright

CHROMIUM_PATH = "/usr/bin/chromium"


def screenshot(url: str, png_path: str, width: int, height: int, wait_seconds: int) -> None:
    with sync_playwright() as p:
        browser = p.chromium.launch(
            headless=True,
            executable_path=CHROMIUM_PATH,
            args=["--no-sandbox", "--disable-gpu", "--disable-software-rasterizer"],
        )
        try:
            page = browser.new_page(
                viewport={"width": width, "height": height},
                device_scale_factor=1,
            )
            page.goto(url, wait_until="networkidle", timeout=30000)
            # Real wall-clock settle time for HA's WebSocket-driven state
            # population to finish after networkidle -- entity states can
            # arrive just after the network itself goes quiet.
            time.sleep(max(0, wait_seconds))
            page.screenshot(path=png_path)
        finally:
            browser.close()
