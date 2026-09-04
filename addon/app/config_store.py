"""Shared, lock-guarded in-memory config for potentially several e-ink
devices, read/written by both the background loop thread and the web UI's
request handlers within the same process (see webui.py). This is what
lets Save take effect on the very next cycle without restarting the
add-on: there's no IPC or file polling between "the UI changed a
setting" and "the loop uses it" -- they're the same Python object.

Each device is a named, independent config (its own dashboard URL,
capture size, fit mode, etc.) and its own output file
(eink_dashboard_<name>.bin) and schedule -- so multiple physical panels
can each show something different. "Device" here means "one named
dashboard-capture profile, and by extension the physical panel whose
firmware was flashed to fetch that specific file" -- not the same thing
as PANEL_WIDTH/PANEL_HEIGHT in constants.py, which is the physical
Waveshare panel's fixed resolution, shared by all devices today.

Persistence to /data/options.json (so settings survive a real add-on
restart, and stay visible/consistent in HA's own Configuration tab) is a
separate concern, handled via the Supervisor API in supervisor_api.py --
if that call fails, the in-memory update still applies (so the live
editing/preview loop keeps working), but the failure is reported back to
the caller so the UI can surface it rather than silently claiming success.
"""
import json
import re
import threading
from typing import Optional

from constants import PANEL_HEIGHT, PANEL_WIDTH

OPTIONS_PATH = "/data/options.json"

DEVICE_DEFAULTS = {
    "dashboard_url": "http://YOUR_HA_HOST:8123/lovelace/your-dashboard",
    "capture_width": 800,
    "capture_height": 480,
    "fit_mode": "letterbox",
    "threshold": 160,
    "wait_seconds": 3,
    "invert": True,
    "interval_minutes": 10,
}

FIT_MODES = ("letterbox", "crop", "stretch")
NAME_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,31}$")


def _blank_status() -> dict:
    return {
        "last_run_at": None,
        "last_run_ok": None,
        "last_run_error": None,
        "next_run_at": None,
        "busy": False,
    }


def _load_persisted() -> dict:
    try:
        with open(OPTIONS_PATH) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def validate_name(name: str) -> Optional[str]:
    if not name or not NAME_RE.match(name):
        return (
            "device name must be lowercase letters, numbers, and hyphens only "
            "(e.g. 'kitchen', 'living-room'), starting with a letter or number, max 32 chars"
        )
    return None


def validate_device(opts: dict) -> Optional[str]:
    """Returns an error message if opts is invalid, else None. Checked
    before either the in-memory update or the persist call, so a bad
    value never reaches either."""
    try:
        cw, ch = int(opts["capture_width"]), int(opts["capture_height"])
        if cw < 1 or ch < 1:
            return "capture_width/capture_height must be positive"
        threshold = int(opts["threshold"])
        if not (0 <= threshold <= 255):
            return "threshold must be between 0 and 255"
        if opts.get("fit_mode") not in FIT_MODES:
            return f"fit_mode must be one of {FIT_MODES}"
        if int(opts["wait_seconds"]) < 0:
            return "wait_seconds must be >= 0"
        if int(opts["interval_minutes"]) < 5:
            return "interval_minutes must be >= 5"
        if not opts.get("dashboard_url", "").strip():
            return "dashboard_url is required"
    except (KeyError, TypeError, ValueError) as e:
        return f"invalid options: {e}"
    return None


class ConfigStore:
    def __init__(self):
        self._lock = threading.Lock()
        persisted = _load_persisted()
        devices = persisted.get("devices")
        if not devices:
            # First run, or upgrading from the pre-multi-device 0.2.0
            # options shape -- carry any old flat keys over into a single
            # "main" device rather than silently discarding them.
            legacy = {k: persisted[k] for k in DEVICE_DEFAULTS if k in persisted}
            devices = [{"name": "main", **DEVICE_DEFAULTS, **legacy}]
        self._devices = {d["name"]: {**DEVICE_DEFAULTS, **d} for d in devices}
        self._status = {name: _blank_status() for name in self._devices}

    def panel_size(self) -> tuple:
        return PANEL_WIDTH, PANEL_HEIGHT

    def list_names(self) -> list:
        with self._lock:
            return list(self._devices.keys())

    def get_device(self, name: str) -> Optional[dict]:
        with self._lock:
            d = self._devices.get(name)
            return dict(d) if d is not None else None

    def upsert_device_in_memory(self, name: str, opts: dict) -> None:
        with self._lock:
            existing = self._devices.get(name, {})
            self._devices[name] = {**DEVICE_DEFAULTS, **existing, **opts, "name": name}
            if name not in self._status:
                self._status[name] = _blank_status()

    def delete_device_in_memory(self, name: str) -> bool:
        with self._lock:
            if name not in self._devices:
                return False
            del self._devices[name]
            self._status.pop(name, None)
            return True

    def snapshot_for_persist(self) -> dict:
        """The full {"devices": [...]} shape written to options.json."""
        with self._lock:
            return {"devices": list(self._devices.values())}

    def get_status(self, name: str) -> Optional[dict]:
        with self._lock:
            s = self._status.get(name)
            return dict(s) if s is not None else None

    def set_status(self, name: str, **kwargs) -> None:
        with self._lock:
            if name in self._status:
                self._status[name].update(kwargs)

    def try_acquire_busy(self, name: str) -> bool:
        """Returns True and marks that device busy if nothing else is
        currently running a cycle/preview for it; False if something
        already is. A busy device doesn't block other devices' cycles.
        Callers MUST call release_busy(name) in a finally block if this
        returns True.
        """
        with self._lock:
            s = self._status.get(name)
            if s is None or s["busy"]:
                return False
            s["busy"] = True
            return True

    def release_busy(self, name: str) -> None:
        with self._lock:
            if name in self._status:
                self._status[name]["busy"] = False
