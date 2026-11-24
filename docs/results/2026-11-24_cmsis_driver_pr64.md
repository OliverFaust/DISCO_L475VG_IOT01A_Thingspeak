# CMSIS-Driver PR #64 (ISM43362 driver V1.17) on the B-L475E-IOT01A (2026-11-24)

Branch `test/cmsis-driver-pr64` only; not merged anywhere. Arm's answer to our issue
[ARM-software/CMSIS-Driver#60](https://github.com/ARM-software/CMSIS-Driver/issues/60) is the pull
request [#64](https://github.com/ARM-software/CMSIS-Driver/pull/64), commit `5995f87`
("Updated Inventek ISM43362 WiFi driver (version 1.17.0)"), open and not released on the day of the
test. Arm asked us to test it on this board.

## Set-up

| | |
|---|---|
| Board | B-L475E-IOT01A (lab board), ST-LINK firmware V2J46M31 |
| Wi-Fi module | ISM43362, firmware `ISM43362-M3G-L44-SPI,C3.5.2.7.STM,v3.5.2,v1.4.0.rc1,v8.2.1,120000000,Inventek eS-WiFi`, SPI3 at 10 MHz, DATARDY polled |
| Driver | `Drivers/CMSIS-Driver/WiFi/ISM43362` replaced by `git archive 5995f87 WiFi/ISM43362`, unmodified (`Drivers/CMSIS-Driver/VERSION`) |
| Config | `Core/Config/WiFi_ISM43362_Config.h`: `WIFI_ISM43362_HW_RESET 1` (new), the rest unchanged (4 sockets) |
| RTOS | FreeRTOS 10.3.1, ST's CMSIS-RTOS2 wrapper, heap_4, `configTOTAL_HEAP_SIZE` 1024 bytes; the application itself takes nothing from this heap |
| Toolchain | STM32CubeIDE 2.1.0 headless build, GNU Tools for STM32 14.3.rel1, Debug configuration (`-O0`), STM32CubeProgrammer 2.22.0; Ubuntu 25.04 |

**Test code** (this branch only):

- **Diagnostic mode:** `APP_DIAG_WIFI_DRIVER_TEST` (`Core/Src/wifi_driver_test.cpp`) is one process
  that calls the driver in the sequences below. For each call it prints the return value, the
  duration, and the RSTN calls the driver made. For each step it prints the FreeRTOS heap
  (`xPortGetFreeHeapSize()`, `xPortGetMinimumEverFreeHeapSize()`).
- **Counting RSTN calls:** in this mode `WiFi_ISM43362_Pin_RSTN()` (`Core/Src/wifi_ism43362_pins.c`)
  counts the driver's calls: *assert* = RSTN(1), PE8 low; *release* = RSTN(0), PE8 high. It also
  counts *level as requested*: the calls after which PE8's output register holds the level the
  driver asked for. So "assert 1, release 1, level as requested 2" means the reset pulse was
  really driven on the pin.
- **Simulating a stuck module (test e):** the flag `wifi_test_hold_reset` drives PE8 low and keeps it
  low. Clearing the flag does not release the pin: only the driver's next RSTN(0) does.

**Builds and captures:**

- **Default values per build:** each image was built from this commit's tree with the defaults in
  `Core/Inc/app_config.h` and `Core/Config/WiFi_ISM43362_Config.h` changed as listed in the table
  below. The changes were made for that build only, and the files were restored afterwards. That is
  why the capture headers say `-dirty` and `<build>/...elf`.
- **Capture:** `tools/flash_and_capture.sh`. After each run it reads `xMinimumEverFreeBytesRemaining`
  over SWD.

| Image | `APP_DIAGNOSTIC_MODE` | `APP_TEST_WIFI_INIT_FAIL` | `WIFI_ISM43362_HW_RESET` | Capture |
|---|---|---|---|---|
| a | `APP_DIAG_NONE` (the application) | 0 | 1 | `2026-11-24_cmsis_driver_pr64/a_soak_console.txt` (masked with `tools/mask_log.py`), `a_soak_feed.txt` |
| b | `APP_DIAG_WIFI_DRIVER_TEST` | 1 (module held in reset) | 1 | `b_init_fail.txt` |
| c, d, e | `APP_DIAG_WIFI_DRIVER_TEST` | 0 | 1 | `cde_hw_reset_1.txt` |
| c, d, e | `APP_DIAG_WIFI_DRIVER_TEST` | 0 | 0 (driver default) | `cde_hw_reset_0.txt` |

## Results

The heap figures are in bytes, out of 1024. heap_4 reports 0 until the first allocation. **1016**
is the free heap with none of the driver's objects allocated (8 bytes of heap_4's own overhead).
**640** is the free heap with the driver initialised: 384 bytes for 2 mutexes and 5 event-flag
objects. That matches the 2.11.0 measurement of `2026-11-12_uplink_recovery.md`.

### a. Normal operation, 30 min soak

Application image (Sensors, Console, Uplink; upload every 60 s), 1880 s capture. The run started
after a software reset; the restart counter for failed Wi-Fi starts was 0 of 3.

| | 2.11.0 (`2026-11-12_uplink_soak_console.txt`) | V1.17, `HW_RESET 1` |
|---|---|---|
| Module ready / joined | yes / 1 join | yes at 0.9 s / 1 join (4.8 s) |
| Uploads | 31 OK | **32 OK, 0 rejected, 0 failed** (entries 238510..238541) |
| Feed check (`tools/compare_uploads.py`) | all match | **all uploads match the feed**: 32 entries, no others in between, values equal |
| FreeRTOS heap at 60, 660, 1260, 1860 s | free 640, minimum ever 640 | **free 640, minimum ever 640** each time |
| Over SWD after the run | `xMinimumEverFreeBytesRemaining` 0x280 | **0x280 (640)**; `__sbrk_heap_end` 0 (no `malloc()`) |
| Uplink stack, max used | 227 words | 239 words of 1024 |

**Passed:** uploads = feed entries, and the heap minimum is unchanged (640 bytes).

### b. Leak fix: Initialize() 5 times with the module held in reset

| Call | Result | Duration | RSTN assert / release | Heap free after | Minimum ever |
|---|---|---|---|---|---|
| Initialize #1 to #5 | `ARM_DRIVER_ERROR` (-1) each | 812 ms each | 1 / 1 each | **1016, 1016, 1016, 1016, 1016** | 640 |

**Passed: the heap is identical after each failure.**

- **The objects are released.** Each failed call created its objects (minimum ever 640) and released
  all of them.
- **No board restart.** The five calls ran one after the other in the same run.
- **2.11.0 for comparison** (`2026-11-12_uplink_recovery.md`): 384 bytes lost per failed call.
  The minimum ever fell from 640 to 264, and the third attempt could no longer create its objects.
- **RSTN:** the driver also asked for a reset on every attempt, as `HW_RESET 1` intends. "Level as
  requested 1 of 2" is the test switch at work: it keeps PE8 low even when the driver releases it.

### c. Reset fix: Initialize, Uninitialize, Initialize

| `HW_RESET` | Call | Result | Duration | RSTN assert / release (level as requested) | Heap free |
|---|---|---|---|---|---|
| 1 | Initialize #1 | `ARM_DRIVER_OK` | 812 ms | 1 / 1 (2) | 640 |
| 1 | Uninitialize | `ARM_DRIVER_OK` | 1 ms | 0 / 0 | 1016 |
| 1 | Initialize #2 | `ARM_DRIVER_OK` | 812 ms | **1 / 1 (2)** | 640 |
| 0 | Initialize #1 | `ARM_DRIVER_OK` | 812 ms | 1 / 1 (2) | 640 |
| 0 | Uninitialize | `ARM_DRIVER_OK` | 1 ms | 0 / 0 | 1016 |
| 0 | Initialize #2 | `ARM_DRIVER_OK` | 13 ms | **0 / 0** | 640 |

After each Initialize, `GetModuleInfo` returned the module's firmware string above.

**Passed.**

- **`HW_RESET 1`:** RSTN is driven on both calls, and the second Initialize succeeds.
- **`HW_RESET 0`:** the old default is preserved. The module is reset on the first Initialize only.
  The second Initialize succeeds without a reset because the module was still working.
- **The heap returns to 1016** after Uninitialize in both cases.

### d. Uninitialize() without Initialize(), and twice in a row

| `HW_RESET` | Sequence | Return values |
|---|---|---|
| 1 and 0 | Uninitialize, Uninitialize (no Initialize before) | **-1 `ARM_DRIVER_ERROR`, -1 `ARM_DRIVER_ERROR`** |
| 1 and 0 | after a successful Initialize: Uninitialize, Uninitialize | `ARM_DRIVER_OK`, **-1 `ARM_DRIVER_ERROR`** |

**Confirmed:** an Uninitialize() on an uninitialised driver now returns `ARM_DRIVER_ERROR`.

- **What V1.16 did:** it returned `ARM_DRIVER_OK` at once, from code reading. V1.17 removed the early
  `if (driver_initialized == 0U) return ARM_DRIVER_OK;`.
- **Why it fails now:** the loop over `event_flags_sockets_id[]` calls `osEventFlagsDelete()` without
  a NULL check, unlike the other objects. With a NULL handle, the CMSIS-RTOS2 wrapper returns
  `osErrorParameter`, and the driver turns that into `ARM_DRIVER_ERROR`.
- **No harm otherwise:** nothing else went wrong. The heap was unchanged, and the next Initialize()
  succeeded in both images.
- **Not caught by Arm's validation:** its `WIFI_Initialize_Uninitialize` test (CMSIS-Driver_Validation,
  `DV_WIFI.c`) never calls Uninitialize() twice in a row, or before Initialize().

