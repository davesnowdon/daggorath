#!/usr/bin/env python3
"""Build a CBM 1581/MEGA65 .d81 disk image holding PRG files.

usage: make_d81.py <out.d81> <DISKNAME> <file.prg> <PRGNAME> [more pairs]

The image is written from scratch: header at 40/0, BAM at 40/1-2,
directory chain from 40/3, files filling tracks sequentially from
track 1.  After writing, the image is re-parsed (directory walked,
chains followed, contents compared) as a self-check.
"""
import sys

TRACKS, SECTORS, SSIZE = 80, 40, 256
DIRTRACK = 40


def lba(t, s):
    return ((t - 1) * SECTORS + s) * SSIZE


def petscii_pad(name, n=16):
    b = name.upper().encode("ascii", "replace")[:n]
    return b + b"\xa0" * (n - len(b))


class D81:
    def __init__(self, diskname):
        self.img = bytearray(TRACKS * SECTORS * SSIZE)
        self.used = set()          # (t, s) allocated
        self.entries = []          # (petscii name, start t, start s, blocks)
        self.diskname = diskname
        for s in (0, 1, 2, 3):
            self.used.add((DIRTRACK, s))

    def next_free(self):
        for t in list(range(1, DIRTRACK)) + list(range(DIRTRACK + 1,
                                                       TRACKS + 1)):
            for s in range(SECTORS):
                if (t, s) not in self.used:
                    return t, s
        raise SystemExit("disk full")

    def add_prg(self, data, name):
        first = cur = self.next_free()
        self.used.add(cur)
        blocks = 0
        pos = 0
        while True:
            chunk = data[pos:pos + 254]
            pos += len(chunk)
            blocks += 1
            off = lba(*cur)
            self.img[off + 2:off + 2 + len(chunk)] = chunk
            if pos >= len(data):
                self.img[off] = 0
                self.img[off + 1] = 1 + len(chunk)   # last byte used
                break
            nxt = self.next_free()
            self.used.add(nxt)
            self.img[off], self.img[off + 1] = nxt
            cur = nxt
        self.entries.append((petscii_pad(name), first[0], first[1], blocks))

    def _write_bam(self):
        did = b"DG"
        for half, sec in ((0, 1), (1, 2)):
            off = lba(DIRTRACK, sec)
            self.img[off + 0] = DIRTRACK if half == 0 else 0
            self.img[off + 1] = 2 if half == 0 else 0xFF
            self.img[off + 2] = 0x44                  # 'D'
            self.img[off + 3] = 0xBB                  # ~'D'
            self.img[off + 4:off + 6] = did
            self.img[off + 6] = 0xC0
            for i in range(40):
                t = half * 40 + i + 1
                free = [s for s in range(SECTORS)
                        if (t, s) not in self.used]
                e = off + 0x10 + i * 6
                self.img[e] = len(free)
                bits = 0
                for s in free:
                    bits |= 1 << s
                self.img[e + 1:e + 6] = bits.to_bytes(5, "little")

    def _write_header_dir(self):
        off = lba(DIRTRACK, 0)
        self.img[off + 0], self.img[off + 1] = DIRTRACK, 3
        self.img[off + 2] = 0x44
        self.img[off + 4:off + 20] = petscii_pad(self.diskname)
        self.img[off + 20:off + 22] = b"\xa0\xa0"
        self.img[off + 22:off + 24] = b"DG"
        self.img[off + 24] = 0xA0
        self.img[off + 25:off + 27] = b"3D"
        self.img[off + 27:off + 29] = b"\xa0\xa0"
        # directory chain (8 entries per sector)
        need = (len(self.entries) + 7) // 8
        secs = [3 + i for i in range(need)]
        for si, sec in enumerate(secs):
            off = lba(DIRTRACK, sec)
            self.used.add((DIRTRACK, sec))
            if si + 1 < len(secs):
                self.img[off], self.img[off + 1] = DIRTRACK, secs[si + 1]
            else:
                self.img[off], self.img[off + 1] = 0, 0xFF
            for ei in range(8):
                gi = si * 8 + ei
                if gi >= len(self.entries):
                    break
                name, t, s, blocks = self.entries[gi]
                e = off + ei * 32
                self.img[e + 2] = 0x82                # PRG, closed
                self.img[e + 3], self.img[e + 4] = t, s
                self.img[e + 5:e + 21] = name
                self.img[e + 30] = blocks & 0xFF
                self.img[e + 31] = blocks >> 8

    def write(self, path):
        self._write_header_dir()
        self._write_bam()
        with open(path, "wb") as f:
            f.write(self.img)


def verify(path, want):
    img = open(path, "rb").read()
    t, s = DIRTRACK, 3
    files = {}
    while t:
        off = lba(t, s)
        for ei in range(8):
            e = off + ei * 32
            if img[e + 2] != 0x82:
                continue
            name = img[e + 5:e + 21].rstrip(b"\xa0").decode("ascii")
            ft, fs = img[e + 3], img[e + 4]
            data = bytearray()
            while True:
                foff = lba(ft, fs)
                nt, ns = img[foff], img[foff + 1]
                if nt == 0:
                    data += img[foff + 2:foff + 1 + ns]
                    break
                data += img[foff + 2:foff + 256]
                ft, fs = nt, ns
            files[name] = bytes(data)
        t, s = img[off], img[off + 1]
        if t == 0:
            break
    for name, data in want.items():
        if files.get(name) != data:
            raise SystemExit(f"VERIFY FAILED for {name}")
    print(f"{path}: {len(files)} file(s), verify OK")


def main():
    out, diskname = sys.argv[1], sys.argv[2]
    pairs = list(zip(sys.argv[3::2], sys.argv[4::2]))
    d = D81(diskname)
    want = {}
    for path, name in pairs:
        data = open(path, "rb").read()
        d.add_prg(data, name)
        want[name.upper()] = data
    d.write(out)
    verify(out, want)


if __name__ == "__main__":
    main()
