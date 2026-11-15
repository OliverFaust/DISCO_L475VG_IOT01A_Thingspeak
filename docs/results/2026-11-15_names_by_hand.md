# Process names by hand in a CubeIDE debug session: docs/06 6.3 step 8 (2026-11-15)

Step 8 of "Dump from STM32CubeIDE by hand" was unclear about where the thread addresses come from,
what format `ring.names` has, and it meant typing one `print` per thread. It now has three steps:

1. In a terminal: `python3 tools/trace_names.py gdb ~/trace_out/ring.bin > ~/trace_out/names.gdb`.
2. In the debugger console: `source /home/<you>/trace_out/names.gdb`, which prints the `TASKNAME` lines.
3. Copy those lines into `~/trace_out/ring.names`. Other lines are ignored by `trace_names.py apply`.

No change to `tools/` or the firmware.

## Tests (this PC, STM32CubeIDE 2.1.0, board not connected)

Input: the real dump `docs/results/2026-11-14_trace_trigger/break_upload/ring.bin` (SHA-256
98121abb9efa7d6a...), made with the current Debug-Trace image (c212334bd4a12e7e). Its `ring.names`,
written by `tools/trace_dump.sh` on the board, gives the names.

| # | Test | Result |
|---|---|---|
| 1 | `python3 tools/trace_names.py gdb ring.bin > names.gdb` | five `printf` commands, one per control block (`names.gdb`) |
| 2 | `source names.gdb` in STM32CubeIDE's `arm-none-eabi-gdb` (14.3.rel1), connected to `gdb_stub.py` | five lines `TASKNAME <address> <name>`, the same as the board's `ring.names` (`gdb_source_output.txt`) |
| 3 | `source names.gdb` with GDB not connected (ELF only), Debug-Trace or Debug ELF | the five `TASKNAME` lines with no name after the address, no error message |
| 4 | test 3's output as `ring.names`, `tools/trace_decode.sh` | `trace_names: no names to apply`; decoded, with the symbol names |
| 5 | a pasted `ring.names` with extra lines: the `source` command, empty and blank lines, GDB's `0x00000000 in ?? ()`, `[Inferior 1 (Remote target) detached]`, `(gdb) ` (`ring.names_pasted`), `tools/trace_decode.sh` | five `trace_names: ... -> ...` lines; `trace.json` threads: `Console`, `IDLE`, `Sensors`, `Uplink`, `WiFi_ISM43362_A` (`decode_pasted.txt`) |
| 6 | the same dump without `ring.names` | no `trace_names` lines; `trace.json` threads: `Idle_TCB.3`, `MainApp_Task(void*)::console`, `::sensors`, `::uplink`, `async_thread_cb_mem` (`decode_without_names.txt`) |

`gdb_stub.py` is a minimal GDB remote stub, started by GDB with
`target remote | python3 -I gdb_stub.py ring.names`. It answers memory reads with zeros, except for
`pcTaskName` (offset 0x34 in `TCB_t`, from `print &((TCB_t*)0)->pcTaskName` in this ELF) of each
control block named in the board's `ring.names`. It stands in for the board: what it tests is that GDB
accepts the generated file with `source` and prints the lines in the format that `apply` reads.

## Not covered

- **STM32CubeIDE's debugger console.** `source` was run in the same GDB (STM32CubeIDE 2.1.0's
  `arm-none-eabi-gdb`) from the command line, not in the GUI console, and not with 2.2.0.
- **On the board.** The board was not connected to this PC. The names in test 2 come from the stub,
  which serves the names that `tools/trace_dump.sh` read from the board on 2026-11-14.
