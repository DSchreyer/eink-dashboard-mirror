"""Rendering + the background scheduler that drives it, once per device,
each on its own interval.

render() is pure (no file writes, no status updates) so it's shared by
three call sites with different side-effect needs: the scheduler itself,
POST /api/devices/<name>/refresh (an on-demand real cycle, writes the
real output files), and POST /api/devices/<name>/preview (an ad hoc
render from whatever's currently in the UI's form, writes nothing -- see
webui.py).
"""
import datetime
import os
import sys
import tempfile
import time
import traceback

from pack import pack
from postprocess import process
from screenshot import screenshot

WWW_DIR = "/config/www"
SCHEDULER_TICK_SECONDS = 30  # how often to check which devices are due


def bin_path(name: str) -> str:
    return f"{WWW_DIR}/eink_dashboard_{name}.bin"


def preview_path(name: str) -> str:
    return f"{WWW_DIR}/eink_dashboard_{name}_preview.png"


def atomic_write(path: str, data: bytes) -> None:
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def render(opts: dict, panel_width: int, panel_height: int) -> tuple:
    """Screenshots opts["dashboard_url"] and converts it per opts. Returns
    (packed_bytes, preview_png_bytes). Raises on any failure (Playwright
    navigation, timeout, etc) -- callers decide how to report that.
    """
    with tempfile.TemporaryDirectory() as tmp:
        raw_png = os.path.join(tmp, "raw.png")
        screenshot(
            opts["dashboard_url"],
            raw_png,
            int(opts["capture_width"]),
            int(opts["capture_height"]),
            int(opts["wait_seconds"]),
        )
        bw = process(
            raw_png,
            panel_width,
            panel_height,
            int(opts["threshold"]),
            bool(opts["invert"]),
            opts.get("fit_mode", "letterbox"),
        )
        packed = pack(bw)

        preview_tmp = os.path.join(tmp, "preview.png")
        bw.save(preview_tmp, format="PNG")
        with open(preview_tmp, "rb") as f:
            preview_png = f.read()

    return packed, preview_png


def run_cycle(store, name: str) -> None:
    """A real cycle for one device, against the store's currently active
    config for it: writes that device's actual files, records status.
    Used by both the scheduler and POST /api/devices/<name>/refresh.
    """
    opts = store.get_device(name)
    if opts is None:
        print(f"[eink-dashboard-mirror] run_cycle: unknown device {name!r}, skipping", flush=True)
        return
    panel_width, panel_height = store.panel_size()
    try:
        packed, preview_png = render(opts, panel_width, panel_height)

        os.makedirs(WWW_DIR, exist_ok=True)
        atomic_write(bin_path(name), packed)
        atomic_write(preview_path(name), preview_png)

        store.set_status(
            name,
            last_run_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            last_run_ok=True,
            last_run_error=None,
        )
        print(
            f"[eink-dashboard-mirror] [{name}] wrote {bin_path(name)} ({len(packed)} bytes) "
            f"from {opts['dashboard_url']}",
            flush=True,
        )
    except Exception as e:
        store.set_status(
            name,
            last_run_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            last_run_ok=False,
            last_run_error=str(e),
        )
        print(f"[eink-dashboard-mirror] [{name}] cycle FAILED:", flush=True)
        traceback.print_exc(file=sys.stdout)


def _run_if_due(store, name: str, next_due: dict) -> None:
    now = time.monotonic()
    due_at = next_due.get(name, 0)  # 0 -> due immediately the first time we see this device
    if now < due_at:
        return

    if store.try_acquire_busy(name):
        try:
            run_cycle(store, name)
        finally:
            store.release_busy(name)
    else:
        print(f"[eink-dashboard-mirror] [{name}] scheduled cycle skipped -- busy with a preview/refresh", flush=True)
        return  # try again next tick rather than pushing the schedule out

    device = store.get_device(name)
    interval = max(60, int(device["interval_minutes"]) * 60) if device else 600
    next_due[name] = time.monotonic() + interval
    next_run_iso = (
        datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(seconds=interval)
    ).isoformat()
    store.set_status(name, next_run_at=next_run_iso)


def loop_forever(store) -> None:
    """The scheduled background loop -- runs as a daemon thread started
    by webui.py. Each device gets its own independent schedule (its own
    interval_minutes), tracked in next_due; a short tick just checks
    which ones are due rather than sleeping for one fixed interval, so
    devices with different intervals -- or a device added after
    startup -- are handled correctly without restarting the loop.
    """
    print("[eink-dashboard-mirror] scheduler starting", flush=True)
    next_due = {}
    while True:
        for name in store.list_names():
            _run_if_due(store, name, next_due)
        time.sleep(SCHEDULER_TICK_SECONDS)
