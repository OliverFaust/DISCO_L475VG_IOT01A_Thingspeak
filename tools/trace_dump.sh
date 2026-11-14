#!/usr/bin/env bash
# trace_dump.sh -- dump CSP4CMSIS's RAM trace ring from the running board (Debug-Trace build).
#
#   tools/trace_dump.sh [mode] [elf] [ring.bin]
#   defaults: Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf, trace_out/ring.bin
#
# Modes (docs/06-tracing.md, 6.3 and 6.6):
#   (none)               halt now and dump
#   --break LOC          run until the program reaches LOC (a function, or file:line), then dump
#     --if COND          ... only when the GDB expression COND is true there (conditional breakpoint)
#     --next             ... and execute that line before dumping (e.g. the trace call on it)
#     --reset            ... restarting the program first (for events at start-up)
#   --wait               run until the program halts itself -- B1 pressed, or trace_trigger()
#                        (Core/Inc/trace_trigger.h): a BKPT once the trigger's post_ms have passed
#     --press            ... and press B1 in software right after connecting (EXTI line 13
#                        software interrupt): the same path as a real press, for tests
#   --timeout S          for --break and --wait: give up after S seconds (default 300); no dump
#
# Steps (what you would do by hand in the STM32CubeIDE debugger, docs/06-tracing.md):
#  1. Start ST-LINK_gdbserver on port 61234 in attach mode (-g): no reset, the board keeps its state.
#  2. arm-none-eabi-gdb connects: connecting halts the core, so the ring stops changing.
#     --break: sets the breakpoint and continues until it is hit; --wait: continues until the core
#     halts at a BKPT. While GDB waits the board runs normally (a hardware breakpoint costs nothing).
#  3. GDB: dump binary memory <out> &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)
#     -- the header (32 bytes) and all records; then print the header for the record.
#  4. Still halted: the process names (pcTaskName) of the threads in the ring, into <ring>.names
#     (tools/trace_names.py; tools/trace_decode.sh puts them into trace.json).
#  5. GDB deletes its breakpoint (--wait: steps past the BKPT) and detaches: the core runs on. The
#     GDB server is stopped.
# Then decode with tools/trace_decode.sh.
#
# Needs STM32CubeIDE 2.1.0 (its ST-LINK_gdbserver, arm-none-eabi-gdb/-nm and STM32CubeProgrammer
# copy), found by tools/toolchain.sh; set CUBEIDE (and CUBEPROG, the programmer's bin folder) if they
# are elsewhere. Tested on Ubuntu.
set -euo pipefail
cd "$(dirname "$0")/.."

usage() { sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }
MODE=now BREAK= COND= NEXT=0 RESET=0 PRESS=0 TIMEOUT=300
while [ $# -gt 0 ]; do
  case "$1" in
    --break)   [ $# -ge 2 ] || usage; MODE=break; BREAK=$2; shift 2 ;;
    --if)      [ $# -ge 2 ] || usage; COND=$2; shift 2 ;;
    --next)    NEXT=1; shift ;;
    --reset)   RESET=1; shift ;;
    --wait)    MODE=wait; shift ;;
    --press)   PRESS=1; shift ;;
    --timeout) [ $# -ge 2 ] || usage; TIMEOUT=$2; shift 2 ;;
    -h|--help) usage ;;
    -*)        echo "unknown option $1" >&2; usage ;;
    *)         break ;;
  esac
done
[ -z "$COND" ] || [ "$MODE" = break ] || { echo "--if needs --break" >&2; exit 2; }
[ "$NEXT" = 0 ] || [ "$MODE" = break ] || { echo "--next needs --break" >&2; exit 2; }
[ "$RESET" = 0 ] || [ "$MODE" = break ] || { echo "--reset needs --break" >&2; exit 2; }
[ "$PRESS" = 0 ] || [ "$MODE" = wait ] || { echo "--press needs --wait" >&2; exit 2; }

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
if [ "$MODE" = wait ]; then
  arm-none-eabi-nm "$ELF" 2>/dev/null | grep " trace_trigger_tick$" >/dev/null || { echo "$ELF has no trace_trigger_tick: no trigger to wait for" >&2; exit 2; }
fi
mkdir -p "$(dirname "$OUT")"

