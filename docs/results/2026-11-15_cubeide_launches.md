# Shared STM32CubeIDE launch configurations for Debug-Trace (2026-11-15)

## Background

Reported from a walk-through of a manual dump in an STM32CubeIDE debug session:

- The dump command failed with `No symbol "csp_trace_ring" in current context`.
- `info files` showed `.../Debug/DISCO_L475VG_IOT01A_Thingspeak.elf`, and `p &csp_trace_ring` gave
  `No symbol`.
- The session had been started with the default launch, which builds and flashes Debug. This
  replaced the Debug-Trace build on the board.

## Added

Two launch configurations, saved as shared files in the project folder. `.gitignore` excludes every
other `*.launch` file.

| File | Build before launch | ELF | Download | Reset strategy | Breakpoint at `main` | Resume |
|---|---|---|---|---|---|---|
| `Debug-Trace (flash and debug).launch` | yes (Debug-Trace) | `Debug-Trace/...elf` | yes, verified | `connect_under_reset` | yes | yes |
| `Debug-Trace (attach, dump).launch` | no | `Debug-Trace/...elf` (symbols only) | no | `no_reset` | no | no (stays halted) |

- Both name the Debug-Trace configuration by its id in `.cproject`
  (`...config.exe.debug.1527852739`), with *Build configuration: auto* off. They do not follow the
  active configuration.
- Attribute names are taken from a launch file written by STM32CubeIDE 1.19.0. The reset strategy
  `no_reset` is one of the values in STM32CubeIDE 2.1.0's ST-LINK plugin
  (`com.st.stm32cube.ide.mcu.debug.stlink_2.4.400`).
- The files were written by hand, not saved from the GUI's dialog. STM32CubeIDE may add or reorder
  attributes when it saves them.

## Docs

- docs/06:
  - 6.3, *Dump from STM32CubeIDE by hand*, replaces the short paragraph that led to the error.
  - 6.2, step 2, flashes with the flash launch.
- docs/07 7.6: the `No symbol` message.

## Tests

Fresh clone of the working tree, with the two launch files and the new `.gitignore` committed in the
clone:

| Test | Result |
|---|---|
| `git ls-files` in the clone | both `.launch` files tracked |
| STM32CubeMX 6.17.0 headless (`config load <ioc>`, `project generate`) in the clone | `OK`; `git status` empty afterwards; SHA-256 of both `.launch` files and `.cproject` unchanged, so the Debug-Trace configuration id still matches |
| `headless-build.sh -data <new workspace> -import <clone> -build .../Debug-Trace` (STM32CubeIDE 2.1.0) | `Build Finished. 0 errors, 0 warnings.`; flash image `c212334bd4a12e7e` (the same as the board's); both `.launch` files still in the project folder |
| `arm-none-eabi-gdb -batch -ex 'p &csp_trace_ring' Debug-Trace/...elf` | `$1 = (struct csp_trace_ring *) 0x10000000 <csp_trace_ring>`, `sizeof` 16416 |
| the same with `Debug/...elf` | `No symbol "csp_trace_ring" in current context.` (the reported message) |

## Not covered (to be tested on the board)

- Both launches in the STM32CubeIDE GUI:
  - whether they are listed after the import;
  - whether the dialog opens them without a change;
  - whether the flash launch flashes and stops at `main`;
  - whether the attach launch connects without a reset and keeps the ring.
- The board was not connected to this PC.
- Whether **Terminate** after the attach session lets the program run on without a reset.

## Addendum: STM32CubeIDE 2.2.0 on a second machine

These GUI tests were run on a second machine with STM32CubeIDE **2.2.0**, not 2.1.0.

- **Debug-Trace (attach, dump)** loaded `.../Debug/DISCO_L475VG_IOT01A_Thingspeak.elf`.
- **Run > Debug Configurations > Main > C/C++ Application** had to be set by hand to
  `Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf`.
- The docs now include this check and the manual fix:
  - docs/06 6.3, step 2, Figure 6.1 (`docs/img/cubeide_launch_main.png`, screenshot to be added);
  - docs/07 7.6.
- The launch files themselves are unchanged. The diff of the 2.2.0 working copy after the GUI
  change was not available here, so it is not yet known:
  - what 2.2.0 writes into the launch files;
  - whether 2.2.0 changed `.cproject`, `.project` or the `.ioc`.
