# Arm CMSIS-Driver: Wi-Fi driver for the Inventek ISM43362 (copied by hand)

The B-L475E-IOT01A's Wi-Fi module (Inventek ISM43362-M3G-L44, on SPI3) is driven by Arm's
CMSIS-Driver `WiFi/ISM43362` (driver version 1.16, CMSIS-Driver WiFi API 1.1).
Versions and commits: `VERSION`.

| Here | Source | Licence |
|---|---|---|
| `WiFi/ISM43362/` (all files) | github.com/ARM-software/CMSIS-Driver, tag 2.11.0, `WiFi/ISM43362`, unmodified | Apache-2.0 (SPDX header in every file; `LICENSE`) |
| `LICENSE` | same repository and tag | Apache-2.0 |
| `Include/Driver_Common.h`, `Driver_SPI.h`, `Driver_WiFi.h` | github.com/ARM-software/CMSIS_6, tag v6.4.0, `CMSIS/Driver/Include`, unmodified | Apache-2.0 (SPDX headers; `Include/LICENSE.txt`) |

Built: `WiFi_ISM43362.c`, `WiFi_ISM43362_Buf.c`, `WiFi_ISM43362_Mem.c`. Not built:
`WiFi/ISM43362/Config/` (excluded in `.cproject`): `WiFi_ISM43362_HW.c` there is Arm's empty template
of the pin functions, and `WiFi_ISM43362_Config.h` is replaced by the project's copy (below).
`Validation/` holds Arm's test reports only (text).

## The project's own parts

| File | What | Licence |
|---|---|---|
| `Core/Config/WiFi_ISM43362_Config.h` | Arm's configuration file with two values changed: SPI driver 3, 10 Mbit/s (notice in the file) | Apache-2.0 (modified copy) |
| `Core/Config/RTE_Components.h` | defines `RTE_CMSIS_RTOS2_FreeRTOS` for the driver | MIT |
| `Core/Src/cmsis_driver_spi3.c` | `ARM_DRIVER_SPI Driver_SPI3` on CubeMX's `hspi3` (polled, blocking) | MIT |
| `Core/Src/wifi_ism43362_pins.c` | `WiFi_ISM43362_Pin_*` on CubeMX's pins PE8 (reset), PE0 (chip select), PE1 (data ready, polled) | MIT |

`Core/Config` is on the C compiler's include path only (the driver is C). CSP4CMSIS, compiled by G++,
also looks for an `RTE_Components.h` and therefore must not see this one.

## Why not ST's driver

STM32Cube FW_L4 contains ST's own ISM43362 driver (`es_wifi`, `wifi`, in
`Projects/B-L475E-IOT01A/Applications/WiFi/Common`). Unlike the board support files in `Drivers/BSP`
(BSD-3-Clause), it belongs to "B-L475E-IOT01A Applications", which the package's `sbom_cdx.json`
lists under ST's SLA0044 licence. SLA0044 limits use to ST devices and forbids redistribution that
would subject the code to open-source terms (it names MIT and BSD). No ST Wi-Fi file is part of
this project.
