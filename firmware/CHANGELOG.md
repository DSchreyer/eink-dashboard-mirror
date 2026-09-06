# Changelog

This firmware doesn't have its own version number (it's flashed directly
from source, not distributed as a package) — entries here are dated by
the paired add-on release that introduced them.

## Alongside addon 0.6.0

- Added a battery-power mode (`DEEP_SLEEP_ENABLED = true` in `main.cpp`):
  real deep sleep between fetch cycles instead of staying WiFi-associated
  continuously. Sleep duration is fetched fresh from the add-on each wake
  (its currently configured `interval_minutes` for this device) rather
  than a second hardcoded constant here, so changing the interval in the
  add-on's web UI takes effect on the panel's next wake with no reflash.
  See the README's "Battery power (deep sleep)" section.
