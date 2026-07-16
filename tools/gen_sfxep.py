#!/usr/bin/env python3
"""Pack the Enterprise SFX blobs: bin-packed, 128-aligned, two profiles.

The Enterprise plays PCM by feeding the Dave 6-bit DAC one byte per tone-0
interrupt (probe-verified rate = 250000/(n+1) Hz: n=33 -> 7352.94 Hz for the
7350 Hz masters, n=67 -> 3676.47 Hz for half-rate samples).  The loader
reads the blob into 16K segments allocated with EXOS fn 24 plus two fixed
tails (F8h above the loader, FEh above the game BSS/LUTs), and the ISR
fetches bytes through the Z80 page-1 window.

Constraints encoded here (matching isr_ep.asm / sound_ep.c / loader.s):
  * no sample may cross a bin (= segment window) boundary - the ISR walks a
    plain 16-bit pointer with no crossing logic;
  * every sample starts 128-aligned and occupies whole 128-byte blocks
    (padded with 0x80 = DAC midline): the ISR end-check is "pointer low
    byte hit a 128 boundary -> dec block counter" and the block counter is
    8-bit (max sample 32640 bytes; largest actual is buzz, 14700);
  * byte-identical samples are deduplicated (0A/0B bdlbdl);
  * half-rate data is pair-averaged from the 7350 Hz masters (crude but
    real low-pass, better than naive decimation).

Profiles:
  1 (stock EP128): 3 dynamic 16K bins + F8-tail (12K) + FE-tail bins.
    Transient-rich mechanics stay full-rate; long/soft sounds go half-rate;
    the DEMOTE list moves further sounds to half-rate until the pack fits.
  2 (expanded RAM): 8 dynamic 16K bins, everything full-rate.

usage: gen_sfxep.py <raw-7350-dir> <out-dir>
writes: <out-dir>/DAGGOR1.SFX, DAGGOR2.SFX, sfx_index_ep.h, sfx_bins.inc
"""
import os
import sys

NAMES = [
    "00_squeak", "01_rattle", "02_growl", "03_beoop", "04_klank",
    "05_grawl", "06_pssst", "07_kklank", "08_pssht", "09_snarl",
    "0A_bdlbdl", "0B_bdlbdl", "0C_gluglg", "0D_phaser", "0E_whoop",
    "0F_clang", "10_whoosh", "11_chuck", "12_klink", "13_clank",
    "14_thud", "15_bang", "16_kaboom", "17_heart", "18_heart",
    "19_buzz",
]

PERIOD_FULL = 33            # 250000/34 = 7352.94 Hz
PERIOD_HALF = 67            # 250000/68 = 3676.47 Hz
BLOCK = 128
PAD = 0x80                  # DAC midline = silence

# Stock profile: sounds that KEEP full rate (sharp transients where
# halving audibly dulls the attack).  Everything else starts half-rate.
FULL_RATE_STOCK = {
    "00_squeak", "04_klank", "06_pssst", "07_kklank", "08_pssht",
    "0F_clang", "10_whoosh", "11_chuck", "12_klink", "13_clank",
    "14_thud", "17_heart", "18_heart",
}
# Demoted to half-rate one at a time (in this order) if the pack is over
# budget.  Longest / least transient first.
DEMOTE_ORDER = ["0F_clang", "07_kklank", "13_clank", "04_klank"]

# Bin capacities and the (bin-code, window-base) each bin maps to at run
# time.  Codes 0..N-1 index the loader's dynamically allocated segment
# list; 0xF8/0xFE are the fixed tails.  Window base = Z80 address of the
# bin's first byte with its segment paged at 0x4000 (so F8's 0x1000
# in-segment offset -> 0x5000; FE's 0x1800 -> 0x5800).
BINS_STOCK = [
    (0x00, 0x4000, 16384),
    (0x01, 0x4000, 16384),
    (0x02, 0x4000, 16384),
    (0xF8, 0x5000, 12288),   # F8 seg offsets 0x1000-0x3FFF (above the loader)
    (0xFE, 0x5800,  8960),   # FE seg offsets 0x1800-0x3AFF (Z80 0x9800-0xBAFF:
]                            #   above the 9 volume LUTs, below the stack)
BINS_FULL = [(i, 0x4000, 16384) for i in range(8)]


