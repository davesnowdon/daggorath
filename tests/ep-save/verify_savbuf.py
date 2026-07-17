#!/usr/bin/env python3
"""verify_savbuf.py <snapshot.ep128s> <savefile> <savbuf_addr>

Exit 0 iff the save file equals the in-RAM SAVBUF captured in an
ep128emu quick snapshot (MEMORY_STATE chunk 0x45508002; SAVBUF is a
Z80 page-1/2 address in the game's segments FC+).
"""
import struct
import sys

snap, savfile, addr = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
data = open(snap, 'rb').read()
pos = 16
segs = {}
while pos + 12 <= len(data):
    t, l = struct.unpack('>II', data[pos:pos + 8])
    body = data[pos + 8:pos + 8 + l]
    pos += 12 + l
    if t == 0x45508002:
        p = 8
        while p + 2 + 16384 <= len(body):
            s = body[p]
            p += 2
            segs[s] = body[p:p + 16384]
            p += 16384
sav = open(savfile, 'rb').read()
seg = 0xFC + (addr >> 14)
buf = segs[seg][addr & 0x3FFF:(addr & 0x3FFF) + len(sav)]
nd = sum(a != b for a, b in zip(sav, buf))
print(f'   save file vs SAVBUF: {nd} differing bytes of {len(sav)}')
sys.exit(0 if nd == 0 else 1)
