#!/usr/bin/env python3
"""fat12.py <image> <NAME.EXT> <out> - extract one file from a FAT12 image.

Matches tools/make_fat12.py geometry (720K: 512B sectors, 2 sectors per
cluster, 1 reserved, 2x3-sector FATs, 112 root entries -> data at sector
14).  Exits 1 if the file is absent.
"""
import sys

img_path, want, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
d = open(img_path, 'rb').read()

name, _, ext = want.partition('.')
entry_name = name.ljust(8)[:8].encode() + ext.ljust(3)[:3].encode()

root_off = (1 + 6) * 512
for i in range(112):
    e = d[root_off + i * 32:root_off + (i + 1) * 32]
    if e[0] in (0, 0xE5):
        continue
    if e[:11] == entry_name:
        clus = e[26] | (e[27] << 8)
        size = int.from_bytes(e[28:32], 'little')
        break
else:
    sys.exit(1)


def fat(n):
    off = 512 + (n * 3) // 2
    v = d[off] | (d[off + 1] << 8)
    return (v >> 4) if n & 1 else (v & 0xFFF)


out = b''
while clus < 0xFF8 and len(out) < size:
    sec = 14 + (clus - 2) * 2
    out += d[sec * 512:(sec + 2) * 512]
    clus = fat(clus)
open(out_path, 'wb').write(out[:size])
print(f'{want}: {size} bytes')
