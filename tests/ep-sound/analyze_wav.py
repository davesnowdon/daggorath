#!/usr/bin/env python3
"""analyze_wav.py - verify the Enterprise port's Dave-DAC audio output.

Usage: analyze_wav.py <capture.wav> <raw-7350-dir>

The capture is an ep128emu sound.file recording of the game booting into
the attract demo.  The demo is deterministic and plays real samples
(title-fade BUZZ ramp, torch/step mechanics, creature sounds), so the
capture must (a) contain actual audio events, and (b) those events must
cross-correlate with the shipped 7350 Hz sample masters - proving the ISR
fetched, LUT-converted and DAC'd the right bytes at the right rate.

Method (numpy): fold to mono, resample to 7350 Hz, find energy events,
normalized-cross-correlate each event against every master (full-rate
form and the pair-averaged half-rate form re-expanded to wall-clock).
PASS if >= 3 events match some master with r >= 0.5.
"""
import os
import struct
import sys

import numpy as np

NAMES = [
    "00_squeak", "01_rattle", "02_growl", "03_beoop", "04_klank",
    "05_grawl", "06_pssst", "07_kklank", "08_pssht", "09_snarl",
    "0A_bdlbdl", "0B_bdlbdl", "0C_gluglg", "0D_phaser", "0E_whoop",
    "0F_clang", "10_whoosh", "11_chuck", "12_klink", "13_clank",
    "14_thud", "15_bang", "16_kaboom", "17_heart", "18_heart",
    "19_buzz",
]
RATE = 7350.0


def read_wav(path):
    """-> (rate, mono float array).  Tolerates a truncated/stale RIFF size
    (the emulator may be killed before finalizing the header)."""
    d = open(path, 'rb').read()
    assert d[:4] == b'RIFF' and d[8:12] == b'WAVE', 'not a WAV'
    pos = 12
    fmt = None
    while pos + 8 <= len(d):
        cid = d[pos:pos + 4]
        sz = struct.unpack('<I', d[pos + 4:pos + 8])[0]
        if cid == b'fmt ':
            fmt = d[pos + 8:pos + 8 + sz]
        elif cid == b'data':
            raw = d[pos + 8:]
            if 0 < sz <= len(raw):         # sz==0: header never finalized
                raw = raw[:sz]
            break
        pos += 8 + sz + (sz & 1)
    audio_fmt, nch, rate = struct.unpack('<HHI', fmt[:8])
    bits = struct.unpack('<H', fmt[14:16])[0]
    assert audio_fmt == 1, f'unsupported wav fmt {audio_fmt}'
    if bits == 16:
        a = np.frombuffer(raw[:len(raw) // (2 * nch) * 2 * nch],
                          dtype='<i2').astype(np.float32) / 32768.0
    elif bits == 8:
        a = (np.frombuffer(raw[:len(raw) // nch * nch],
                           dtype=np.uint8).astype(np.float32) - 128) / 128.0
    else:
        sys.exit(f'unsupported bit depth {bits}')
    return rate, a.reshape(-1, nch).mean(axis=1)


def xcorr_best(win, ref):
    """max normalized cross-correlation of ref sliding over win."""
    n = len(ref)
    if n > len(win):
        n = len(win)
        ref = ref[:n]
    ref = ref - ref.mean()
    re = np.sqrt((ref * ref).sum())
    if re < 1e-9:
        return 0.0
    ref = ref / re
    w = win - win.mean()
    num = np.correlate(w, ref, mode='valid')
    # sliding energy of w over each window of length n; windows quieter
    # than an absolute rms floor are excluded (0/0 blows the ratio up)
    c = np.concatenate(([0.0], np.cumsum(w * w)))
    en = np.sqrt(np.maximum(c[n:] - c[:-n], 0.0))
    valid = en > (0.01 * np.sqrt(n))
    if not np.any(valid):
        return 0.0
    return float(np.max(np.abs(num[valid]) / en[valid]))


def main():
    wav_path, raw_dir = sys.argv[1], sys.argv[2]
    rate, sig = read_wav(wav_path)
    print(f'{wav_path}: {len(sig)} samples @ {rate} Hz '
          f'({len(sig)/rate:.1f}s)')
    x_new = np.arange(int(len(sig) * RATE / rate)) * (rate / RATE)
    sig = np.interp(x_new, np.arange(len(sig)), sig).astype(np.float32)

    refs = {}
    for nm in NAMES:
        raw = np.frombuffer(
            open(os.path.join(raw_dir, nm + '.raw'), 'rb').read(),
            dtype=np.uint8).astype(np.float32)
        if len(raw) < 400:                 # hearts: too short to correlate
            continue
        full = (raw - 128) / 128.0
        refs[nm] = full[:4096]
        halfd = (raw[: len(raw) // 2 * 2].reshape(-1, 2).mean(axis=1)
                 - 128) / 128.0
        refs[nm + ':half'] = np.repeat(halfd, 2)[:4096]

    W = 512
    nwin = len(sig) // W
    energy = (sig[:nwin * W].reshape(-1, W) ** 2).sum(axis=1)
    floor = np.sort(energy)[nwin // 4] if nwin else 0.0
    thresh = max(floor * 20, 0.05)
    events = []
    i = 0
    while i < nwin:
        if energy[i] > thresh:
            j = i
            while j < nwin and (energy[j] > thresh * 0.2 or j - i < 4):
                j += 1
            events.append((i * W, min(j * W + 2048, len(sig))))
            i = j + 2
        else:
            i += 1
    print(f'{len(events)} audio events above threshold '
          f'(floor {floor:.4f}, thresh {thresh:.4f})')

    matched = 0
    for (s, e) in events[:16]:
        win = sig[s:e]
        if len(win) < 600:
            continue
        scores = sorted(((xcorr_best(win, ref[:2048]), nm)
                         for nm, ref in refs.items()), reverse=True)
        (best_r, best_nm), (r2, nm2) = scores[0], scores[1]
        t = s / RATE
        verdict = 'MATCH' if best_r >= 0.5 else 'weak'
        if best_r >= 0.5:
            matched += 1
        print(f'  event @{t:6.1f}s len {(e-s)/RATE:4.1f}s: '
              f'{best_nm} r={best_r:.2f} (then {nm2} r={r2:.2f}) [{verdict}]')

    if matched >= 3:
        print(f'EP-SOUND PASS ({matched} events matched sample masters)')
        return 0
    print('EP-SOUND FAIL (fewer than 3 events matched)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
