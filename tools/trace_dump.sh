#!/usr/bin/env bash
# trace_dump.sh -- dump CSP4CMSIS's RAM trace ring from the running board (Debug-Trace build).
#
#   tools/trace_dump.sh [elf] [ring.bin]
#   defaults: Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf, trace_out/ring.bin
#
# Steps (what you would do by hand in the STM32CubeIDE debugger, docs/06-tracing.md):
#  1. Start ST-LINK_gdbserver on port 61234 in attach mode (-g): no reset, the board keeps its state.
#  2. arm-none-eabi-gdb connects: connecting halts the core, so the ring stops changing.
#  3. GDB: dump binary memory <out> &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)
#     -- the header (32 bytes) and all records; then print the header for the record.
#  4. Still halted: the process names (pcTaskName) of the threads in the ring, into <ring>.names
#     (tools/trace_names.py; tools/trace_decode.sh puts them into trace.json).
#  5. GDB detaches: the core runs on. The GDB server is stopped.
# Then decode with tools/trace_decode.sh.
#
# Needs STM32CubeIDE 2.1.0 (its ST-LINK_gdbserver, arm-none-eabi-gdb/-nm and STM32CubeProgrammer
# copy), found by tools/toolchain.sh; set CUBEIDE (and CUBEPROG, the programmer's bin folder) if they
# are elsewhere. Tested on Ubuntu.
set -euo pipefail
cd "$(dirname "$0")/.."

ELF=${1:-Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf}
OUT=${2:-trace_out/ring.bin}
. tools/toolchain.sh                    # CUBEIDE, STM32_PROGRAMMER_CLI, arm-none-eabi-nm on the PATH
CUBEPROG=${CUBEPROG:-$(dirname "${STM32_PROGRAMMER_CLI:-$HOME/st/STM32CubeProgrammer/bin/x}")}
PORT=${GDB_PORT:-61234}

GDBSERVER=$(ls "$CUBEIDE"/plugins/com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.*/tools/bin/ST-LINK_gdbserver 2>/dev/null | head -1)
GDB=$(ls "$CUBEIDE"/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/arm-none-eabi-gdb 2>/dev/null | head -1)
[ -x "$GDBSERVER" ] || { echo "ST-LINK_gdbserver not found under $CUBEIDE (set CUBEIDE)" >&2; exit 2; }
[ -x "$GDB" ]       || { echo "arm-none-eabi-gdb not found under $CUBEIDE (set CUBEIDE)" >&2; exit 2; }
[ -f "$ELF" ]       || { echo "$ELF not found: build the Debug-Trace configuration first" >&2; exit 2; }
arm-none-eabi-nm "$ELF" 2>/dev/null | grep " csp_trace_ring$" >/dev/null || { echo "$ELF has no csp_trace_ring: not a Debug-Trace build" >&2; exit 2; }
mkdir -p "$(dirname "$OUT")"

NAMES=${OUT%.bin}.names
LOG=$(mktemp)
CMDS=$(mktemp)
"$GDBSERVER" -g -p "$PORT" -d -l 1 -cp "$CUBEPROG" > "$LOG" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true; wait $SERVER 2>/dev/null || true; rm -f "$LOG" "$CMDS"' EXIT
for _ in $(seq 1 50); do grep -q "Waiting for debugger connection" "$LOG" && break; sleep 0.2; done
grep -q "Waiting for debugger connection" "$LOG" || { cat "$LOG" >&2; echo "GDB server did not start" >&2; exit 1; }

"$GDB" -q -batch "$ELF" \
  -ex "target remote localhost:$PORT" \
  -ex "printf \"halted at pc = %p, write_count = %u\\n\", \$pc, csp_trace_ring.header.write_count" \
  -ex "dump binary memory $OUT &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)" \
  -ex "print/x csp_trace_ring.header" \
  -ex "shell python3 -I tools/trace_names.py gdb $OUT > $CMDS" \
  -ex "source $CMDS" \
  -ex "detach" 2>&1 | grep -v "^warning: .*\(libthread\|auto-load\)" | tee "$LOG.gdb"
grep "^TASKNAME " "$LOG.gdb" > "$NAMES" || true
rm -f "$LOG.gdb"

echo "== wrote $OUT ($(stat -c %s "$OUT") bytes) and $NAMES ($(wc -l < "$NAMES") names); the board is running again"
