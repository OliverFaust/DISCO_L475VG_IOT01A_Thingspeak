# Fresh-clone rehearsal (2026-11-14)

The project was set up again from GitHub in a new, empty directory on the test machine (Ubuntu 25.04),
following only `README.md` and `docs/01`-`03` (and `06` for the trace), at commit `b2fa29c` of
`csp4cmsis-port`. Board: the lab B-L475E-IOT01A (ST-LINK firmware V2J46M31). Tools as in docs/01:
STM32CubeIDE 2.1.0 in `~/st/stm32cubeide_2.1.0`, git 2.48.1, Python 3.13.3, minicom.

Deviations from what a student does, and why:

- A scratch folder stood in for `~` (the clone and the workspace `stm32ws` inside it), so that the
  working copy was not touched.
- **The GUI steps were not rehearsed**: 3.3 (*File > Import*), the GUI build of 3.4, and 3.6 (*Run As*,
  the *Edit Configuration* dialog, a possible ST-LINK upgrade prompt). The rehearsal had no way to operate
  the STM32CubeIDE window. They were replaced by the command-line routes that the same sections
  document (`headless-build.sh -import/-build`, `tools/flash_and_capture.sh`), which give the same
  build and flash. **To do by hand: 3.3, 3.4 (GUI), 3.6 (GUI).**
- The ThingSpeak channel (docs/02) already existed; its values were copied into the new secrets files
  by a script, without printing them.

## Steps and results

| Docs | Step | Result |
|---|---|---|
| 01, 1.3 | `ls ~/st/stm32cubeide_2.1.0/stm32cubeide`, `git --version`, `python3 --version` | as documented (`git version 2.48.1`, `Python 3.13.3`) |
| 01, 1.4 | udev rules, `id -nG`, `/dev/serial/by-id/`, `STM32_Programmer_CLI -l st-link` (the copy inside STM32CubeIDE, glob as documented) | as documented: `49-stlinkv2-1.rules`, `dialout`, `...-if02 -> ../../ttyACM0`, `ST-LINK FW  : V2J46M31`, `Board Name  : STM32L4IO` |
| 01, 1.5 | `minicom -D /dev/serial/by-id/...-if02 -b 115200`, board reset | start banner received (minicom run under `script` with `-C` capture). Before it, lines received earlier by the PC (see finding 4). |
| 03, 3.1 | `git clone https://github.com/OliverFaust/DISCO_L475VG_IOT01A_Thingspeak.git` | cloned the default branch `main` = **the Mbed version** (finding 1); after `git switch csp4cmsis-port` the listing matches 3.1 |
| 03, 3.2 | build **before** copying the template | `#error "Core/Inc/app_secrets.h is missing. In the project folder run:  cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h  then ..."`; `Build Failed. 11 errors, 0 warnings.` (finding 2) |
| 03, 3.2 | `cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h`, values filled in; `git status --short` | no line with `app_secrets.h` (nor `thingspeak_host.env`) |
| 03, 3.4 | build Debug, Release, Debug-Trace (headless) | all `Build Finished. 0 errors, 0 warnings.`; Debug `text 109664 data 152 bss 48376`, Release `text 63984`, as documented; **flash images identical** to the working copy's builds with the same secrets: Debug `c23e71dbd09cbd1f`, Release `2afbffa9b5dff454`, Debug-Trace `8697fe9e54851aa4` |
| 03, 3.6 | `tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 90 run.txt` | `Download verified successfully`; 90 s captured: `2026-11-14_fresh_clone_debug_console.txt` |
| 03, 3.7 | check the console line by line | every line of the table present: banner, `CSP4CMSIS 3.1.0`, `Reset cause: software reset` (as the table says for the script), HTS221 check all OK, sample every 5 s, `Wi-Fi module ready` at 0.925 s, `joined (8824 ms)` at 9.757 s (finding 3), `sample #3 -> entry ...` at 11.1 s and again at 71.4 s, stack report at 60 s; `malloc()` never called, FreeRTOS heap minimum 640 bytes free |
| 02, 2.4 | `tools/thingspeak_feed.sh 3` | newest `entry_id` equals the last `-> entry` on the console; fields 1-3 filled |
| 06, 6.2-6.4 | flash Debug-Trace (75 s capture), `tools/trace_dump.sh ...` about 4 s after the upload line, `tools/trace_decode.sh ...` without `PROBCLI` | dump with 5 names; ring clean (`torn 0`, `orphan continuation records 0`, `dropped 0`, heartbeats 1 s apart); `== eventlist not found: decode.py --no-eventlist`; process names applied; `== ProB not found (set PROBCLI): conformance check (csp_trace.py) skipped`; the upload is in the 20 s window. Checks: `ALT offers: FAIL ... 2 unresolved` (finding 5) besides the documented artefacts; `2026-11-14_fresh_clone_trace_summary.txt`, console `2026-11-14_fresh_clone_trace_console.txt` |

## Findings: docs and reality

1. **`git clone` gives the Mbed version.** The default branch `main` still holds the Mbed project; the
   docs describe the state after the merge. No change to the docs: correct once `csp4cmsis-port` is merged
   into `main`. Until then, reviewers need `git switch csp4cmsis-port` after the clone.
2. **The build without the secrets file** ends with `Build Failed. 11 errors, 0 warnings.` -- the
   `#error` from four source files and follow-on errors. docs/03, 3.2 now says so.
3. **Join time** 8.8 s here, 4.8 s in the documented example; docs/03, 3.7 said "within about 10 s".
   The table now gives the range measured in all recorded runs (3.0 to 8.8 s).
4. **Old lines in a newly opened terminal**: the PC keeps what the board sent while no program read
   the port, and minicom shows it first. Documented in docs/03, 3.5 (found while testing minicom for
   docs/01).
5. **`ALT offers: FAIL; ... 2 unresolved`** -- the dump began 0.07 ms before the Console's `WaitAlt`,
   after the `AltGuard` records of its first two guards (`WaitAlt names 4 guards, 2 described`). A
   third kind of window-start artefact, not covered by docs/06, 6.4; now explained there and added to
   `docs/upstream/csp4cmsis-dev-tracing-notes.md` (note 2).

Found while writing the docs, before the rehearsal (fixed in commit `ae5be94`):

6. `tools/flash_and_capture.sh`, `trace_dump.sh` and `decode.py` needed `arm-none-eabi-nm` on the
   `PATH`, which an installation of STM32CubeIDE alone does not provide (on the test machine it came
   from Ubuntu's `binutils-arm-none-eabi`). `tools/toolchain.sh` now finds STM32CubeIDE's own tools and
   its copy of `STM32_Programmer_CLI`; checked with a `PATH` without any GNU Arm or ST tool, with and
   without STM32CubeIDE present.

## Not covered

- The GUI steps listed above (3.3, 3.4 GUI, 3.6 GUI).
- docs/02, 2.1-2.2 (creating a channel; the existing one was used).
- Windows.
