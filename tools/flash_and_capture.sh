#!/usr/bin/env bash
# flash_and_capture.sh -- flash an ELF to the B-L475E-IOT01A and record its console output.
#
#   tools/flash_and_capture.sh <elf> <seconds> <output.txt>
#   e.g. tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 60 docs/results/run.txt
#
# Steps: flash and verify over SWD (ST-LINK), start listening on the ST-LINK virtual COM port
# (115200 8N1), reset the board (so the log starts at boot), record for <seconds>, then read
# two heap counters over SWD while the program keeps running. The output file starts with a
# header (date, commit, tools, commands) followed by the console output exactly as received.
#
# Needs: STM32_Programmer_CLI (STM32CubeProgrammer; on the PATH or set STM32_PROGRAMMER_CLI),
# arm-none-eabi-nm, stty, and read access to the serial port (group "dialout").
# Tested on Ubuntu. On Windows use STM32CubeIDE (Run) and a serial terminal instead (docs/03).
set -euo pipefail

ELF=${1:?usage: $0 <elf> <seconds> <output.txt>}
SECONDS_TO_CAPTURE=${2:?usage: $0 <elf> <seconds> <output.txt>}
OUT=${3:?usage: $0 <elf> <seconds> <output.txt>}

CLI=${STM32_PROGRAMMER_CLI:-$(command -v STM32_Programmer_CLI || echo "$HOME/st/STM32CubeProgrammer/bin/STM32_Programmer_CLI")}
[ -x "$CLI" ] || { echo "STM32_Programmer_CLI not found (set STM32_PROGRAMMER_CLI)" >&2; exit 1; }

# The ST-LINK/V2-1 virtual COM port; by-id names do not change when other USB serial devices are plugged in.
PORT=${VCP_PORT:-$(ls /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_*-if02 2>/dev/null | head -1 || true)}
[ -n "$PORT" ] || { echo "No ST-LINK virtual COM port found (is the board connected by its ST-LINK USB port, CN7?)" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

FLASH_CMD=("$CLI" -c port=SWD -w "$ELF" -v)
RESET_CMD=("$CLI" -c port=SWD -hardRst)

echo "== flashing $ELF"
"${FLASH_CMD[@]}" > "$LOG.flash" 2>&1 || { cat "$LOG.flash"; exit 1; }
grep -E "File download complete|Download verified successfully" "$LOG.flash" | sed 's/\x1b\[[0-9;]*m//g'

echo "== listening on $(readlink -f "$PORT") for $SECONDS_TO_CAPTURE s"
stty -F "$PORT" 115200 cs8 -cstopb -parenb raw -echo
timeout "$SECONDS_TO_CAPTURE" cat "$PORT" > "$LOG" &
CAT_PID=$!
sleep 0.5
"${RESET_CMD[@]}" > "$LOG.reset" 2>&1 || { cat "$LOG.reset"; exit 1; }
wait "$CAT_PID" || true     # timeout ends cat with status 124

# Heap check (program still running; "mode=HOTPLUG" attaches without halting or resetting).
read_word() {   # symbol -> 32-bit value at its address, or "n/a"
  local addr
  addr=$(arm-none-eabi-nm "$ELF" | awk -v s="$1" '$3 == s {print $1}')
  [ -n "$addr" ] || { echo "n/a"; return; }
  "$CLI" -c port=SWD mode=HOTPLUG -r32 "0x$addr" 4 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' \
    | awk -v a="0x$addr" 'toupper($1) == toupper(a) && $2 == ":" {print "0x" $3}' | head -1   # "0x20001C48 : 00000000"
}
SBRK=$(read_word __sbrk_heap_end)
FREERTOS_MIN=$(read_word xMinimumEverFreeBytesRemaining)

{
  echo "# Console capture"
  echo "# date:      $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
  echo "# commit:    $(git -C "$(dirname "$0")/.." describe --always --dirty 2>/dev/null || echo unknown)"
  echo "# elf:       $ELF (sha256 $(sha256sum "$ELF" | cut -c1-16)...)"
  echo "# compiler:  $(arm-none-eabi-readelf -p .comment "$ELF" | sed -n 's/.*\(GCC: .*\)/\1/p' | head -1)"
  echo "# tool:      $("$CLI" --version 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -i 'version' | head -1 | xargs)"
  echo "# flash:     STM32_Programmer_CLI ${FLASH_CMD[*]:1}"
  echo "# reset:     STM32_Programmer_CLI ${RESET_CMD[*]:1}"
  echo "# probe:     ST-LINK firmware $("$CLI" -l stlink 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | awk -F': ' '/ST-LINK FW/ {print $2; exit}')"
  echo "# serial:    ST-LINK virtual COM port ($(readlink -f "$PORT")), 115200 8N1, captured for $SECONDS_TO_CAPTURE s from before the reset"
  echo "# after the capture, read over SWD (program running):"
  echo "#   __sbrk_heap_end                = $SBRK   (0x00000000: malloc() was never called)"
  echo "#   xMinimumEverFreeBytesRemaining = $FREERTOS_MIN   (0x00000000: the FreeRTOS heap was never used)"
  echo "# ----------------------------------------------------------------------------------------"
  tr -d '\r' < "$LOG"
} > "$OUT"
rm -f "$LOG.flash" "$LOG.reset"

echo "== written $OUT ($(grep -vc '^#' "$OUT") lines of console output)"
