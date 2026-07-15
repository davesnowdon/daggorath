#!/usr/bin/env python3
"""Build a 720 KB FAT12 floppy image holding files, no mtools/sudo needed.

usage: make_fat12.py <out.img> <HOSTFILE> <DOSNAME> [<HOSTFILE> <DOSNAME> ...]

Geometry is the canonical PC 720 KB 3.5" DD layout that EXDOS (and a
GoTek / real DD floppy) reads as standard FAT12:

    512 B/sector, 2 sectors/cluster, 1 reserved (boot) sector,
    2 FATs x 3 sectors, 112 root entries (7 sectors), media 0xF9,
    9 sectors/track, 2 heads, 1440 total sectors.

    sector 0        boot sector (BPB only; EP boots from ROM, PC boot
                    code is never executed - just needs a valid BPB)
    sectors 1..3    FAT 1
    sectors 4..6    FAT 2
    sectors 7..13   root directory (112 x 32 B)
    sectors 14..    data area; cluster 2 == sector 14

DOSNAME is an 8.3 name (e.g. HELLO.COM, EXDOS.INI); it is upper-cased
and space padded.  After writing, the image is re-parsed from its own
bytes (BPB read back, root walked, FAT chains followed, contents
compared) as a self-check - the same belt-and-braces approach as
make_d81.py.
"""
import struct
import sys

BYTES_PER_SEC = 512
SEC_PER_CLUS = 2
RESERVED = 1
NUM_FATS = 2
ROOT_ENTRIES = 112
TOTAL_SEC = 1440
MEDIA = 0xF9
SEC_PER_FAT = 3
SEC_PER_TRACK = 9
HEADS = 2

CLUS_BYTES = BYTES_PER_SEC * SEC_PER_CLUS          # 1024
ROOT_SECS = (ROOT_ENTRIES * 32) // BYTES_PER_SEC   # 7
FAT_START = RESERVED                               # sector 1
ROOT_START = RESERVED + NUM_FATS * SEC_PER_FAT     # sector 7
DATA_START = ROOT_START + ROOT_SECS                # sector 14
TOTAL_CLUS = (TOTAL_SEC - DATA_START) // SEC_PER_CLUS

# A fixed date/time stamp keeps images reproducible (no Date.now()).
# 2026-07-15 12:00:00 in FAT packed form.
FAT_DATE = ((2026 - 1980) << 9) | (7 << 5) | 15
FAT_TIME = (12 << 11) | (0 << 5) | 0


def boot_sector():
    b = bytearray(BYTES_PER_SEC)
    b[0:3] = bytes((0xEB, 0x3C, 0x90))          # JMP short + NOP
    b[3:11] = b"DAGGOR12"                        # OEM name (8 bytes)
    struct.pack_into("<H", b, 11, BYTES_PER_SEC)
    b[13] = SEC_PER_CLUS
    struct.pack_into("<H", b, 14, RESERVED)
    b[16] = NUM_FATS
    struct.pack_into("<H", b, 17, ROOT_ENTRIES)
    struct.pack_into("<H", b, 19, TOTAL_SEC)
    b[21] = MEDIA
    struct.pack_into("<H", b, 22, SEC_PER_FAT)
    struct.pack_into("<H", b, 24, SEC_PER_TRACK)
    struct.pack_into("<H", b, 26, HEADS)
    struct.pack_into("<I", b, 28, 0)             # hidden sectors
    struct.pack_into("<I", b, 32, 0)             # total sec 32 (0 -> use 16)
    b[36] = 0x00                                 # drive number
    b[38] = 0x29                                 # extended boot signature
    struct.pack_into("<I", b, 39, 0x12464147)    # volume id
    b[43:54] = b"DAGGORATH  "                    # volume label (11 bytes)
    b[54:62] = b"FAT12   "                        # fs type (8 bytes)
    b[510] = 0x55
    b[511] = 0xAA
    return b


def dos_name(name):
    name = name.upper()
    if "." in name:
        base, ext = name.split(".", 1)
    else:
        base, ext = name, ""
    if len(base) > 8 or len(ext) > 3 or not base:
        raise ValueError("not a valid 8.3 name: %r" % name)
    return base.ljust(8).encode("ascii") + ext.ljust(3).encode("ascii")


