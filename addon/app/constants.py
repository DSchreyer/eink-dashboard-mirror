"""Default panel output size -- now a per-device *default*, not a fixed
global (see config_store.DEVICE_DEFAULTS / ConfigStore.panel_size(name)).
800x480 remains the default because it's this add-on's own proven panel
(the Waveshare 7.5" V2 this whole pipeline was built and tested against --
see wifi-firmware/), but any device can override panel_width/panel_height
to match different hardware.

Whatever value ends up configured for a device MUST still match that
device's own firmware buffer size exactly (see wifi-firmware/'s own docs
-- EPD_BUFFER_SIZE is panel_width*panel_height/8 for bw, /4 for gray4) --
a mismatch here silently breaks the firmware's fixed-size download/
malloc/change-hash logic on the other end of this same wire format (see
pack.py). That's a per-device pairing to get right when flashing a new
panel's firmware, not something this add-on can enforce from its own
side.
"""
PANEL_WIDTH = 800
PANEL_HEIGHT = 480
