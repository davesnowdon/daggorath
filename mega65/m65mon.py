#!/usr/bin/env python3
"""xemu MEGA65 uartmon client: PRG injection and scripted control.

xemu must be running with -uartmon SOCKET.  Usage:

  m65mon.py SOCKET inject FILE.PRG   upload PRG, verify, jump to its
                                     BASIC-stub SYS address
  m65mon.py SOCKET cmd CMD [CMD...]  raw monitor commands, plus:
                                       @sleep:N     wait N seconds
                                       @key:XX[,..] press matrix scancode(s)
                                       @type:TEXT   type TEXT (\\r = RETURN)

Monitor crib: m/M ADDR (dump 16/256 bytes, 28-bit hex addr), s ADDR B..
(set memory), g ADDR (set PC), r (registers), ~exit (quit xemu, fires
-dumpscreen/-screenshot).  Keys are injected by writing the C65 matrix
scancode to $FFD3615 (press) and $7F (release) - $D615 takes scancodes,
NOT ascii.

The same protocol drives a real MEGA65 over its serial monitor, which
is how m65 -r works; this script is xemu-only because it speaks a unix
socket.
"""
import socket
import sys
import time

# C64/C65 keyboard matrix scancodes (row*8+col) for the keys the game uses
SCAN = {
    '\r': 0x01, ' ': 0x3C, '\x08': 0x00,  # RETURN, SPACE, INST/DEL
    '3': 0x08, 'W': 0x09, 'A': 0x0A, '4': 0x0B, 'Z': 0x0C, 'S': 0x0D,
    'E': 0x0E, '5': 0x10, 'R': 0x11, 'D': 0x12, '6': 0x13, 'C': 0x14,
    'F': 0x15, 'T': 0x16, 'X': 0x17, '7': 0x18, 'Y': 0x19, 'G': 0x1A,
    '8': 0x1B, 'B': 0x1C, 'H': 0x1D, 'U': 0x1E, 'V': 0x1F, '9': 0x20,
    'I': 0x21, 'J': 0x22, '0': 0x23, 'M': 0x24, 'K': 0x25, 'O': 0x26,
    'N': 0x27, 'P': 0x29, 'L': 0x2A, 'Q': 0x3E, '1': 0x38, '2': 0x3B,
}

BOOTSTRAP_ADDR = 0x0340   # tape buffer: LDX #$FF TXS JMP entry
IRQ_STUB_ADDR = 0x0350    # ack CIA1+VIC, then kernal-stub unwind + RTI


def read_reply(s, timeout=3.0):
    """Read until the monitor's '.' prompt."""
    s.settimeout(timeout)
    data = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
            if data.rstrip().endswith(b"."):
                break
    except socket.timeout:
        pass
    return data.decode("ascii", "replace")


def send(s, line):
    s.sendall(line.encode("ascii") + b"\r\n")
    return read_reply(s)


def inject_key(s, scancode):
    send(s, "sffd3615 %x" % scancode)
    time.sleep(0.10)
    send(s, "sffd3615 7f")


def prg_entry(payload, load):
    """Parse the SYS digits out of the BASIC stub."""
    i = 4                                   # skip line link + line number
    while i < len(payload) and payload[i] != 0x9E:   # SYS token
        i += 1
    i += 1
    while i < len(payload) and payload[i] == 0x20:   # skip spaces
        i += 1
    digits = ""
    while i < len(payload) and chr(payload[i]).isdigit():
        digits += chr(payload[i])
        i += 1
    if not digits:
        sys.exit("no SYS entry found in BASIC stub at $%04X" % load)
    return int(digits)


def inject(s, path):
    with open(path, "rb") as f:
        prg = f.read()
    load = prg[0] | (prg[1] << 8)
    payload = prg[2:]
    entry = prg_entry(payload, load)
    print("PRG: %d bytes at $%04X, entry $%04X" % (len(payload), load, entry))

    # A previous run's raster IRQ may still be live, with its $0314
    # vector pointing into the bytes about to be overwritten - park
    # the vector on a minimal handler first.  It must ACK the sources
    # (LDA $DC0D / $FF -> $D019) or the un-acked interrupt storms the
    # CPU into the ROM's IRQ entry the moment anything CLIs.
    send(s, "s%x ad 0d dc a9 ff 8d 19 d0 68 a8 68 aa 68 40" % IRQ_STUB_ADDR)
    send(s, "s0314 %02x %02x" % (IRQ_STUB_ADDR & 0xFF, IRQ_STUB_ADDR >> 8))

    for off in range(0, len(payload), 32):
        chunk = payload[off:off + 32]
        send(s, "s%x %s" % (load + off, " ".join("%02x" % b for b in chunk)))

    # verify first and last 16 bytes
    for off in (0, len(payload) - 16):
        want = payload[off:off + 16]
        got = send(s, "m%x" % (load + off))
        hexs = got.split(":")[-1].strip().rstrip(".").strip()
        if bytes.fromhex(hexs)[:16] != want:
            sys.exit("verify FAILED at $%04X:\n%s" % (load + off, got))
    print("verify OK")

    # Reset the hardware stack, then jump: LDX #$FF TXS JMP entry.
    # No SEI: llvm-mos's crt0 chain includes a charset-shift init that
    # calls the KERNAL's CHROUT, which needs the ROM IRQ alive (the
    # parked vector above keeps those IRQs harmless until plat_init
    # takes the machine over).
    boot = [0xA2, 0xFF, 0x9A, 0x4C, entry & 0xFF, entry >> 8]
    send(s, "s%x %s" % (BOOTSTRAP_ADDR, " ".join("%02x" % b for b in boot)))
    send(s, "g%x" % BOOTSTRAP_ADDR)
    print("running from $%04X" % entry)


def run_cmds(s, cmds):
    for c in cmds:
        if c.startswith("@sleep:"):
            time.sleep(float(c[7:]))
        elif c.startswith("@key:"):
            for part in c[5:].split(","):
                inject_key(s, int(part, 16))
                time.sleep(0.15)
        elif c.startswith("@type:"):
            for ch in c[6:].replace("\\r", "\r"):
                inject_key(s, SCAN[ch.upper()])
                time.sleep(0.15)
        else:
            print(send(s, c), end="")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sock_path, mode = sys.argv[1], sys.argv[2]
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock_path)
    read_reply(s, timeout=0.5)              # eat the banner, if any
    if mode == "inject":
        inject(s, sys.argv[3])
    elif mode == "cmd":
        run_cmds(s, sys.argv[3:])
    else:
        sys.exit(__doc__)
    s.close()


if __name__ == "__main__":
    main()
