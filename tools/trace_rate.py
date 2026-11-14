#!/usr/bin/env python3
"""trace_rate.py -- record rate over time in a CSP4CMSIS RAM trace ring dump (sizing the ring).

    tools/trace_rate.py trace_out/ring.bin [--bin 0.1] [--top 15]

Reads the dump that tools/trace_dump.sh writes (header + records, csp_trace_ring.h), orders the
records by write index, unwraps the DWT cycle-counter time stamps (records are less than one wrap
apart: the backend writes a heartbeat every second) and prints:
  - the time span and the average rate;
  - records per bin (default 0.1 s) as a coarse histogram, and the busiest 1 s window;
  - records per component/message (id), the most frequent first.
Component 0x51 is the RTOS (thread switches 0x5104/0x5105), 0xC0.. CSP4CMSIS (trace/CSP4CMSIS.scvd).
"""
import argparse
import struct
import sys
from collections import Counter

HDR = struct.Struct('<IHHIIIIII')     # magic, format, record_size, capacity, policy, write_count, clock_hz, dropped, dwt_ctrl
REC = struct.Struct('<IHHII')         # ts, id, seq, a, b


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('dump')
    ap.add_argument('--bin', type=float, default=0.1, help='histogram bin in seconds (default 0.1)')
    ap.add_argument('--top', type=int, default=15)
    a = ap.parse_args()
    data = open(a.dump, 'rb').read()
    magic, fmt, rsize, cap, policy, wcount, clock, dropped, _ = HDR.unpack_from(data, 0)
    if magic != 0x52505343 or rsize != 16:
        print('not a CSP4CMSIS ring dump (magic/record size)', file=sys.stderr)
        return 1
    n = min(wcount, cap)
    first = wcount - n
    recs = []
    for w in range(first, wcount):          # write index order = time order
        ts, rid, seq, va, vb = REC.unpack_from(data, 32 + 16 * (w % cap))
        recs.append((ts, rid, seq))
    t, last, acc = [], None, 0
    for ts, _, _ in recs:                    # unwrap 32-bit CYCCNT
        if last is not None and ts < last:
            acc += 1 << 32
        last = ts
        t.append((acc + ts) / clock)
    t0 = t[0]
    t = [x - t0 for x in t]
    span = t[-1] if t[-1] > 0 else 1e-9
    print(f'records: {n} of {wcount} written (capacity {cap}, policy {"stop" if policy else "overwrite"}, '
          f'dropped {dropped}); span {span:.3f} s; average {n / span:.0f} records/s')
    bins = Counter(int(x / a.bin) for x in t)
    peak = max(bins.values())
    print(f'records per {a.bin:g} s (peak {peak} = {peak / a.bin:.0f} records/s):')
    for b in range(0, int(span / a.bin) + 1):
        c = bins.get(b, 0)
        if c:
            print(f'  {b * a.bin:8.2f} s {c:5d} ' + '#' * max(1, round(60 * c / peak)))
    j, best, bstart = 0, 0, 0.0          # busiest 1 s window
    for i in range(len(t)):
        while t[i] - t[j] > 1.0:
            j += 1
        if i - j + 1 > best:
            best, bstart = i - j + 1, t[j]
    print(f'busiest 1 s window: {best} records from {bstart:.3f} s')
    ids = Counter(rid for _, rid, _ in recs)
    print('records per id (component << 8 | message):')
    for rid, c in ids.most_common(a.top):
        print(f'  0x{rid:04X} {c:6d}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
