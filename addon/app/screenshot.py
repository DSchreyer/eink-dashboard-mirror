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


def screenshot(
    url: str,
    png_path: str,
    width: int,
    height: int,
    wait_seconds: int,
    full_page: bool = False,
) -> None:
    """full_page=False (default) captures exactly the viewport (width x
    height) -- if the dashboard's real content is taller than that, the
    result is silently cropped at height, same as scrolling a browser
    window and screenshotting only what's visible. full_page=True instead
    captures the entire scrollable page at its natural height (which may
    be taller than `height`) -- postprocess.process()'s existing
    fit_mode="letterbox" then scales that whole, taller image down to fit
    the panel, so nothing is lost. This is what fixes "dashboard is larger
    than the panel and gets cut off": the fix isn't a bigger capture_height
    (you don't know the dashboard's real rendered height in advance), it's
    capturing the page's actual full height and letting fit_mode do the
    scale-down.
    """
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
            page.screenshot(path=png_path, full_page=full_page)
        finally:
            browser.close()
