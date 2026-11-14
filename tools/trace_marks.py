#!/usr/bin/env python3
"""trace_marks.py -- show the trace trigger (Core/Src/trace_trigger.c) in Perfetto.

    tools/trace_marks.py events.csv trace.json

decode.py writes events of components it does not know as a CSV row only. This script takes the
rows of component "Trigger" (0x52, tools/trace_trigger.scvd) from events.csv and adds them to
trace.json (decode.py --perfetto) as a group "trigger" with one track:

  - an instant "armed (B1 / trace_trigger())" at each TriggerArmed -- written by the tick hook at
    its first tick after the press or call, so at most 1 ms after it,
  - an instant "halt (BKPT)" (or "fired, no debugger") at each TriggerFired,
  - a slice "post-trigger N ms" from TriggerArmed to its TriggerFired: everything inside it
    happened after the press, everything to its left before.

Without trigger rows the file is left unchanged. Used by tools/trace_decode.sh.
Nothing is changed on the board or in CSP4CMSIS.
"""
import csv
import json
import re
import sys

PID, TID = 6, 60000


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    csv_path, json_path = sys.argv[1:]
    rows = [r for r in csv.DictReader(open(csv_path)) if r['component'] == 'Trigger']
    if not rows:
        return
    trace = json.load(open(json_path))
    ev = trace['traceEvents']
    armed = None
    for r in rows:
        us = float(r['time_s']) * 1e6
        vals = dict(re.findall(r'(\w+)=(\d+)', r['value']))
        if r['event'] == 'TriggerArmed':
            armed = (us, int(vals.get('remaining_ms', 0)))
            ev.append({'ph': 'i', 's': 't', 'name': 'armed (B1 / trace_trigger())', 'pid': PID,
                       'tid': TID, 'ts': us, 'args': {'remaining_ms': armed[1]}})
        elif r['event'] == 'TriggerFired':
            dbg = vals.get('debugger') == '1'
            ev.append({'ph': 'i', 's': 't', 'name': 'halt (BKPT)' if dbg else 'fired, no debugger',
                       'pid': PID, 'tid': TID, 'ts': us,
                       'args': {'debugger': int(dbg), 'late_ms': int(vals.get('late_ms', 0))}})
            if armed is not None:
                ev.append({'ph': 'X', 'name': 'post-trigger %d ms' % armed[1],
                           'pid': PID, 'tid': TID + 1, 'ts': armed[0], 'dur': max(us - armed[0], 0.001),
                           'args': {'remaining_ms': armed[1], 'measured_ms': round((us - armed[0]) / 1e3, 3)}})
                armed = None
    ev.append({'ph': 'M', 'name': 'process_name', 'pid': PID, 'args': {'name': 'trigger'}})
    ev.append({'ph': 'M', 'name': 'thread_name', 'pid': PID, 'tid': TID, 'args': {'name': 'trigger'}})
    ev.append({'ph': 'M', 'name': 'thread_name', 'pid': PID, 'tid': TID + 1, 'args': {'name': 'post-trigger'}})
    ev.append({'ph': 'M', 'name': 'process_sort_index', 'pid': PID, 'args': {'sort_index': -1}})
    json.dump(trace, open(json_path, 'w'))
    print('trace_marks: %d trigger events added to %s' % (len(rows), json_path))


if __name__ == '__main__':
    sys.dont_write_bytecode = True
    main()
