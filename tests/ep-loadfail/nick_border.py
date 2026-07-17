#!/usr/bin/env python3
"""nick_border.py <snapshot.ep128s> - print the Nick border colour byte.

Reads the NICK_STATE chunk (0x45508005) of an ep128emu quick snapshot;
borderColor sits at body offset 57 (layout verified against
nick.cpp saveState: 8B version+nLines, 3 booleans, colorMode, videoMode,
4 booleans, LM, RM, 16 palette bytes, 5 uint32s, displayEnabled,
currentSlot, then borderColor).
"""
import struct
import sys

data = open(sys.argv[1], 'rb').read()
pos = 16
while pos + 12 <= len(data):
    t, l = struct.unpack('>II', data[pos:pos + 8])
    body = data[pos + 8:pos + 8 + l]
    pos += 12 + l
    if t == 0x45508005:
        print(body[57])
        sys.exit(0)
print('no NICK_STATE chunk', file=sys.stderr)
sys.exit(2)
