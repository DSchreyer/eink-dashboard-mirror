"""Entry point: runs the background scheduler as a daemon thread and
serves the config editor + API in the main thread. Single process so the
scheduler and the web UI share one in-memory ConfigStore -- see
config_store.py for why that's what lets Save apply without restarting
the add-on.
"""
import io
import os
import threading

from flask import Flask, jsonify, request, send_file, send_from_directory

import supervisor_api
from config_store import ConfigStore, validate_device, validate_name
from snapshot_loop import loop_forever, preview_path, render, run_cycle

app = Flask(__name__, static_folder="static", static_url_path="/static")
store = ConfigStore()


def _persist_all():
    """Best-effort persist of the full device list to /data/options.json.
    Returns (persisted: bool, warning: str | None) -- never raises, since
    the in-memory update this follows has already applied regardless.
    """
    try:
        supervisor_api.set_self_options(store.snapshot_for_persist())
        return True, None
    except supervisor_api.SupervisorError as e:
        # The caller's JSON response surfaces this as a "warning" field,
        # which is easy to miss in a UI toast -- also put it in the log,
        # since it means an edit just made is NOT actually durable (it'll
        # revert to whatever's in /data/options.json on the next add-on
        # restart) and that's worth a permanent record. A 403 here is the
        # most likely cause -- see config.yaml's hassio_role comment.
        print(f"[eink-dashboard-mirror] ERROR: failed to persist config: {e}", flush=True)
        return False, str(e)


@app.get("/")
def index():
    return send_from_directory(app.static_folder, "index.html")


@app.get("/api/devices")
def api_list_devices():
    # panel_size used to be one global value shown here; now it's a
    # per-device field (panel_width/panel_height, in each device's own
    # GET /api/devices/<name> response) since different devices can
    # target different panel hardware.
    return jsonify({"names": store.list_names()})


@app.get("/api/devices/<name>")
def api_get_device(name):
    device = store.get_device(name)
    if device is None:
        return jsonify({"error": f"no such device: {name}"}), 404
    return jsonify(device)


@app.post("/api/devices/<name>")
def api_upsert_device(name):
    """Create (if new) or update (if existing) a device, and persist the
    full device list. This is what the UI's Save button calls."""
    err = validate_name(name)
    if err:
        return jsonify({"ok": False, "error": err}), 400

    opts = request.get_json(force=True, silent=True) or {}
    existing = store.get_device(name) or {}
    merged = {**existing, **opts}
    err = validate_device(merged)
    if err:
        return jsonify({"ok": False, "error": err}), 400

    is_new = store.get_device(name) is None
    store.upsert_device_in_memory(name, merged)
    persisted, warning = _persist_all()

    body = {"ok": True, "persisted": persisted, "created": is_new}
    if warning:
        body["warning"] = warning
    return jsonify(body)


@app.delete("/api/devices/<name>")
def api_delete_device(name):
    if len(store.list_names()) <= 1:
        return jsonify({"ok": False, "error": "can't delete the last remaining device"}), 400
    if not store.delete_device_in_memory(name):
        return jsonify({"ok": False, "error": f"no such device: {name}"}), 404
    persisted, warning = _persist_all()
    body = {"ok": True, "persisted": persisted}
    if warning:
        body["warning"] = warning
    return jsonify(body)


@app.get("/api/devices/<name>/status")
def api_device_status(name):
    status = store.get_status(name)
    if status is None:
        return jsonify({"error": f"no such device: {name}"}), 404
    return jsonify(status)


@app.post("/api/devices/<name>/preview")
def api_device_preview(name):
    base = store.get_device(name) or {}
    opts = request.get_json(force=True, silent=True) or {}
    merged = {**base, **opts}
    err = validate_device(merged)
    if err:
        return jsonify({"error": err}), 400

    # Preview doesn't require an existing device (you can preview before
    # ever saving a new one). If the device already exists, take its
    # normal busy lock so this can't collide with its own scheduled
    # cycle; a brand-new, not-yet-saved device has no scheduled cycle to
    # collide with, so it skips locking entirely (accepting the minor,
    # low-stakes risk of two rapid double-clicks overlapping, rather than
    # building out locking for names that don't exist yet).
    already_exists = store.get_device(name) is not None
    if already_exists and not store.try_acquire_busy(name):
        return jsonify({"error": "another preview/refresh/scheduled run is in progress"}), 409
    try:
        # From `merged` (the in-flight form values), not store.panel_size(
        # name) -- same reasoning as every other field here: an unsaved
        # panel_width/height edit in the form should be what Preview
        # actually renders against, not whatever's still persisted.
        panel_width, panel_height = int(merged["panel_width"]), int(merged["panel_height"])
        packed, preview_png = render(merged, panel_width, panel_height)
    except Exception as e:
        return jsonify({"error": str(e)}), 502
    finally:
        if already_exists:
            store.release_busy(name)

    return send_file(io.BytesIO(preview_png), mimetype="image/png")


@app.post("/api/devices/<name>/refresh")
def api_device_refresh(name):
    if store.get_device(name) is None:
        return jsonify({"error": f"no such device: {name}"}), 404
    if not store.try_acquire_busy(name):
        return jsonify({"error": "another preview/refresh/scheduled run is in progress"}), 409
    try:
        run_cycle(store, name)
    finally:
        store.release_busy(name)

    status = store.get_status(name)
    if not status["last_run_ok"]:
        return jsonify({"ok": False, "error": status["last_run_error"]}), 502
    return jsonify({"ok": True})


@app.get("/api/devices/<name>/live-preview")
def api_device_live_preview(name):
    # Serves the actual file the panel last fetched -- kept inside this
    # add-on's own routes (rather than pointing the browser at HA's
    # /local/... URL) because a relative path from an ingress-proxied
    # page can't reliably escape to a sibling route outside the add-on's
    # own proxied path prefix.
    path = preview_path(name)
    if not os.path.exists(path):
        return jsonify({"error": "no preview written yet"}), 404
    return send_file(path, mimetype="image/png")


def main() -> None:
    threading.Thread(target=loop_forever, args=(store,), daemon=True).start()
    # threaded=True matters here: Werkzeug's dev server is single-threaded
    # by default, meaning it can only ever serve ONE request at a time. A
    # single slow or stuck render (a screenshot that takes a while, or a
    # Chromium process that hangs on close -- see screenshot.py's own
    # watchdog around that) would otherwise freeze *every* other request
    # behind it -- Refresh, Preview, even the UI's own status poll --
    # until the whole add-on was restarted. With threaded=True, one
    # slow/stuck device's request no longer blocks unrelated requests
    # (status polls, other devices' actions) -- it can only make that one
    # device's own busy-lock stay held, which is the correct, narrower
    # failure mode.
    app.run(host="0.0.0.0", port=8099, threaded=True)


if __name__ == "__main__":
    main()
