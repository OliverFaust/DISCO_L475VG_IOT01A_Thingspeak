"""Minimal GDB remote stub on stdin/stdout: memory is zero except pcTaskName (TCB + 0x34)
of each TASKNAME line in the given names file. Only to test GDB's `source` of names.gdb."""
import sys, re
mem = {}
for line in open(sys.argv[1]):
    m = re.match(r'TASKNAME (0x[0-9a-f]+) (\S+)', line)
    if m:
        a = int(m.group(1), 16) + 0x34
        for i, c in enumerate(m.group(2).encode() + b'\0'):
            mem[a + i] = c
inp, out = sys.stdin.buffer, sys.stdout.buffer
def send(p):
    out.write(b'$' + p + b'#%02x' % (sum(p) & 0xff)); out.flush()
def recv():
    while True:
        c = inp.read(1)
        if not c: sys.exit(0)
        if c == b'$': break
    p = b''
    while (c := inp.read(1)) != b'#': p += c
    inp.read(2); out.write(b'+'); out.flush()
    return p
while True:
    p = recv()
    if p.startswith(b'qSupported'): send(b'PacketSize=4000')
    elif p == b'?': send(b'S05')
    elif p == b'g': send(b'00' * 4 * 16)
    elif p.startswith(b'm'):
        a, n = (int(x, 16) for x in p[1:].split(b','))
        send(bytes(mem.get(a + i, 0) for i in range(n)).hex().encode())
    elif p.startswith(b'H') or p.startswith(b'D'): send(b'OK')
    elif p == b'qAttached': send(b'1')
    elif p == b'k': sys.exit(0)
    else: send(b'')
