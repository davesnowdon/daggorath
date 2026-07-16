#!/usr/bin/env python3
"""parse_dave.py - decode the DAVE_STATE chunk of an ep128emu snapshot.

Dave's sound/interrupt registers are write-only on the hardware, so the
convention EXOS expects on restore (probe unknown f) can't be read by a
program on the machine.  The emulator's snapshot, however, stores the full
decoded Dave state (dave.cpp saveState, chunk 0x45508004, version
0x01000003), from which the effective A7h and B4h register values are
reconstructed here.

Usage: parse_dave.py <snapshot.ep128s|qs_ep128.dat>
"""
import sys
import struct

MAGIC = bytes([0x5D, 0x12, 0xE4, 0xF4, 0xC9, 0xDA, 0xB6, 0x42,
               0x01, 0x33, 0xDE, 0x07, 0xD2, 0x34, 0xF2, 0x22])
CHUNK_DAVE = 0x45508004


class R:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def u32(self):
        v = struct.unpack('>I', self.d[self.p:self.p + 4])[0]
        self.p += 4
        return v

    def flag(self):          # writeBoolean = one byte 0/1
        return self.u8() != 0


def find_dave_chunk(path):
    data = open(path, 'rb').read()
    if data[:16] != MAGIC:
        sys.exit(f'{path}: not an ep128emu snapshot')
    pos = 16
    while pos + 12 <= len(data):
        ctype, clen = struct.unpack('>II', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + clen]
        pos += 12 + clen
        if ctype == CHUNK_DAVE:
            return body
    sys.exit(f'{path}: no DAVE_STATE chunk')


def main(path):
    r = R(find_dave_chunk(path))
    version = r.u32()
    if not (0x01000000 <= version <= 0x01000003):
        sys.exit(f'unsupported DAVE_STATE version 0x{version:08X}')

    r.u8()                     # clockDiv
    r.u8()                     # clockCnt
    r.u8()                     # polycntVL selection
    for _ in range(5):
        r.u32()                # 4 polycnt phases + maxphase
    for _ in range(4):
        r.u8()                 # polycnt states
    for _ in range(4):
        r.u32()                # clk 62500/1000/50/1 phases
    r.u8()                     # clk_62500 state

    chn = []
    for _ in range(3):         # tone channels 0-2
        r.u8(), r.u8(), r.u8()             # state, prv, state1
        r.u32()                            # phase
        frq = r.u32()
        polysel = r.u8()
        hp = r.flag()
        rm = r.flag()
        run = r.flag()
        left = r.u8()
        right = r.u8()
        chn.append(dict(frq=frq, polysel=polysel, hp=hp, rm=rm, run=run,
                        left=left, right=right))
    # channel 3 (noise)
    r.u8(), r.u8(), r.u8(), r.u8()         # state, prv, state1, state2
    r.u8(), r.u8()                         # clk source, clk source prv
    r.flag()                               # noise_polycnt_is_7bit
    r.u8()                                 # input polycnt selection
    r.flag(), r.flag(), r.flag()           # lp, hp, rm
    n_left, n_right = r.u8(), r.u8()

    dac_l = r.flag()
    dac_r = r.flag()
    snd_sel = r.u8()           # 0=tone0 1=tone1 2=50Hz 3=1kHz
    en_snd = r.flag()
    en_1hz = r.flag()
    en_1 = r.flag()
    en_2 = r.flag()
    for _ in range(4):
        r.u8()                 # int states
    actives = [r.flag() for _ in range(4)]
    r.u32()                    # audioOutput
    pages = [r.u8() for _ in range(4)]

    sel_bits = {0: 0x40, 1: 0x60, 2: 0x20, 3: 0x00}[snd_sel]
    a7 = ((0 if chn[0]['run'] else 0x01)
          | (0 if chn[1]['run'] else 0x02)
          | (0 if chn[2]['run'] else 0x04)
          | (0x08 if dac_l else 0) | (0x10 if dac_r else 0)
          | sel_bits)
    b4_en = ((0x01 if en_snd else 0) | (0x04 if en_1hz else 0)
             | (0x10 if en_1 else 0) | (0x40 if en_2 else 0))

    sel_name = {0: 'tone0', 1: 'tone1', 2: '50Hz', 3: '1kHz'}[snd_sel]
    print(f'{path}:')
    print(f'  A7h (sound/int control) = 0x{a7:02X}   '
          f'[int source {sel_name}, DAC L={int(dac_l)} R={int(dac_r)}, '
          f'sync bits {a7 & 7:03b}]')
    print(f'  B4h enable bits         = 0x{b4_en:02X}   '
          f'[snd={int(en_snd)} 1Hz={int(en_1hz)} '
          f'INT1(video)={int(en_1)} INT2={int(en_2)}]')
    print(f'  latches active: snd={int(actives[0])} 1Hz={int(actives[1])} '
          f'INT1={int(actives[2])} INT2={int(actives[3])}')
    for i, c in enumerate(chn):
        print(f'  tone{i}: period={c["frq"]} poly={c["polysel"]} '
              f'run={int(c["run"])} vol L/R={c["left"]}/{c["right"]}')
    print(f'  noise vol L/R = {n_left}/{n_right}')
    print(f'  page regs B0-B3 = '
          + ' '.join(f'{p:02X}' for p in pages))


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
