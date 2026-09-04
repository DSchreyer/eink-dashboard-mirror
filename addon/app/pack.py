"""Packs a PIL mode-'1' image into the exact byte format the raw ESP32
firmware's Epd::DisplayFrame() expects -- MUST match convert_image.py /
gen_test_image.py in the waveshare-7in5-v2-esp32-fix driver repo exactly,
since this is the other end of the same wire protocol:

  - 1 bit per pixel, MSB first, 8 pixels per byte
  - packed row-major (width/8 bytes per row)
  - bit = 1 -> white pixel, bit = 0 -> black pixel
"""


def pack(img) -> bytes:
    w, h = img.size
    assert w % 8 == 0, "width must be a multiple of 8"
    px = img.load()
    out = bytearray(w * h // 8)
    for y in range(h):
        row_base = y * (w // 8)
        for xbyte in range(w // 8):
            b = 0
            for bit in range(8):
                x = xbyte * 8 + bit
                if px[x, y]:
                    b |= 0x80 >> bit
            out[row_base + xbyte] = b
    return bytes(out)


def pack_gray4(img) -> bytes:
    """Packs a 4-level-grayscale image (mode 'L', every pixel one of
    postprocess.GRAY4_LEVELS -- 0xFF/0xC0/0x80/0x00, Waveshare's own
    GRAY1..GRAY4) into the 2-bit-per-pixel wire format the firmware's
    Epd::DisplayFrame4Gray() expects.

    Ported directly from Waveshare's own Python reference
    (getbuffer_4Gray() in their epd7in5_V2.py), including its specific
    value remap before masking (0xC0 -> 0x80, 0x80 -> 0x40) -- this
    remap is what makes the extracted top-2-bits come out as
    white=0b11, GRAY2=0b10, GRAY3=0b01, black=0b00, which is the exact
    plane-bit mapping DisplayFrame4Gray()'s own port of Waveshare's C
    reference decodes. Getting this remap wrong silently produces the
    wrong gray level rather than an error, so don't simplify it away.

    4 horizontally-adjacent pixels pack into one byte, MSB-first
    (leftmost of the 4 in bits 7:6, rightmost in bits 1:0) -- width/4
    bytes per row, packed row-major.
    """
    w, h = img.size
    assert w % 4 == 0, "width must be a multiple of 4"
    px = img.load()
    out = bytearray(w * h // 4)
    for y in range(h):
        row_base = y * (w // 4)
        for xbyte in range(w // 4):
            b = 0
            for i in range(4):
                x = xbyte * 4 + i
                v = px[x, y]
                if v == 0xC0:
                    v = 0x80
                elif v == 0x80:
                    v = 0x40
                b |= (v & 0xC0) >> (i * 2)
            out[row_base + xbyte] = b
    return bytes(out)
