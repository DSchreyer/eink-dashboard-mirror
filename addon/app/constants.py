"""Physical panel output size -- fixed by the Waveshare 7.5" V2 hardware
and the ESP32 firmware's buffer size (wifi-firmware/src/main.cpp:
EPD_BUFFER_SIZE = 800*480/8 = 48000 bytes). NEVER user-editable: a mismatch
here silently breaks the firmware's fixed-size download/malloc/change-hash
logic on the other end of this same wire format (see pack.py).
"""
PANEL_WIDTH = 800
PANEL_HEIGHT = 480
