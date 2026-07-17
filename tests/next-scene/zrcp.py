#!/usr/bin/env python3
"""zrcp.py - minimal ZEsarUX remote protocol (ZRCP) client.

Shared by the next-scene / next-sound / next-save harnesses (imported or
run as a CLI).  Speaks the text protocol on --remoteprotocol-port
(default 10000): every reply ends with the "command> " prompt.

CLI:
  zrcp.py [--port N] cmd  '<raw command>'      print the reply
  zrcp.py [--port N] keys '<text>' [press_ms]  type text (\\n -> ENTER);
                                               send-keys-ascii holds each
                                               key press_ms + the same
                                               release gap, so budget
                                               2*press_ms per character
  zrcp.py [--port N] mem  ADDR LEN OUTFILE     binary memory dump
"""
import socket
import sys

PROMPT = b'command> '


class Zrcp:
    def __init__(self, host='127.0.0.1', port=10000, timeout=180):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b''
        self._read_to_prompt()          # consume the connect banner

    def _read_to_prompt(self):
        while not self.buf.rstrip(b' ').endswith(PROMPT.rstrip(b' ')):
            data = self.sock.recv(65536)
            if not data:
                raise ConnectionError('ZRCP connection closed')
            self.buf += data
        out = self.buf[:self.buf.rfind(PROMPT)]
        self.buf = b''
        return out.decode('latin1').strip()

    def cmd(self, line):
        self.sock.sendall(line.encode('latin1') + b'\n')
        return self._read_to_prompt()

    def keys(self, text, press_ms=80):
        """Type text through the emulated keyboard (13 = ENTER)."""
        codes = [13 if c == '\n' else ord(c) for c in text]
        return self.cmd('send-keys-ascii %d %s'
                        % (press_ms, ' '.join(str(c) for c in codes)))

    def mem(self, addr, length):
        """Read CPU-visible memory as bytes."""
        reply = self.cmd('read-memory %d %d' % (addr, length))
        hexstr = ''.join(reply.split())
        return bytes.fromhex(hexstr)


def main():
    args = sys.argv[1:]
    port = 10000
    if args and args[0] == '--port':
        port = int(args[1])
        args = args[2:]
    if not args:
        print(__doc__, file=sys.stderr)
        return 2
    z = Zrcp(port=port)
    if args[0] == 'cmd':
        print(z.cmd(args[1]))
    elif args[0] == 'keys':
        text = args[1].replace('\\n', '\n')
        z.keys(text, int(args[2]) if len(args) > 2 else 80)
    elif args[0] == 'mem':
        data = z.mem(int(args[1], 0), int(args[2], 0))
        with open(args[3], 'wb') as f:
            f.write(data)
        print('%d bytes -> %s' % (len(data), args[3]))
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
