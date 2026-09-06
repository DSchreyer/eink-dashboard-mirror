# Changelog

## 0.6.0

- Preview/refresh/log errors now give a plain-language reason (unreachable
  dashboard URL, timeout, or bad panel size) instead of a raw Playwright
  or Python exception string.
- A failed config persist (e.g. an insufficient Supervisor role) is now
  logged, and Delete surfaces the same "not persisted to disk" warning
  Save already did — previously a failed persist on Delete was silent.
  The warning is now visually distinct (amber) from an actual failure
  (red), since the edit itself did succeed, just not durably yet.
- Added a Troubleshooting section to the add-on README, and a more
  actionable comment in `config.yaml` on testing whether `hassio_role`
  can be narrowed from `manager` to `default` on your own Supervisor.
- Companion firmware change (see `firmware/CHANGELOG.md`): the add-on now
  writes each device's `interval_minutes` to a small sidecar file next to
  its `.bin`, so a battery-mode panel's sleep duration can track the
  add-on's configured interval without a reflash.

## Earlier versions

Not tracked in this file — see the repository's commit history.
