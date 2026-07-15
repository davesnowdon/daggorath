#!/usr/bin/env python3
"""compare_scene.py - byte-exact EP framebuffer vs desktop goldens.

Usage: compare_scene.py <snapshot.ep128s...> --goldens <dir>

Parses each ep128emu quick-snapshot (chunk stream: 16-byte magic, then
[type u32 BE][len u32 BE][data][crc u32]), finds the MEMORY_STATE chunk
(0x45508002: version u32 + pageTable[4] + repeated [seg][isROM][16K]),
takes segment 0xFF (the game's video segment), reads the LPT's visible-
record LD1 pointer (segment offset 0x3804/5 = Z80 0xF804/5) to learn
which of the two framebuffers is FRONT (a complete presented frame -
plat_present flips only after a full redraw), and extracts those 6144
linear 1bpp bytes.

Each desktop golden (P5 PGM, 256x192, written by the desktop shim's
--screenshot at a virtual jiffy, i.e. also a present-boundary frame) is
packed to the same linear 1bpp layout.  A snapshot PASSES if it equals
some golden byte-for-byte; the matched golden's jiffy number (from the
file name g<jiffy>.pgm) is reported so the caller can also check the
EP's jiffy RATE against wall-clock spacing.
"""
import sys
import os
import glob
import struct

MAGIC = bytes([0x5D, 0x12, 0xE4, 0xF4, 0xC9, 0xDA, 0xB6, 0x42,
               0x01, 0x33, 0xDE, 0x07, 0xD2, 0x34, 0xF2, 0x22])
CHUNK_MEMORY = 0x45508002
FB_BYTES = 6144


def parse_snapshot_fb(path, jiffy_addr=None):
    """-> (front_base, 6144 fb bytes, ep_jiffies or None).

    jiffy_addr is _isr_jiffies' Z80 address from the game's .map; the
    game's steady-state paging is P0=FC P1=FD P2=FE P3=FF, so a Z80
    address maps to segment 0xFC + (addr >> 14) at offset addr & 0x3FFF.
    """
    data = open(path, 'rb').read()
    if data[:16] != MAGIC:
        raise ValueError(f'{path}: not an ep128emu snapshot')
    pos = 16
    segs = {}
    while pos + 12 <= len(data):
        ctype, clen = struct.unpack('>II', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + clen]
        pos += 12 + clen
        if ctype != CHUNK_MEMORY:
            continue
        # version u32 + pageTable[4], then [seg][isROM][16384]*
        p = 8
        while p + 2 + 16384 <= len(body):
            seg = body[p]
            p += 2
            segs[seg] = body[p:p + 16384]
            p += 16384
    if 0xFF not in segs:
        raise ValueError(f'{path}: no RAM segment 0xFF in snapshot')
    seg_ff = segs[0xFF]
    ld1 = seg_ff[0x3804] | (seg_ff[0x3805] << 8)
    if ld1 not in (0xC000, 0xD800):
        raise ValueError(f'{path}: LPT LD1 = 0x{ld1:04X}, '
                         'expected 0xC000 or 0xD800')
    off = ld1 - 0xC000
    jiffies = None
    if jiffy_addr is not None:
        seg = 0xFC + (jiffy_addr >> 14)
        o = jiffy_addr & 0x3FFF
        if seg in segs:
            jiffies = segs[seg][o] | (segs[seg][o + 1] << 8)
    return ld1, bytes(seg_ff[off:off + FB_BYTES]), jiffies


def pack_pgm(path):
    """P5 256x192 PGM -> 6144 linear 1bpp bytes (bit 7 leftmost)."""
    raw = open(path, 'rb').read()
    if not raw.startswith(b'P5'):
        raise ValueError(f'{path}: not a P5 PGM')
    # header: P5 <ws> 256 <ws> 192 <ws> 255 <single ws> data
    fields, i, tok = [], 2, b''
    while len(fields) < 3:
        c = raw[i:i + 1]
        i += 1
        if c.isspace():
            if tok:
                fields.append(int(tok))
                tok = b''
        elif c == b'#':
            while raw[i:i + 1] != b'\n':
                i += 1
        else:
            tok += c
    w, h, maxv = fields
    if (w, h) != (256, 192):
        raise ValueError(f'{path}: {w}x{h}, expected 256x192')
    px = raw[i:i + w * h]
    out = bytearray(FB_BYTES)
    for y in range(192):
        for xb in range(32):
            b = 0
            base = y * 256 + xb * 8
            for k in range(8):
                if px[base + k] > (maxv // 2):
                    b |= 0x80 >> k
            out[y * 32 + xb] = b
    return bytes(out)


def main():
    args = sys.argv[1:]
    if '--goldens' not in args:
        sys.exit(__doc__)
    gi = args.index('--goldens')
    golden_dir = args[gi + 1]
    rest = args[:gi] + args[gi + 2:]
    jiffy_addr = None
    if '--jiffy-addr' in rest:
        ji = rest.index('--jiffy-addr')
        jiffy_addr = int(rest[ji + 1], 0)
        rest = rest[:ji] + rest[ji + 2:]
    snaps = rest

    goldens = {}          # packed bytes -> jiffy
    for g in sorted(glob.glob(os.path.join(golden_dir, 'g*.pgm'))):
        jiffy = int(os.path.basename(g)[1:-4])
        goldens.setdefault(pack_pgm(g), jiffy)
    if not goldens:
        sys.exit(f'no g<jiffy>.pgm goldens in {golden_dir}')
    print(f'{len(goldens)} unique golden frames loaded')

    matched = []
    for s in snaps:
        try:
            base, fb, epj = parse_snapshot_fb(s, jiffy_addr)
        except ValueError as e:
            print(f'  {os.path.basename(s)}: UNREADABLE ({e})')
            continue
        jtxt = f'EP jiffy {epj}' if epj is not None else 'EP jiffy n/a'
        j = goldens.get(fb)
        if j is not None:
            print(f'  {os.path.basename(s)}: IDENTICAL to desktop '
                  f'jiffy {j} ({jtxt}, front 0x{base:04X}, '
                  '0 differing bytes)')
            matched.append((s, j, epj))
        else:
            # nearest golden by differing-byte count, for diagnosis
            best = min(goldens.items(),
                       key=lambda kv: sum(a != b for a, b in zip(kv[0], fb)))
            nd = sum(a != b for a, b in zip(best[0], fb))
            print(f'  {os.path.basename(s)}: no exact match '
                  f'({jtxt}; nearest: desktop jiffy {best[1]}, '
                  f'{nd} differing bytes)')

    if len(matched) >= 2:
        print(f'EP-SCENE IDENTICAL ({len(matched)}/{len(snaps)} snapshots '
              'byte-exact against desktop goldens)')
        # stopwatch data for the caller: snapshot index -> EP jiffy
        if all(m[2] is not None for m in matched):
            print('EP_JIFFIES ' + ' '.join(
                f'{os.path.basename(s).split("_")[1].split(".")[0]}:{epj}'
                for s, _, epj in matched))
        return 0
    print('EP-SCENE FAILED (fewer than 2 byte-exact matches)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
