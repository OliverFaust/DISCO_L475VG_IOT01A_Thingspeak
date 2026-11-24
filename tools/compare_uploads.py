#!/usr/bin/env python3
"""compare_uploads.py -- compare the uploads a device run reports with the ThingSpeak feed.

    tools/compare_uploads.py <console capture> <feed listing>

<console capture>: output of tools/flash_and_capture.sh (raw or masked); uses the Console's lines
    "[  t s] #<seq>  T = <t> C  RH = <h> %  p = <p> hPa" and
    "[  t s] Uplink: sample #<seq> -> entry <id> (...)"
<feed listing>: output of tools/thingspeak_feed.sh N, with N large enough to cover the run.

Checks, and prints a summary:
  - every entry id the device reports is in the feed, with field1..3 equal to the values the
    Console printed for that sample (2 decimal places);
  - the feed has no other entries between the first and the last reported entry id;
  - counts: uploads reported OK / REJECTED / FAILED by the device.
Exit status 0 if everything matches, 1 otherwise.
"""
import re
import sys

SAMPLE = re.compile(r'#(\d+)\s+T = (-?[\d.]+) C\s+RH = (-?[\d.]+) %\s+p = (-?[\d.]+) hPa')
UPLOAD = re.compile(r'Uplink: sample #(\d+) -> entry (\d+)')
REJECT = re.compile(r'Uplink: sample #\d+ REJECTED')
FAILED = re.compile(r'Uplink: sample #\d+ upload FAILED')
FEED = re.compile(r'^(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$')


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    samples, uploads, rejected, failed = {}, [], 0, 0
    for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
        if m := SAMPLE.search(line):
            samples[int(m.group(1))] = (m.group(2), m.group(3), m.group(4))
        if m := UPLOAD.search(line):
            uploads.append((int(m.group(1)), int(m.group(2))))
        rejected += bool(REJECT.search(line))
        failed += bool(FAILED.search(line))
    feed = {}
    for line in open(sys.argv[2], encoding='utf-8'):
        if m := FEED.match(line.strip()):
            feed[int(m.group(1))] = (m.group(3), m.group(4), m.group(5))

    ok = True
    print(f"device: {len(uploads)} uploads OK, {rejected} rejected, {failed} failed")
    if not uploads:
        print("no uploads in the capture")
        return 1
    ids = [e for _, e in uploads]
    lo, hi = min(ids), max(ids)
    in_window = sorted(e for e in feed if lo <= e <= hi)
    print(f"feed: {len(in_window)} entries with ids {lo}..{hi}")
    for seq, entry in uploads:
        if entry not in feed:
            print(f"  MISSING in feed: entry {entry} (sample #{seq})"); ok = False
        elif seq in samples and feed[entry] != samples[seq]:
            print(f"  DIFFERENT: entry {entry}: feed {feed[entry]} vs sample #{seq} {samples[seq]}"); ok = False
    extra = sorted(set(in_window) - set(ids))
    if extra:
        print(f"  entries in the feed not reported by the device: {extra}"); ok = False
    if len(set(ids)) != len(ids):
        print("  an entry id was reported twice"); ok = False
    print("RESULT:", "all uploads match the feed" if ok else "MISMATCH")
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
