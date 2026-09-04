"""Rendering + the background scheduler that drives it, once per device,
each on its own interval.

render() is pure (no file writes, no status updates) so it's shared by
three call sites with different side-effect needs: the scheduler itself,
POST /api/devices/<name>/refresh (an on-demand real cycle, writes the
real output files), and POST /api/devices/<name>/preview (an ad hoc
render from whatever's currently in the UI's form, writes nothing -- see
webui.py).
"""
import concurrent.futures
import datetime
import os
import sys
import tempfile
import time
import traceback

from pack import pack, pack_gray4
from postprocess import process
from screenshot import screenshot

WWW_DIR = "/config/www"
SCHEDULER_TICK_SECONDS = 30  # how often to check which devices are due

# Wall-clock ceiling on one render, enforced independent of Playwright's own
# per-call timeouts (page.goto has one; browser.close() and the Playwright
# driver process itself do not). A wedged Chromium process there can hang
# render() forever -- and since callers release their busy-lock in a
# `finally` *after* render() returns, a hang here means it never does,
# which shows up as "Refresh does nothing, needs the add-on restarted."
# 60s is generous: page.goto's own timeout is 30s and a normal cycle
# finishes in single-digit seconds, so this only ever trips on a genuine
# hang, not a slow-but-working render.
RENDER_TIMEOUT_SECONDS = 60


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
    navigation, timeout, RENDER_TIMEOUT_SECONDS wall-clock ceiling, etc)
    -- callers decide how to report that.

    Runs the real work (_render_impl) in a worker thread with a hard
    timeout so this call always returns within RENDER_TIMEOUT_SECONDS no
    matter what happens inside Playwright -- see the constant's comment.
    shutdown(wait=False) is deliberate: if the timeout trips, the worker
    thread is abandoned rather than waited on (Python can't force-kill a
    thread), so the underlying Chromium process may leak until the
    container itself recycles it -- but the caller gets control back
    immediately either way, which is what actually matters: it's what
    lets the caller's `finally: release_busy()` run promptly instead of
    never running at all.
    """
    ex = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    future = ex.submit(_render_impl, opts, panel_width, panel_height)
    try:
        return future.result(timeout=RENDER_TIMEOUT_SECONDS)
    except concurrent.futures.TimeoutError:
        raise TimeoutError(
            f"render() exceeded {RENDER_TIMEOUT_SECONDS}s -- likely a stuck "
            "Chromium process; it may still be running in the background"
        ) from None
    finally:
        ex.shutdown(wait=False)


def _render_impl(opts: dict, panel_width: int, panel_height: int) -> tuple:
    with tempfile.TemporaryDirectory() as tmp:
        raw_png = os.path.join(tmp, "raw.png")
        screenshot(
            opts["dashboard_url"],
            raw_png,
            int(opts["capture_width"]),
            int(opts["capture_height"]),
            int(opts["wait_seconds"]),
            full_page=bool(opts.get("capture_full_page", False)),
        )
        bw = process(
            raw_png,
            panel_width,
            panel_height,
            int(opts["threshold"]),
            bool(opts["invert"]),
            opts.get("fit_mode", "letterbox"),
            color_mode=opts.get("color_mode", "bw"),
        )
        # pack_gray4() depends on process() having quantized to exactly
        # its 4 fixed gray levels (mode "L", not "1") -- see both
        # functions' own docstrings for why they're paired this way.
        packed = pack_gray4(bw) if opts.get("color_mode", "bw") == "gray4" else pack(bw)

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
    panel_width, panel_height = store.panel_size(name)
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