def pack_fat(chains_terminated):
    """chains_terminated: dict cluster -> next (0xFFF for EOF)."""
    entries = [0] * TOTAL_CLUS_TOTAL
    entries[0] = 0xF00 | MEDIA                    # media in low byte
    entries[1] = 0xFFF
    for c, nxt in chains_terminated.items():
        entries[c] = nxt
    fat = bytearray(SEC_PER_FAT * BYTES_PER_SEC)
    for i in range(0, TOTAL_CLUS_TOTAL - (TOTAL_CLUS_TOTAL % 2), 2):
        e0, e1 = entries[i] & 0xFFF, entries[i + 1] & 0xFFF
        off = (i // 2) * 3
        fat[off] = e0 & 0xFF
        fat[off + 1] = ((e0 >> 8) & 0x0F) | ((e1 & 0x0F) << 4)
        fat[off + 2] = (e1 >> 4) & 0xFF
    if TOTAL_CLUS_TOTAL % 2:                       # trailing odd entry
        i = TOTAL_CLUS_TOTAL - 1
        e0 = entries[i] & 0xFFF
        off = (i // 2) * 3
        fat[off] = e0 & 0xFF
        fat[off + 1] = (e0 >> 8) & 0x0F
    return fat


# FAT spans SEC_PER_FAT sectors -> 1024 12-bit entries fit in 1536 bytes.
TOTAL_CLUS_TOTAL = (SEC_PER_FAT * BYTES_PER_SEC * 2) // 3   # 1024


def build(out_path, pairs):
    img = bytearray(TOTAL_SEC * BYTES_PER_SEC)
    img[0:BYTES_PER_SEC] = boot_sector()

    fat_next = {}
    root = bytearray(ROOT_ENTRIES * 32)
    next_cluster = 2
    root_i = 0

    for host, dosname in pairs:
        data = open(host, "rb").read()
        nclus = max(1, (len(data) + CLUS_BYTES - 1) // CLUS_BYTES)
        if next_cluster + nclus - 2 > TOTAL_CLUS + 1:
            raise SystemExit("image full: %s needs %d clusters" %
                             (dosname, nclus))
        first = next_cluster
        for k in range(nclus):
            c = first + k
            nxt = 0xFFF if k == nclus - 1 else c + 1
            fat_next[c] = nxt
            sec = DATA_START + (c - 2) * SEC_PER_CLUS
            off = sec * BYTES_PER_SEC
            chunk = data[k * CLUS_BYTES:(k + 1) * CLUS_BYTES]
            img[off:off + len(chunk)] = chunk
        next_cluster += nclus

        e = root_i * 32
        root[e:e + 11] = dos_name(dosname)
        root[e + 11] = 0x20                       # attr = archive
        struct.pack_into("<H", root, e + 14, FAT_TIME)   # create time
        struct.pack_into("<H", root, e + 16, FAT_DATE)   # create date
        struct.pack_into("<H", root, e + 22, FAT_TIME)   # write time
        struct.pack_into("<H", root, e + 24, FAT_DATE)   # write date
        struct.pack_into("<H", root, e + 26, first)      # first cluster
        struct.pack_into("<I", root, e + 28, len(data))  # size
        root_i += 1

    fat = pack_fat(fat_next)
    for n in range(NUM_FATS):
        off = (FAT_START + n * SEC_PER_FAT) * BYTES_PER_SEC
        img[off:off + len(fat)] = fat
    img[ROOT_START * BYTES_PER_SEC:
        ROOT_START * BYTES_PER_SEC + len(root)] = root

    open(out_path, "wb").write(img)
    return img


def verify(img, pairs):
    """Re-parse the image bytes and compare each file to its source."""
    assert img[510] == 0x55 and img[511] == 0xAA, "bad boot signature"
    assert struct.unpack_from("<H", img, 11)[0] == BYTES_PER_SEC
    assert img[13] == SEC_PER_CLUS and img[21] == MEDIA

    def read_fat(c):
        off = FAT_START * BYTES_PER_SEC + (c // 2) * 3
        if c & 1:
            return ((img[off + 1] >> 4) | (img[off + 2] << 4)) & 0xFFF
        return (img[off] | ((img[off + 1] & 0x0F) << 8)) & 0xFFF

    want = {dos_name(dn): open(h, "rb").read() for h, dn in pairs}
    seen = 0
    for i in range(ROOT_ENTRIES):
        e = ROOT_START * BYTES_PER_SEC + i * 32
        name = bytes(img[e:e + 11])
        if name[0] in (0x00, 0xE5):
            continue
        first = struct.unpack_from("<H", img, e + 26)[0]
        size = struct.unpack_from("<I", img, e + 28)[0]
        chain, c = [], first
        while 2 <= c < 0xFF0:
            chain.append(c)
            c = read_fat(c)
        blob = bytearray()
        for c in chain:
            sec = DATA_START + (c - 2) * SEC_PER_CLUS
            o = sec * BYTES_PER_SEC
            blob += img[o:o + CLUS_BYTES]
        blob = blob[:size]
        assert name in want, "unexpected file %r" % name
        assert bytes(blob) == want[name], "content mismatch for %r" % name
        seen += 1
    assert seen == len(pairs), "expected %d files, found %d" % (
        len(pairs), seen)


def main(argv):
    if len(argv) < 4 or len(argv) % 2 != 0:
        sys.exit(__doc__)
    out = argv[1]
    rest = argv[2:]
    pairs = [(rest[i], rest[i + 1]) for i in range(0, len(rest), 2)]
    img = build(out, pairs)
    verify(img, pairs)
    total = sum(len(open(h, "rb").read()) for h, _ in pairs)
    print("wrote %s: %d files, %d bytes, %d/%d clusters free" %
          (out, len(pairs), total,
           TOTAL_CLUS - sum(max(1, (len(open(h, 'rb').read()) +
                            CLUS_BYTES - 1) // CLUS_BYTES) for h, _ in pairs),
           TOTAL_CLUS))
    print("self-check OK (re-parsed image matches sources)")


if __name__ == "__main__":
    main(sys.argv)
