# Scripts in tools/: no silent exits, any STM32CubeIDE version (2026-11-15)

Reported from a walk-through: `tools/trace_dump.sh` ended with exit 2 and no message when STM32CubeIDE
was neither in `~/st/stm32cubeide_2.1.0` nor in `/opt/st/stm32cubeide_2.1.0`.
`GDBSERVER=$(ls ... | head -1)` failed under `set -euo pipefail` before the script reached its own
error message.

## Changes

- `tools/toolchain.sh`:
  - Uses the newest `stm32cubeide_*` folder with a `plugins/` folder, any version, from `~/st` and
    `/opt/st` (`sort -V`, newest first). `CUBEIDE` still overrides it.
  - Never stops the calling script.
  - Offers `cubeide_tool` (a tool in an STM32CubeIDE plugin, or empty), `need_cmd` and
    `toolchain_missing`. `toolchain_missing` says which folder was used or which folders were
    searched, whether `CUBEIDE` is not an STM32CubeIDE folder, and how to set `CUBEIDE`.
  - Run on its own, it prints what it found.
- All scripts in `tools/` have an ERR trap, which prints the script, line, exit status and command
  for any failure the script does not handle itself:
  - The other scripts use `set -Eeuo pipefail`, so the trap also applies inside functions.
  - `check_secrets.sh` keeps `set -uo pipefail`: it reports every finding and continues.
- Lookups that may legitimately find nothing end in `|| true` inside the command substitution.
- Every script checks its inputs (ELF, ring file, tools) and says what is missing, with the script
  name.
- `thingspeak_feed.sh`:
  - Its message for a non-numeric channel ID shows the format without quotes.
  - It checks for `curl`.
- `tools/thingspeak_host.env.template` and docs/02 2.4 show the format: `NAME=value` without
  quotes, unlike `app_secrets.h`. docs/03 points to it.
- docs/01 1.3: the lookup rule and the output of `tools/toolchain.sh`.
- docs/06:
  - 6.3: the dump is `ring.bin` and `ring.names`; `trace.json` comes from `trace_decode.sh`.
  - 6.9: the two new messages.

The firmware is not changed (no file under `Core/`), so the Debug, Release and Debug-Trace images
are the same as in `2026-11-14_first_upload_after_boot.md`.

## Tests

### Test environment

- `$SCRATCH` stands for a scratch folder.
- `$SCRATCH/noarm` is a `PATH` folder with links to everything in `/usr/bin` and `/usr/local/bin`
  except `arm-none-eabi-*` and `STM32_Programmer_CLI`, so that no Arm tool is on the `PATH` (the test
  machine has Ubuntu's `arm-none-eabi-nm` in `/usr/bin`).
- Fake STM32CubeIDE folders:
  - `$SCRATCH/empty`: an empty folder.
  - `$SCRATCH/st/stm32cubeide_2.0.3`: a folder name that does not exist.
  - `home1/st/stm32cubeide_2.0.5`: an empty folder.
  - `home2/st/stm32cubeide_2.0.9/plugins`: an empty `plugins/` folder.
  - `home3/st/stm32cubeide_2.1.0`: a link to the real installation.
- `/opt/st` holds STM32CubeIDE 1.15.1 and 1.19.0 on this machine.

Full output: `2026-11-15_script_errors/failure_cases.txt` and `success_cases.txt`.

| # | Case | Result |
|---|---|---|
| 1 | `CUBEIDE` = empty folder, `trace_dump.sh` | `ST-LINK_gdbserver (part of STM32CubeIDE) not found.`, the `CUBEIDE` value, `... has no plugins/ folder`, the `CUBEIDE=` hint; exit 2 |
| 2 | `CUBEIDE` = a 2.0.3 folder name that does not exist | the same, with that path; exit 2 |
| 3 | `HOME` with only an empty `stm32cubeide_2.0.5`, `toolchain.sh` | the empty folder is skipped; `/opt/st/stm32cubeide_1.19.0` (newest with `plugins/`) is used; exit 0 |
| 4 | `HOME` with `stm32cubeide_2.0.9/plugins` (empty), `trace_dump.sh` | 2.0.9 is the newest and is chosen; `ST-LINK_gdbserver ... not found`, `STM32CubeIDE: .../stm32cubeide_2.0.9 (newest of ...)`; exit 2 |
| 5 | no Arm tools on the `PATH`, `CUBEIDE` empty, `trace_decode.sh` | `arm-none-eabi-nm not found.` and the same lines; exit 2 |
| 6 | the same, `trace_dump.sh` | `ST-LINK_gdbserver ... not found.`; exit 2 |
| 7 | the same, `flash_and_capture.sh` | `arm-none-eabi-nm not found.`; exit 1 |
| 8 | no Arm tools on the `PATH`, `toolchain.sh` | all four tools from `~/st/stm32cubeide_2.1.0_2` (the newest of the two 2.1.0 folders here, same tool versions); programmer from `~/st/STM32CubeProgrammer` |
| 8b | the same with only `home3/st/stm32cubeide_2.1.0` | all four tools from that installation (docs/01 shows this output) |
| 9 | no Arm tools on the `PATH`, `trace_decode.sh` on an older ring | `arm-none-eabi-nm` from STM32CubeIDE; `== decoded: ...`; exit 0 |
| 10 | a 100-byte random file as ring | `ring2log.py`'s `not a format 1 trace ring`, then `trace_decode.sh: stopped by an unexpected error (exit 1) at line 41: python3 -I "$T/ring2log.py" ...`; exit 1 (the ERR trap) |
| 11 | ring file missing, `trace_decode.sh` | `... not found: dump the ring first (tools/trace_dump.sh)`; exit 2 |
| 12 | ELF missing, `trace_dump.sh` | `... not found: build the Debug-Trace configuration first`; exit 2 |
| 15 | unknown option, `trace_dump.sh` | `trace_dump.sh: unknown option --bogus` and the usage; exit 2 |
| 16 | `check_secrets.sh` | `check_secrets: clean ...`; exit 0 |
| 18 | `thingspeak_feed.sh 2` | two entries; exit 0 |
| 13 | `trace_dump.sh`, no Arm tools on the `PATH`, board **not connected** | GDB server's `ST-Link enumeration failed`, then `trace_dump.sh: GDB server did not start (board connected? another debug session open?)`; exit 1 |
| 17 | `flash_and_capture.sh`, the same | `flash_and_capture.sh: no ST-LINK virtual COM port found (is the board connected ...?)`; exit 1 |

## Not covered

- **On-board success runs after this change.** The board was not connected to the test PC
  (cases 13 and 17), although it was powered and uploading: entries up to 237118, case 18.
  - Not run: `trace_dump.sh` (dump) and `flash_and_capture.sh`.
  - Their tool lookup was checked by cases 8 and 8b.
  - What they do once the tools are found is unchanged, apart from the messages.
- **The decoded thread names in case 9.** The ring came from an older image, so the names were not
  checked (its `.names` file was left out).
- **Windows.** Not tested.