### e. Recovery in the same run (report only)

Sequence:
1. Initialize, join, DNS lookup of `api.thingspeak.com`, TCP connect to port 80, close.
2. Then the module stops answering: PE8 held low.
3. A DNS lookup and IsConnected show it.
4. Uninitialize, Initialize (PE8 still low until the driver releases it).
5. Join, DNS, connect again.

No ThingSpeak update was sent in this test.

| Step | `HW_RESET 1` | `HW_RESET 0` |
|---|---|---|
| Before: Initialize, join, DNS, connect | all OK (join 4.8 s) | all OK (join 4.7 s) |
| Stuck: SocketGetHostByName | `ARM_DRIVER_ERROR` after 618 ms | `ARM_DRIVER_ERROR` after 613 ms |
| Stuck: IsConnected | 0 (not connected; printed as "0 ARM_DRIVER_OK" by the test) | 0 |
| Uninitialize | `ARM_DRIVER_OK`, heap 1016 | `ARM_DRIVER_OK`, heap 1016 |
| Initialize | **`ARM_DRIVER_OK`**, 812 ms, RSTN 1 / 1 (2) | **`ARM_DRIVER_ERROR`**, 12 ms, RSTN 0 / 0 |
| After: join, DNS, connect | **all OK** (join 4.8 s) | not run |
| Heap free at the end | 640 (driver initialised) | 1016 (failed Initialize released everything) |

**Answer: yes, at driver level, with `HW_RESET 1`.**

- **What worked:** Uninitialize/Initialize brought a module that no longer answered back to a working
  network path in the same run. There was no board restart and no heap loss.
- **`HW_RESET 0`:** the same recovery fails, because the module is not reset. As in V1.16, the failed
  Initialize no longer leaks.
- **Not tested:**
  - this recovery inside the application's Uplink process (it still restarts the board; see below);
  - a module that hangs with RSTN high (only "held in reset" was simulated).

## What this means for the project

- **The workaround stays on `main` for now:** the board restart after a failed module start, and the
  `.noinit` counter (docs/05 5.2, docs/07 7.1).
- **Once Arm has merged and released V1.17:**
  1. take the released driver;
  2. set `WIFI_ISM43362_HW_RESET 1`;
  3. let the Uplink retry Uninitialize/Initialize in the same run instead of restarting, as docs/05
     5.2 describes.
- **Check the d-behaviour first:** the Uplink calls Uninitialize() only after a successful
  Initialize(). So the `ARM_DRIVER_ERROR` of d does not affect it, as long as it calls Uninitialize()
  once per Initialize().

The reply to Arm on PR #64 is drafted separately (not in the repository).