NAMES=${OUT%.bin}.names
TMP=$(mktemp -d)
LOG=$TMP/server.log
"$GDBSERVER" -g -p "$PORT" -d -l 1 -cp "$CUBEPROG" > "$LOG" 2>&1 &
SERVER=$!
GDBPID=
trap '[ -z "$GDBPID" ] || kill $GDBPID 2>/dev/null || true; kill $SERVER 2>/dev/null || true; wait $SERVER 2>/dev/null || true; rm -rf "$TMP"' EXIT
for _ in $(seq 1 50); do grep -q "Waiting for debugger connection" "$LOG" && break; sleep 0.2; done
grep -q "Waiting for debugger connection" "$LOG" || { cat "$LOG" >&2; echo "GDB server did not start" >&2; exit 1; }

# The GDB script. $hit = 1 once the core has stopped where it should; only then is the ring dumped.
G=$TMP/dump.gdb
{
  echo "target remote localhost:$PORT"
  echo "set \$hit = 0"
  case "$MODE" in
    now)
      echo "set \$hit = 1" ;;
    break)
      if [ -n "$COND" ]; then echo "break $BREAK if $COND"; else echo "break $BREAK"; fi
      echo "commands"
      echo "  set \$hit = 1"
      echo "  shell touch $TMP/hit"
      echo "end"
      [ "$RESET" = 1 ] && echo "monitor reset"
      echo "printf \"== running until the breakpoint is hit (at most $TIMEOUT s)\\n\""
      echo "continue" ;;
    wait)
      if [ "$PRESS" = 1 ]; then
        echo "printf \"== B1 pressed in software (EXTI line 13)\\n\""
        echo "set *(volatile unsigned int *)0x40010410 = 0x2000"   # EXTI_SWIER1, line 13
      fi
      echo "printf \"== running until the program halts at its trigger (at most $TIMEOUT s)\\n\""
      echo "continue"
      echo "if (*(unsigned short *)\$pc & 0xff00) == 0xbe00"        # BKPT #imm (T1 encoding)
      echo "  set \$hit = 1"
      echo "  shell touch $TMP/hit"
      echo "  frame"
      echo "end" ;;
  esac
  echo "if \$hit == 1"
  [ "$NEXT" = 1 ] && echo "  next"
  echo "  printf \"halted at pc = %p, write_count = %u\\n\", \$pc, csp_trace_ring.header.write_count"
  echo "  dump binary memory $OUT &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)"
  echo "  print/x csp_trace_ring.header"
  echo "  shell python3 -I tools/trace_names.py gdb $OUT > $TMP/names.gdb"
  echo "  source $TMP/names.gdb"
  echo "  shell touch $TMP/dumped"
  echo "else"
  echo "  printf \"== not stopped by the breakpoint or trigger (timeout?): no dump\\n\""
  echo "end"
  [ "$MODE" = wait ] && echo "if \$hit == 1" && echo "  set \$pc = \$pc + 2" && echo "end"   # past the BKPT
  [ "$MODE" = break ] && echo "delete"
  echo "detach"
} > "$G"

"$GDB" -q -batch -x "$G" "$ELF" > "$TMP/gdb.out" 2>&1 &
GDBPID=$!
if [ "$MODE" != now ]; then
  # Watchdog: after TIMEOUT s without a hit, interrupt GDB; the script then skips the dump,
  # deletes the breakpoint and detaches.
  ( sleep "$TIMEOUT"; [ -e "$TMP/hit" ] || kill -INT $GDBPID 2>/dev/null || true ) &
  WATCH=$!
fi
tail -n +1 -f "$TMP/gdb.out" --pid=$GDBPID | grep --line-buffered -v "^warning: .*\(libthread\|auto-load\)" || true
wait $GDBPID || true
GDBPID=
[ -z "${WATCH:-}" ] || kill $WATCH 2>/dev/null || true

if grep -q "^Error in testing condition" "$TMP/gdb.out"; then
  # GDB stops when it cannot evaluate a condition, as if it were true: the stop is not the one asked for.
  echo "== the condition could not be evaluated (see the GDB error above); GDB stopped anyway." >&2
  echo "   $OUT (if written) is NOT the event you asked for. Fix --if and run again." >&2
  exit 1
fi
if [ -e "$TMP/dumped" ]; then
  grep "^TASKNAME " "$TMP/gdb.out" > "$NAMES" || true
  echo "== wrote $OUT ($(stat -c %s "$OUT") bytes) and $NAMES ($(wc -l < "$NAMES") names); the board is running again"
else
  echo "== no dump written; the board is running again" >&2
  exit 1
fi
