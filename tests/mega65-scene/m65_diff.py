#!/usr/bin/env python3
"""Diff a desktop 256x192 PGM against a raw 8000-byte MEGA65 bitmap dump
(C64 hires cell interleave, 256x192 window at cell column 4), masking
the heart glyph cells (status row, cols 15-16) and, optionally with
--mask-cursor, the prompt cursor cell (its blink phase is timing, not
rendering)."""
import sys


def m65_addr(y, xbyte):
    # window x offset = 4 cells = 32 bytes into each 320-byte cell row
    return ((y >> 3) * 320) + (y & 7) + 32 + (xbyte << 3)


args = [a for a in sys.argv[1:] if not a.startswith("--")]
mask_cursor = "--mask-cursor" in sys.argv

with open(args[0], "rb") as f:
    assert f.readline().strip() == b"P5"
    w, h = map(int, f.readline().split())
    maxv = int(f.readline())
    pix = f.read(w * h)

ref = bytearray(8000)
for y in range(192):
    for xb in range(32):
        b = 0
        for bit in range(8):
            if pix[y * 256 + xb * 8 + bit] > maxv // 2:
                b |= 0x80 >> bit
        ref[m65_addr(y, xb)] = b

m65 = open(args[1], "rb").read()
masked = {m65_addr(y, xb) for y in range(152, 159) for xb in (15, 16)}
if mask_cursor:
    # prompt cursor: text row 21 (pixel rows 168-174), cell col 1
    masked |= {m65_addr(y, 1) for y in range(168, 175)}
# bytes outside the 256-wide window (cells 0-3 and 36-39) are never
# drawn by the backend and stay zero; compare only the window
diffs = [i for i in range(8000)
         if i not in masked
         and 32 <= (i % 320) < 288
         and ref[i] != m65[i]]
print(f"diff bytes (heart cells masked): {len(diffs)}")
for i in diffs[:20]:
    print(f"  off {i}: desktop {ref[i]:02X} mega65 {m65[i]:02X}")
sys.exit(1 if diffs else print("FIXED-SCENE PIXEL-IDENTICAL") or 0)
