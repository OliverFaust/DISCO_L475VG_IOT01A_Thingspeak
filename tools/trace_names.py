#!/usr/bin/env python3
"""trace_names.py -- process names for a ring dump whose start-up records were overwritten.

decode.py names a thread after its process's name() (CSProcess::name(): "Sensors", "Console",
"Uplink"), taken from the ProcessName record that CSP4CMSIS writes once, when the process starts.
In an overwrite ring that holds an upload, those records are long gone, and decode.py falls back
to the symbol of the thread's control block ("MainApp_Task(void*)::sensors"). The names are still
on the board: CSP4CMSIS's Run passes name() to osThreadNew, and FreeRTOS keeps it in the control
block's pcTaskName. This script fetches them at dump time and puts them into trace.json:

  tools/trace_names.py gdb ring.bin
      GDB commands (used by tools/trace_dump.sh while the core is halted): for every control
      block in the ring's thread records (component 0x51), print "TASKNAME <address> <pcTaskName>".

  tools/trace_names.py apply ring.names trace.json ELF
      In trace.json (decode.py --perfetto), replace each thread's symbol name with its
      pcTaskName: the thread groups, and the same object name in slice and flow labels (e.g.
      "waiting on ALT MainApp_Task(void*)::console" -> "waiting on ALT Console": the ALT object
      is a member of the Console process). events.csv and summary.txt are left as decoded.

Nothing is changed on the board or in CSP4CMSIS.
"""
import json
import os
import re
import struct
import sys

HDR = struct.Struct('<IHHIIIIII')     # csp_trace_ring.h: magic, format, record_size, capacity, policy, write_count, ...
REC = struct.Struct('<IHHII')         # ts, id, seq, a, b
THREAD_IDS = (0x5102, 0x5104, 0x5105)  # TaskCreate, TaskSwitchedIn, TaskSwitchedOut: a = control block
RAM = (0x20000000, 0x20018000)         # SRAM1 of the STM32L475VG (control blocks are statically allocated there)


def tcbs(ring):
    data = open(ring, 'rb').read()
    magic, _, rsize, cap, _, wcount, _, _, _ = HDR.unpack_from(data, 0)
    if magic != 0x52505343 or rsize != 16:
        sys.exit('%s: not a CSP4CMSIS ring dump' % ring)
    found = set()
    for w in range(max(0, wcount - cap), wcount):
        _, rid, _, a, _ = REC.unpack_from(data, 32 + 16 * (w % cap))
        if rid in THREAD_IDS and RAM[0] <= a < RAM[1]:
            found.add(a)
    return sorted(found)


def cmd_gdb(ring):
    for a in tcbs(ring):
        print('printf "TASKNAME 0x%%08x %%s\\n", 0x%08x, ((TCB_t *)0x%08x)->pcTaskName' % (a, a))
    return 0


def cmd_apply(names_file, trace, elf):
    sys.dont_write_bytecode = True                      # no __pycache__ inside lib/csp4cmsis-trace
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', 'lib', 'csp4cmsis-trace', 'tools', 'trace'))
    import decode                                       # CSP4CMSIS's decoder: the same symbol names
    syms = decode.Symbols(elf, os.environ.get('NM', 'arm-none-eabi-nm'))
    rename = {}
    for line in open(names_file):
        m = re.match(r'TASKNAME (0x[0-9a-fA-F]+) (.*\S)\s*$', line)
        if not m:
            continue
        sym = syms.short(int(m.group(1), 16))
        if sym and sym != m.group(2):
            rename[sym] = m.group(2)
    if not rename:
        print('trace_names: no names to apply')
        return 0
    pattern = re.compile('(?:' + '|'.join(re.escape(s) for s in sorted(rename, key=len, reverse=True)) + r')(?![\w.])')

    def fix(v):
        if isinstance(v, str):
            return pattern.sub(lambda m: rename[m.group(0)], v)
        if isinstance(v, list):
            return [fix(x) for x in v]
        if isinstance(v, dict):
            return {k: fix(x) for k, x in v.items()}
        return v

    d = json.load(open(trace))
    d = fix(d)
    with open(trace, 'w') as f:
        json.dump(d, f)
    for s, n in sorted(rename.items(), key=lambda x: x[1]):
        print('trace_names: %s -> %s' % (s, n))
    return 0


def main():
    if len(sys.argv) == 3 and sys.argv[1] == 'gdb':
        return cmd_gdb(sys.argv[2])
    if len(sys.argv) == 5 and sys.argv[1] == 'apply':
        return cmd_apply(*sys.argv[2:])
    sys.exit(__doc__)


if __name__ == '__main__':
    sys.exit(main())
