"""Thin wrapper around the two Supervisor REST calls this add-on needs to
persist its own config. Isolated in its own file since the exact endpoint
path/payload shape is the least session-verified part of this feature --
if it needs correcting, this is the one file to fix.

Requires `hassio_api: true` in config.yaml, which injects SUPERVISOR_TOKEN
into the environment.
"""
import os

import requests

BASE = "http://supervisor/addons/self"
TOKEN = os.environ.get("SUPERVISOR_TOKEN", "")
HEADERS = {
    "Authorization": f"Bearer {TOKEN}",
    "Content-Type": "application/json",
}


class SupervisorError(Exception):
    pass


def set_self_options(options: dict) -> None:
    """Persists `options` as this add-on's own config, so it survives a
    real restart and stays visible in HA's Configuration tab. Raises
    SupervisorError on any failure -- callers should catch this and
    surface it to the user rather than silently losing the persist step
    (the in-memory config still applies either way; this is durability,
    not the live-editing path).
    """
    if not TOKEN:
        raise SupervisorError("SUPERVISOR_TOKEN not set -- is hassio_api: true in config.yaml?")
    try:
        r = requests.post(f"{BASE}/options", headers=HEADERS, json={"options": options}, timeout=10)
        r.raise_for_status()
        body = r.json()
        if body.get("result") != "ok":
            raise SupervisorError(f"Supervisor returned non-ok result: {body}")
    except requests.RequestException as e:
        raise SupervisorError(f"could not reach Supervisor API: {e}") from e
