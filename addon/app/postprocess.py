"""Raw browser screenshot -> the panel's fixed resolution, in either pure
black/white (color_mode "bw") or 4-level grayscale (color_mode "gray4").
No dithering: dashboard UI (text, lines, icons, card edges) is flat
content, and dithering makes flat content look noisy rather than crisp --
matches what today's driver bring-up confirmed the panel actually renders
cleanly (see the main repo's README).

fit_mode controls how a screenshot whose size doesn't already match the
panel's fixed output size gets reconciled to it:
  - "letterbox" (default): preserve the full screenshot, scaled down/up by
    a single factor so nothing is cropped, centered on a white canvas.
    This is the right choice when the capture size (e.g. matching a real
    tablet's screen) doesn't share the panel's aspect ratio -- the whole
    dashboard stays visible instead of losing its edges.
  - "crop": center-crop to the panel's aspect ratio, then scale to fill
    exactly -- no white borders, but content outside the matched aspect
    ratio is discarded. This was the only behavior before fit_mode existed
    (via ImageOps.fit); kept as an explicit option, not the default.
  - "stretch": plain resize to the panel's exact dimensions, ignoring
    aspect ratio. Distorts the image if the aspect ratios differ.

color_mode "gray4" quantizes to 4 fixed, evenly-spaced levels rather than
a single black/white threshold -- there's no equivalent "threshold" knob
for 4 levels here; keeping the bucket edges fixed (not configurable) is a
deliberate simplicity choice, not a limitation of the format. The 4
values (0xFF/0xC0/0x80/0x00) match Waveshare's own GRAY1..GRAY4 constants
exactly, since pack.py's pack_gray4() depends on seeing precisely these
four values to build the correct wire format for the panel.
"""
from PIL import Image, ImageOps

FIT_MODES = ("letterbox", "crop", "stretch")
COLOR_MODES = ("bw", "gray4")

# Waveshare's own GRAY1 (white) .. GRAY4 (black) constants -- pack_gray4()
# in pack.py depends on the quantized image containing exactly these four
# values, nothing in between.
GRAY4_LEVELS = (0xFF, 0xC0, 0x80, 0x00)


def _fit(gray: Image.Image, width: int, height: int, fit_mode: str) -> Image.Image:
    if fit_mode == "crop":
        return ImageOps.fit(gray, (width, height), method=Image.LANCZOS)
    if fit_mode == "stretch":
        return gray.resize((width, height), Image.LANCZOS)
    if fit_mode != "letterbox":
        raise ValueError(f"unknown fit_mode: {fit_mode!r} (expected one of {FIT_MODES})")

    # letterbox: single scale factor for both axes, so proportions are
    # preserved -- never crops, never distorts. Resized while still
    # grayscale ("L"), not on the final packed image: LANCZOS on a
    # already-quantized/1-bit image produces garbage, since there's no
    # intermediate gray value left to interpolate through.
    scale = min(width / gray.width, height / gray.height)
    new_w = max(1, round(gray.width * scale))
    new_h = max(1, round(gray.height * scale))
    resized = gray.resize((new_w, new_h), Image.LANCZOS)
    canvas = Image.new("L", (width, height), color=255)  # white
    canvas.paste(resized, ((width - new_w) // 2, (height - new_h) // 2))
    return canvas


def _quantize_gray4(gray: Image.Image) -> Image.Image:
    """Maps every pixel to the nearest of the 4 fixed GRAY4_LEVELS. Stays
    mode "L" (not "1" -- PIL's 1-bit mode can't hold 4 values); pack_gray4()
    is what turns this into the panel's actual 2-bit-per-pixel wire format.
    """
    lut = []
    for p in range(256):
        nearest = min(GRAY4_LEVELS, key=lambda level: abs(level - p))
        lut.append(nearest)
    return gray.point(lut, mode="L")


def process(
    screenshot_path: str,
    panel_width: int,
    panel_height: int,
    threshold: int,
    invert: bool = True,
    fit_mode: str = "letterbox",
    color_mode: str = "bw",
) -> Image.Image:
    if color_mode not in COLOR_MODES:
        raise ValueError(f"unknown color_mode: {color_mode!r} (expected one of {COLOR_MODES})")

    img = Image.open(screenshot_path)
    gray = img.convert("L")

    if gray.size != (panel_width, panel_height):
        gray = _fit(gray, panel_width, panel_height, fit_mode)

    gray = ImageOps.autocontrast(gray)
    if invert:
        # HA's default frontend theme is dark (dark background, light
        # text/icons) -- thresholded as-is, that's mostly-solid-black on
        # the panel: technically fine (a large OTP-mode black fill is
        # confirmed clean), but not the conventional paper-like e-ink
        # look, and it means a much bigger black fill refreshing every
        # cycle. Inverting gives the usual white-background/black-text
        # result regardless of which HA theme is active. Set invert=False
        # if your dashboard already uses a light theme.
        gray = ImageOps.invert(gray)

    if color_mode == "gray4":
        return _quantize_gray4(gray)

    bw = gray.point(lambda p: 255 if p > threshold else 0, mode="L").convert("1")
    return bw