def half_rate(data):
    n = len(data) // 2
    return bytes((data[2 * i] + data[2 * i + 1] + 1) // 2 for i in range(n))


def padded(data):
    rem = len(data) % BLOCK
    return data if rem == 0 else data + bytes([PAD]) * (BLOCK - rem)


def pack(uniq, bins):
    """First-fit-decreasing of unique samples into bins.
    uniq: {key: bytes}; returns {key: (bin_idx, offset)} or None if no fit."""
    order = sorted(uniq, key=lambda k: len(uniq[k]), reverse=True)
    used = [0] * len(bins)
    place = {}
    for key in order:
        size = len(uniq[key])
        for i, (_, _, cap) in enumerate(bins):
            if used[i] + size <= cap:
                place[key] = (i, used[i])
                used[i] += size
                break
        else:
            return None, None
    return place, used


def build_profile(raws, bins, half_set, demote):
    """-> (rows[26], bin_bytes list, half_set actually used)"""
    half_set = set(half_set)
    demote = list(demote)
    while True:
        uniq = {}               # (data-key) -> padded bytes
        keyof = {}              # sample name -> (data-key, period, blocks)
        for name in NAMES:
            data = raws[name]
            if name in half_set:
                data, period = half_rate(data), PERIOD_HALF
            else:
                period = PERIOD_FULL
            key = (hash(data), len(data), period)
            # hash collisions: verify by content
            while key in uniq and uniq[key] != padded(data):
                key = (key[0] + 1, key[1], key[2])
            uniq[key] = padded(data)
            keyof[name] = (key, period, (len(data) + BLOCK - 1) // BLOCK)
        place, used = pack(uniq, bins)
        if place is not None:
            break
        if not demote:
            sys.exit("gen_sfxep: profile does not fit and demote list is "
                     "exhausted")
        half_set.add(demote.pop(0))

    rows = []
    for name in NAMES:
        key, period, blocks = keyof[name]
        bin_idx, off = place[key]
        code, wbase, _ = bins[bin_idx]
        assert blocks <= 255, name
        rows.append(dict(name=name, code=code, waddr=wbase + off,
                         blocks=blocks, period=period))
    bin_bytes = []
    for i in range(len(bins)):
        buf = bytearray([PAD]) * 0
        buf = bytearray(used[i])
        for key, (b, off) in place.items():
            if b == i:
                buf[off:off + len(uniq[key])] = uniq[key]
        bin_bytes.append(bytes(buf))
    return rows, bin_bytes, half_set


def verify(rows, bin_bytes, raws, half_used, bins):
    """Re-read every sample from the packed bins and compare to source."""
    for row in rows:
        name = row["name"]
        want = raws[name]
        if name in half_used:
            want = half_rate(want)
        bin_idx = next(i for i, (c, w, _) in enumerate(bins)
                       if c == row["code"] and
                       w <= row["waddr"] < w + bins[i][2] + BLOCK)
        off = row["waddr"] - bins[bin_idx][1]
        got = bin_bytes[bin_idx][off:off + len(want)]
        assert got == want, f"verify failed: {name}"
        assert row["waddr"] % BLOCK == 0, f"misaligned: {name}"


def main():
    raw_dir, out_dir = sys.argv[1], sys.argv[2]
    raws = {n: open(os.path.join(raw_dir, n + ".raw"), "rb").read()
            for n in NAMES}

    rows1, bins1, half1 = build_profile(raws, BINS_STOCK,
                                        set(NAMES) - FULL_RATE_STOCK,
                                        DEMOTE_ORDER)
    rows2, bins2, half2 = build_profile(raws, BINS_FULL, set(), [])
    verify(rows1, bins1, raws, half1, BINS_STOCK)
    verify(rows2, bins2, raws, half2, BINS_FULL)

    with open(os.path.join(out_dir, "DAGGOR1.SFX"), "wb") as f:
        for b in bins1:
            f.write(b)
    with open(os.path.join(out_dir, "DAGGOR2.SFX"), "wb") as f:
        for b in bins2:
            f.write(b)

    with open(os.path.join(out_dir, "sfx_index_ep.h"), "w") as f:
        f.write("/* generated by tools/gen_sfxep.py - do not edit.\n"
                " * Enterprise SFX placement, indexed by sound_ids.h order.\n"
                " * bin: 0..N-1 = loader-allocated segment list index;\n"
                " *      0xF8/0xFE = fixed tail segments.\n"
                " * waddr: Z80 address with the segment paged at 0x4000.\n"
                " * blocks: 128-byte blocks; period: Dave tone-0 period. */\n")
        f.write("typedef struct {\n"
                "    uint8_t bin;\n"
                "    uint16_t waddr;\n"
                "    uint8_t blocks;\n"
                "    uint8_t period;\n"
                "} sfx_ep_row_t;\n\n")
        for pname, rows, nsegs in (("SFX1", rows1, 3), ("SFX2", rows2, 8)):
            f.write("#define %s_NSEGS %du\n" % (pname, nsegs))
            f.write("static const sfx_ep_row_t %s[SND_COUNT] = {\n" % pname)
            for r in rows:
                f.write("    { 0x%02X, 0x%04X, %3u, %2u },  /* %s%s */\n"
                        % (r["code"], r["waddr"], r["blocks"], r["period"],
                           r["name"],
                           " (half)" if r["period"] == PERIOD_HALF else ""))
            f.write("};\n\n")

    with open(os.path.join(out_dir, "sfx_bins.inc"), "w") as f:
        f.write("; generated by tools/gen_sfxep.py - do not edit.\n"
                "; per-profile bin byte counts, in file order, for the\n"
                "; loader's sequential EXOS read-block calls.\n")
        for pname, bb in (("SFX1", bins1), ("SFX2", bins2)):
            for i, b in enumerate(bb):
                f.write("%s_BIN%d EQU %d\n" % (pname, i, len(b)))
            f.write("%s_NBINS EQU %d\n" % (pname, len(bb)))

    tot1 = sum(len(b) for b in bins1)
    tot2 = sum(len(b) for b in bins2)
    extra = sorted(half1 - (set(NAMES) - FULL_RATE_STOCK))
    print(f"DAGGOR1.SFX (stock): {tot1} bytes in "
          f"{[len(b) for b in bins1]}")
    if extra:
        print(f"  demoted to half-rate to fit: {extra}")
    print(f"DAGGOR2.SFX (full):  {tot2} bytes in "
          f"{[len(b) for b in bins2]}")


if __name__ == "__main__":
    main()
