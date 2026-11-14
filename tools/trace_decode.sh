#!/usr/bin/env bash
# trace_decode.sh -- decode a ring dump with CSP4CMSIS's trace tools (lib/csp4cmsis-trace/tools/trace).
#
#   tools/trace_decode.sh [ring.bin] [elf] [out dir]
#   defaults: trace_out/ring.bin, Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf, trace_out
#
# Writes into <out dir>:
#   events.log        ring2log.py: the records as an Event Recorder log
#   ring_report.txt   ring2log.py: header, records kept/lost, torn records, time span
#   events.csv        decode.py: one row per event (time, component, event, thread, value)
#   summary.txt       decode.py: counts per event and thread
#   checks.json       decode.py: library state checks (buffered channel replay, ALT guards)
#   stable.csv        decode.py: stable points
#   trace.json        decode.py: Perfetto trace -- open it in https://ui.perfetto.dev; if <ring>.names
#                     exists (tools/trace_dump.sh), threads carry their process names (trace_names.py)
# then, if ProB is available (PROBCLI, or probcli on the PATH):
#   conf/             csp_trace.py: conformance check of the trace against the CSP models
#
# decode.py needs eventlist (CMSIS-Toolbox 1.2) for full formatting; without it (EVENTLIST unset
# and no eventlist on the PATH) this script uses decode.py --no-eventlist, which supports the
# FreeRTOS .scvd used here.
set -euo pipefail
cd "$(dirname "$0")/.."
. tools/toolchain.sh                    # arm-none-eabi-nm (decode.py) from STM32CubeIDE if not on the PATH

RING=${1:-trace_out/ring.bin}
ELF=${2:-Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf}
OUT=${3:-trace_out}
T=lib/csp4cmsis-trace/tools/trace
SCVD=lib/csp4cmsis-trace/trace/FreeRTOS_ring.scvd
mkdir -p "$OUT"

python3 -I "$T/ring2log.py" "$RING" -o "$OUT/events.log" --report "$OUT/ring_report.txt"

EL=()
if [ -z "${EVENTLIST:-}" ] && ! command -v eventlist >/dev/null 2>&1; then
  EL=(--no-eventlist)
  echo "== eventlist not found: decode.py --no-eventlist"
fi
python3 -I "$T/decode.py" -a "$ELF" -I "$SCVD" "${EL[@]}" "$OUT/events.log" \
  -o "$OUT/events.csv" --summary "$OUT/summary.txt" --checks "$OUT/checks.json" \
  --stable "$OUT/stable.csv" --perfetto "$OUT/trace.json"
NAMES=${RING%.bin}.names
if [ -s "$NAMES" ]; then
  python3 -I tools/trace_names.py apply "$NAMES" "$OUT/trace.json" "$ELF"
fi
echo "== decoded: $OUT/events.csv, $OUT/trace.json (open in https://ui.perfetto.dev)"

PROB=${PROBCLI:-$(command -v probcli || true)}
if [ -n "$PROB" ] && [ -x "$PROB" ]; then
  PROBCLI="$PROB" python3 -I "$T/csp_trace.py" "$OUT/events.csv" --stable "$OUT/stable.csv" --out "$OUT/conf" --mutate
  PROBCLI="$PROB" python3 -I "$T/csp_trace.py" --prove "$OUT/conf"
else
  echo "== ProB not found (set PROBCLI): conformance check (csp_trace.py) skipped"
fi
