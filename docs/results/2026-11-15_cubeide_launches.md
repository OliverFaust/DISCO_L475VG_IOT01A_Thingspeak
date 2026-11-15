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

## Addendum 2: the same with STM32CubeIDE 2.1.0; launch files corrected (2026-11-15, evening)

**Reported:** the same behaviour with STM32CubeIDE 2.1.0 on the test PC: **Debug-Trace (attach,
dump)** loaded `Debug/...elf` until the Main tab's **C/C++ Application** was set by hand to
`Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf`; then the dump worked.

**What the test clone on this PC shows** (fresh clone of `0bbf9f5` at 19:22, workspace opened with
STM32CubeIDE 2.1.0 at 19:20, per the workspace log):

- `git status`: `.cproject` and `Debug-Trace (flash and debug).launch` modified; two new files,
  `.settings/stm32cubeide.project.prefs` and `.settings/com.st.stm32cube.ide.mcu.sfrview.prefs`.
  The `.project` and the `.ioc` were not changed.
- **The flash launch** was rewritten by the IDE at 19:59:53. It gained 25 attributes, all with
  default values (debug authentication, SWO clock limit, multi-drop, semihosting `Disabled`, RTOS
  proxy off, CTI, halt delay, memory blocks, ...). No existing attribute changed: `PROGRAM_NAME`, the
  load list path and the build configuration are still `Debug-Trace`.
- **The attach launch** was not written by the IDE: same content and size as committed (7344 bytes),
  file time = clone time. So the Main-tab edit of the attach launch was not saved into the shared
  file. No other launch configuration exists, neither in the project (also none ignored by
  `.gitignore`) nor in the workspace (`.metadata/.plugins/org.eclipse.debug.core/.launches` does
  not exist).
- **`.cproject`:** one line added at the end,
  `<storageModule moduleId="org.eclipse.cdt.make.core.buildtargets"/>` (the empty make-targets list).
- **No `Debug/` ELF in that clone:** only `Debug-Trace/` was built (19:33). A session in this clone
  cannot have loaded this clone's `Debug/...elf`.

**Where STM32CubeIDE 2.1.0 takes the ELF from** (its plugins, disassembled with `javap`):

- The Main tab (CDT `CAbstractMainTab.updateProgramFromConfig`) shows `PROGRAM_NAME`; GDB loads the
  program from it (CDT `verifyProgramPath`).
- What ST's launch downloads and loads symbols from is the load list
  (`com.st.stm32cube.ide.mcu.debug.launch.loadList`, class `LoadListInfo$LoadListItem`). An item has
  the fields `fIsFromMainTab`, `fPath`, ..., `fProjectName`, **`fBuildConfigId`**,
  **`fUseActiveBuildConfig`**, `fPerformBuild`, `fDownload`, `fLoadSymbols`. If
  `fUseActiveBuildConfig` is true, `getAbsolutePath()` ignores `fPath` and takes the artifact of the
  project's **active** build configuration (`getDefaultConfiguration()`,
  `${PWD}/${BuildArtifactFilePrefix}${BuildArtifactFileName}`) -- after an import, Debug.
- The hand-written files had neither field. In 2.1.0 a missing `fUseActiveBuildConfig` is `null`
  and counts as false, so `fPath` is used. Whether 2.2.0 treats it the same could not be checked here
  (2.2.0 is not installed on this PC).

**Root cause: not found on this PC.** Nothing in the files or the workspace explains a `Debug/...elf`
in 2.1.0. The likeliest explanation is a missing build-configuration field in the load list read as
"use the active configuration", but that is not proven.

**Correction (both files):**

1. The load list item now names the build configuration explicitly:
   `"fBuildConfigId":"com.st.stm32cube.ide.mcu.gnu.managedbuild.config.exe.debug.1527852739"`
   (Debug-Trace) and `"fUseActiveBuildConfig":false`, in the field order that Gson writes.
   Parsed with ST's own `LoadListInfo` class and Gson from STM32CubeIDE 2.1.0 (Java 21 of the IDE):
   `path=Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf config=...1527852739 useActive=false` for
   both; the old attach file gave `config=null`.
2. Both files are now in the form the IDE writes: the 25 default attributes added (the flash launch
   is the IDE's own file from the test clone, plus change 1; the attach launch has the same key set
   with its own values). Opening them no longer modifies the tracked files. A Python serializer
   reproduces the IDE-saved file byte for byte, so the format is the IDE's.
3. `.cproject`: the `buildtargets` line the IDE adds, so that a fresh clone stays clean.

The two files differ only in: load list (`fPerformBuild`, `fDownload`), `reset_strategy`
(`connect_under_reset` / `no_reset`), `setResume`, `setStopAt`, `DEBUGGER_STOP_AT_MAIN`,
`ATTR_BUILD_BEFORE_LAUNCH_ATTR` (1 / 0). Paths: project-relative only (no `/home`, `/media`,
`C:`).

**Checks:** headless import of the changed tree into a new workspace and build of Debug-Trace and
Debug with STM32CubeIDE 2.1.0: 0 errors, 0 warnings; images Debug-Trace `c212334bd4a12e7e`, Debug
`62205605b1f7a49f` (unchanged); the import and build left both launch files and `.cproject`
unchanged. Not checked here: the GUI (no board on this PC's USB), STM32CubeIDE 2.2.0.

**Not committed:** `.settings/stm32cubeide.project.prefs` (hashed keys written per workspace) and
`.settings/com.st.stm32cube.ide.mcu.sfrview.prefs` (SFR view state). They appear as untracked files
after the first GUI session; they do not affect the build or the launches.

**Screenshot `Screenshot From 2026-11-15 20-01-54.png`:** it shows the **Select Configuration**
dialog (both shared launches listed, `Debug-Trace (flash and debug)` selected, the flash launch's
download in the console), not the Main tab. It does not match Figure 6.1's caption, so it was not
added; Figure 6.1 still needs a screenshot of the Main tab.
