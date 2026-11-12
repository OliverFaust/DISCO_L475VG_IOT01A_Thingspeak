# Phase 3, step 1: Wi-Fi bring-up (2026-11-12)

Goal: talk to the ISM43362 Wi-Fi module on SPI3 with Arm's CMSIS-Driver, using the SPI3 and pin
configuration of STM32CubeMX; print firmware version and MAC address, scan for access points.

## 1. Licence check of the candidate drivers

| Candidate | Where | Licence | Used |
|---|---|---|---|
| ST `es_wifi.c/.h`, `es_wifi_io.c/.h`, `wifi.c/.h`, `es_wifi_conf_template.h` | STM32Cube FW_L4 V1.18.2 (FW.L4.1.18.0 + patch 1.18.2), `Projects/B-L475E-IOT01A/Applications/WiFi/Common` | headers: "terms in the LICENSE file in the root directory of this software component"; the package's `LICENSE.md` puts *STM32 Projects* under SLA0044, and `sbom_cdx.json` lists "B-L475E-IOT01A Applications 1.18.2: Ultimate Liberty license (SLA0044)" | no |
| Arm CMSIS-Driver `WiFi/ISM43362` (8 files) | github.com/ARM-software/CMSIS-Driver, tag 2.11.0 | `SPDX-License-Identifier: Apache-2.0` in every file | yes |
| `Driver_Common.h`, `Driver_SPI.h`, `Driver_WiFi.h` | github.com/ARM-software/CMSIS_6, tag v6.4.0 | `SPDX-License-Identifier: Apache-2.0` | yes |

Provenance, commits, and what is built: `Drivers/CMSIS-Driver/README.md` and `VERSION`.

## 2. Integration

- **SPI3 and pins from CubeMX.** `Core/Src/cmsis_driver_spi3.c` implements `Driver_SPI3` on `hspi3`
  (16-bit frames, mode 0, 10 Mbit/s); `Core/Src/wifi_ism43362_pins.c` drives reset (PE8) and chip
  select (PE0) and reads data ready (PE1) on the CubeMX pins. Neither initialises any hardware.
- **Interrupts: none.** SPI transfers are polled (HAL, blocking), and DATARDY is polled by the driver
  every 1 ms (it detects at start-up that no data-ready event arrives). SPI3 and EXTI1 interrupts are
  not enabled in the NVIC, so nothing interacts with `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
  (5). If DATARDY is made interrupt-driven later, its EXTI1 priority must be 5..15 (numerically), since
  the handler would call `osEventFlagsSet()`; with ST's CMSIS-RTOS2 wrapper that call from an
  interrupt goes through `xEventGroupSetBitsFromISR()` and the FreeRTOS timer task (priority 2 here).
- **Configuration.** `Core/Config/WiFi_ISM43362_Config.h`: Arm's file with SPI driver 3 and
  10 Mbit/s; all else default (4 sockets, 16 KB receive memory pool, driver thread
  `osPriorityAboveNormal` with a 1 KB static stack). `Core/Config/RTE_Components.h` defines
  `RTE_CMSIS_RTOS2_FreeRTOS`, so the driver's thread also gets a static control block.
- **Build:** 0 errors, 0 warnings (Debug). Size: text 76028, data 112, bss 40112 (phase 1: bss 17032;
  +16 KB of it is the driver's receive memory pool, a static array).
- **Regeneration:** after a CubeMX regeneration the `.cproject` keeps the new include paths and the
  exclusion of `Drivers/CMSIS-Driver/WiFi/ISM43362/Config` (only the line order changes).

## 3. On the board

Test process: `Core/Src/wifi_bringup_test.cpp`, started instead of the Sensors/Console network
(at the time a constant in `Core/Src/app.cpp`; now `APP_DIAGNOSTIC_MODE APP_DIAG_WIFI_BRINGUP` in
`Core/Inc/app_config.h`). Command:
`tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 30 <raw>`, then
`tools/mask_log.py <raw> docs/results/2026-11-12_wifi_bringup_console.txt`.

```
CMSIS-Driver WiFi API 1.1, ISM43362 driver 1.16
  FreeRTOS heap before WiFi_Initialize: free 0 of 1024 bytes, minimum ever free 0 bytes
Initialize: OK (812 ms)
PowerControl(FULL): OK
  FreeRTOS heap after Initialize: free 640 of 1024 bytes, minimum ever free 640 bytes
Module info: OK: ISM43362-M3G-L44-SPI,C3.5.2.7.STM,v3.5.2,v1.4.0.rc1,v8.2.1,120000000,Inventek eS-WiFi
MAC address (station): OK: C4:7F:51:XX:XX:XX
Scan: 2 access point(s) (1773 ms)
   1  Vod***********                    DA:D8:E5:XX:XX:XX  ch  10   -63 dBm  WPA2
   2  MTS                               00:1E:42:XX:XX:XX  ch  11   -86 dBm  WPA2
  FreeRTOS heap at the end: free 640 of 1024 bytes, minimum ever free 640 bytes
```

- **Module firmware: C3.5.2.7.STM** (ISM43362-M3G-L44-SPI; the STM variant, which Arm's driver
  accepts in any version; its documented limitations refer to C3.5.2.5.STM).
- `Initialize` takes 812 ms: 50 ms reset pulse and 750 ms start-up wait, both in the driver.
- Masked in the committed file: the last three bytes of every MAC address/BSSID and all but three
  characters of each SSID (`tools/mask_log.py`); a BSSID with its SSID can be located with public
  Wi-Fi databases. The raw capture is not committed.

## 4. FreeRTOS heap

The driver creates two mutexes and five event flag objects (`osMutexNew`, `osEventFlagsNew` with
no memory supplied), i.e. from the FreeRTOS heap; everything else of it is static.

| Configuration | Free after `Initialize` | Used by the driver | Minimum ever free after the test |
|---|---|---|---|
| `TOTAL_HEAP_SIZE` 4096 (phase 1 value) | 3712 | **384 bytes** | 3712 |
| `TOTAL_HEAP_SIZE` **1024** (new value) | 640 | **384 bytes** | 640 (`xMinimumEverFreeBytesRemaining` read over SWD: 0x280) |

`TOTAL_HEAP_SIZE` is now **1024** (STM32CubeMX, FREERTOS > Config parameters): 2.7 times the
measured need, and 3 KB less RAM. The application code still allocates nothing: newlib's
`malloc()` is never called (`__sbrk_heap_end = 0`). The heap use is re-measured in the soak test
(step 3), where sockets are opened and closed repeatedly.

## Not yet done

- The driver thread's stack high-water mark (1 KB static stack): step 3, with the other processes.
- The data-ready interrupt (see section 2); polling is used.
