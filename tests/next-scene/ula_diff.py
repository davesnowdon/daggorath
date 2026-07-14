#!/usr/bin/env python3
"""Diff a desktop 256x192 PGM against a raw 6144-byte ULA bitmap dump,
masking the heart glyph cells (status row, cols 15-16)."""
import sys


def ula_addr(y, xbyte):
    return (((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)) + xbyte


with open(sys.argv[1], "rb") as f:
    assert f.readline().strip() == b"P5"
    w, h = map(int, f.readline().split())
    maxv = int(f.readline())
    pix = f.read(w * h)

ula = bytearray(6144)
for y in range(192):
    for xb in range(32):
        b = 0
        for bit in range(8):
            if pix[y * 256 + xb * 8 + bit] > maxv // 2:
                b |= 0x80 >> bit
        ula[ula_addr(y, xb)] = b

nxt = open(sys.argv[2], "rb").read()
masked = {ula_addr(y, xb) for y in range(152, 159) for xb in (15, 16)}
diffs = [i for i in range(6144) if i not in masked and ula[i] != nxt[i]]
print(f"diff bytes (heart cells masked): {len(diffs)}")
for i in diffs[:20]:
    print(f"  off {i}: desktop {ula[i]:02X} next {nxt[i]:02X}")
sys.exit(1 if diffs else print("FIXED-SCENE PIXEL-IDENTICAL") or 0)
