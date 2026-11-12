# 8. How the project was made

This chapter rebuilds the STM32CubeMX/STM32CubeIDE project of this repository from nothing, one
setting at a time. Follow it when you start a **new** project of your own on the B-L475E-IOT01A
(or another STM32 board) and want the same foundation: FreeRTOS with ST's CMSIS-RTOS2 wrapper,
CSP4CMSIS as a source library, C++17, and no dynamic memory allocation.

You do **not** need this chapter to build and run the project in this repository: the `.ioc` file
and all generated code are committed (chapter 3). Every setting below is already in
`DISCO_L475VG_IOT01A_Thingspeak.ioc`; open that file in STM32CubeMX to compare at any step.

**Tested with:** Ubuntu 25.04, STM32CubeMX 6.17.0, STM32Cube FW_L4 V1.18.2 (FreeRTOS V10.3.1 with
ST's CMSIS-RTOS2 wrapper), STM32CubeIDE 2.1.0 (GNU Tools for STM32 14.3.rel1), CSP4CMSIS 3.1.0.
Windows: the STM32CubeMX and STM32CubeIDE dialogs are the same; not tested.

Conventions: **Bold** names are what you click or select. "You should now see ..." is a check:
if you do not see it, stop and compare with the committed `.ioc`.

---

## 8.1 Overview: what is configured and why

| Item | Setting | Why |
|---|---|---|
| MCU | STM32L475VGTx (LQFP100) | The B-L475E-IOT01A's microcontroller. Started from the bare MCU, not the board preset (8.3). |
| Clock | 80 MHz: MSI 4 MHz → PLL (/1, ×40, /2) | Maximum speed of the STM32L475; no crystal needed. |
| Debug | Serial Wire (PA13, PA14) | Reserves the ST-LINK pins so they cannot be given another function by mistake. |
| HAL timebase | **TIM6** | FreeRTOS uses SysTick for its tick; the HAL's `HAL_GetTick()` must use a different timer. |
| USART1 | Asynchronous, PB6/PB7, 115200 8N1 | Wired to the ST-LINK: appears as the virtual COM port (VCP) on the PC. |
| I2C2 | PB10/PB11, Standard mode 100 kHz, internal pull-ups | The on-board sensor bus: HTS221 (temperature, humidity), LPS22HB (pressure), and others. |
| SPI3 | Full-duplex master, PC10/PC11/PC12, 16 bit, 10 Mbit/s | Bus of the ISM43362 Wi-Fi module. Configured now, used from phase 2 on. |
| GPIO | PE0 CSN (high), PE8 RST, PB12 BOOT0, PB13 WAKEUP, PE1 DRDY (EXTI1, rising) | Control lines of the ISM43362, as in ST's own Wi-Fi driver. Unused in phase 1. |
| FreeRTOS | CMSIS_V2, heap 4096 B, newlib reentrant, stack overflow check 2, defaultTask static | See 8.5. |
| Project | STM32CubeIDE, generate under root, copy only necessary files | The layout CSP4CMSIS's integration guide expects. |

The pin assignments come from the board's schematic (user manual UM2153) and agree with ST's board
definition in STM32CubeMX (`B-L475E-IOT01A1`) and with ST's Wi-Fi example driver
(`Projects/B-L475E-IOT01A/Applications/WiFi/Common/Src/es_wifi_io.c` in STM32Cube FW_L4).

---

## 8.2 Install the STM32L4 MCU package

STM32CubeMX needs the STM32Cube FW_L4 package (HAL drivers, CMSIS, FreeRTOS) to generate code.

1. Start STM32CubeMX. **Help > Manage embedded software packages**, tab **STM32Cube MCU Packages**,
   expand **STM32L4**, tick **STM32Cube MCU Package for STM32L4 Series 1.18.2** (shown as 1.18.0 with
   patch 1.18.2), **Install**. Accept the licence.
   The download needs a free myST account (STM32CubeMX asks you to log in) and about 1 GB of disk.
2. You should now see a green box in front of 1.18.2, and a folder
   `~/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2` (Windows: `C:\Users\<you>\STM32Cube\Repository\...`).

**Without a myST login** (what was done for this repository, on a machine without one): ST publishes
the same package on GitHub. A shallow clone is about 1.3 GB:
```bash
git clone --depth 1 --branch v1.18.2 --recurse-submodules --shallow-submodules \
    https://github.com/STMicroelectronics/STM32CubeL4.git \
    ~/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2
```
Its `package.xml` identifies it as `FW.L4.1.18.0` with patch `FW.L4.1.18.2`, exactly like the
download; STM32CubeMX accepts it without further steps. (The folder may also be a symbolic link to
another disk; on this machine it is, because the home partition was nearly full.)

---

## 8.3 Create the project

1. **File > New Project**. Tab **MCU/MPU Selector**, **Commercial Part Number**: `STM32L475VGT6`.
   Select the line **STM32L475VGTx** (package LQFP100) in the list, **Start Project**.
2. You should now see the **Pinout & Configuration** tab with the 100-pin package, all pins grey.

**Why the MCU selector and not the Board Selector?** The board preset (`B-L475E-IOT01A1`) is
convenient, but even with "No" to "Initialize all peripherals with their default mode" it assigns a
function to about 80 pins (Arduino connector GPIOs, QSPI, DFSDM, USB, ...), and CubeMX then generates
initialisation code for every GPIO among them. Starting from the bare MCU, the project contains
exactly the 14 pins it uses, and the generated `MX_GPIO_Init()` is short enough to read. The pin
labels used below are the board preset's names, so the two remain easy to compare.

---

## 8.4 Pinout & Configuration

Pins are assigned by **left-clicking the pin** in the package view and choosing the function from the
list; a label is set with **right-click > Enter User Label**. Peripherals are enabled in the
**Categories** list on the left. When you enable a peripheral, CubeMX picks default pins that may not
be the board's pins: after each step, check the pins in the table and move them if needed (click the
correct pin and choose the signal; CubeMX moves the function there).

### 8.4.1 System Core > SYS

- **Debug: Serial Wire.** You should now see PA13 (SYS_JTMS-SWDIO) and PA14 (SYS_JTCK-SWCLK) green.
- **Timebase Source: TIM6.**

FreeRTOS takes over SysTick for the kernel tick. The HAL keeps its own millisecond counter for
`HAL_GetTick()`/`HAL_Delay()` and the timeouts of all HAL drivers; with TIM6 as its timebase the two
do not interfere. (With SysTick for both, CubeMX warns when generating code.) TIM6 is a basic timer
with nothing else to do on this board.

### 8.4.2 System Core > RCC and the clock tree

- **RCC**: leave **High Speed Clock (HSE)** and **Low Speed Clock (LSE)** **Disable**.
- Tab **Clock Configuration**:
  - **MSI RC**: 4000 kHz (the default, MSI range 6).
  - **PLL Source Mux**: **MSI**.
  - **/M**: **/1**, **\*N**: **X 40**, **/R**: **/2** (PLLCLK = 4 MHz × 40 / 2 = 80 MHz).
  - **System Clock Mux**: **PLLCLK**.
  - **AHB Prescaler /1, APB1 Prescaler /1, APB2 Prescaler /1.**
  - You should now see **HCLK (MHz) = 80**, and 80 MHz for the APB1 and APB2 peripheral clocks.

Typing `80` into the HCLK box and pressing Enter lets CubeMX find a configuration itself; check
that it chose the values above.

Not used: the board has a 32.768 kHz crystal (LSE) that could calibrate the MSI (MSI PLL mode) or
drive the RTC. The factory-trimmed MSI is accurate to a fraction of a percent near room temperature
(see the STM32L475 datasheet), well within what a UART at 115200 baud tolerates (a few percent),
so this project does without it. Use the LSE if you need accurate time over hours.

### 8.4.3 Connectivity > USART1 (ST-LINK virtual COM port)

- **Mode: Asynchronous**, **Hardware Flow Control (RS232): Disable**.
- Pins: **PB6 = USART1_TX**, **PB7 = USART1_RX**. CubeMX usually picks PA9/PA10 first; click PB6
  and choose USART1_TX, click PB7 and choose USART1_RX.
- Labels: PB6 `ST_LINK_UART1_TX`, PB7 `ST_LINK_UART1_RX`.
- **Parameter Settings**: leave the defaults: **Baud Rate 115200**, **Word Length 8 Bits**,
  **Parity None**, **Stop Bits 1**.

The board's ST-LINK/V2-1 forwards USART1 (PB6/PB7) to the PC as a virtual COM port
(Linux: `/dev/ttyACM*`). Only PB6/PB7 are wired to it, not PA9/PA10.

### 8.4.4 Connectivity > I2C2 (sensors)

- **I2C: I2C**.
- Pins: **PB10 = I2C2_SCL**, **PB11 = I2C2_SDA**; labels `INTERNAL_I2C2_SCL`, `INTERNAL_I2C2_SDA`.
- **Parameter Settings**: **I2C Speed Mode: Standard Mode**, **I2C Speed Frequency: 100 KHz**
  (defaults). You should now see **Timing: 0x10909CEC** (computed by CubeMX for a 80 MHz I2C clock).

- **GPIO Settings** (tab inside I2C2): **GPIO Pull-up/Pull-down: Pull-up** for PB10 and PB11.
  You should now see, after code generation, `GPIO_InitStruct.Pull = GPIO_PULLUP;` in
  `HAL_I2C_MspInit()` (`Core/Src/stm32l4xx_hal_msp.c`).

100 kHz is the conservative choice: all devices on the bus support it, and one sensor reading
moves only a few dozen bytes. The internal pull-ups (about 40 kOhm) are what ST's own board support
code enables on these pins (`I2Cx_MspInit()` in `stm32l475e_iot01.c`); they are harmless if the
board also has external pull-ups on this bus.

The sensor drivers do not configure I2C2 themselves in this project (8.7): CubeMX's
`MX_I2C2_Init()` is the only place.

### 8.4.5 Connectivity > SPI3 (Wi-Fi module; used from phase 2)

- **Mode: Full-Duplex Master**, **Hardware NSS Signal: Disable** (the module's chip select PE0 is
  driven by software).
- Pins: **PC10 = SPI3_SCK**, **PC11 = SPI3_MISO**, **PC12 = SPI3_MOSI**; labels
  `ISM43362_SPI3_SCK`, `ISM43362_SPI3_MISO`, `ISM43362_SPI3_MOSI`. If CubeMX placed SPI3 on PB3/PB4/PB5
  (or PA15), move each signal to PC10/PC11/PC12.
- **Parameter Settings**:
  - **Frame Format: Motorola**, **Data Size: 16 Bits**, **First Bit: MSB First**.
  - **Prescaler (for Baud Rate): 8** — you should now see **Baud Rate: 10.0 MBits/s**.
  - **Clock Polarity (CPOL): Low**, **Clock Phase (CPHA): 1 Edge**.
  - **CRC Calculation: Disabled**, **NSSP Mode: Disabled**.
- **GPIO Settings** (tab inside SPI3): **Maximum output speed: Medium** for all three pins;
  **PC11 (MISO): Pull-up**.

These are the values of ST's ISM43362 driver for this board (`es_wifi_io.c`: 16-bit frames, mode 0,
10 MHz — the module allows up to 20 MHz). The driver configures SPI3 itself; configuring it in CubeMX
as well documents the pins in the `.ioc`, and phase 2 decides which of the two initialisations stays.

### 8.4.6 GPIO: control lines of the Wi-Fi module

Click each pin and choose the function, then set it in **System Core > GPIO**:

| Pin | Function | GPIO output level | Mode | Pull | Speed | User label |
|---|---|---|---|---|---|---|
| PE0 | GPIO_Output | **High** | Output Push Pull | No pull | **Medium** | `ISM43362_SPI3_CSN` |
| PE8 | GPIO_Output | Low | Output Push Pull | No pull | Low | `ISM43362_RST` |
| PB12 | GPIO_Output | Low | Output Push Pull | No pull | Low | `ISM43362_BOOT0` |
| PB13 | GPIO_Output | Low | Output Push Pull | No pull | Low | `ISM43362_WAKEUP` |
| PE1 | GPIO_EXTI1 | — | **External Interrupt Mode with Rising edge trigger detection** | No pull | — | `ISM43362_DRDY_EXTI1` |

- CSN (chip select, active low) starts **high**: the module is not selected.
- RST (active low) starts **low**: the module stays in reset until phase 2's driver releases it,
  so in phase 1 the module is quiet. BOOT0 low: the module boots its own firmware, not its bootloader.
- DRDY rises when the module has data. **System Core > NVIC: leave "EXTI line1 interrupt" unticked**
  for now; phase 2 decides between polling DRDY and the interrupt.

You should now see 14 green pins: PA13, PA14, PB6, PB7, PB10, PB11, PB12, PB13, PC10, PC11, PC12,
PE0, PE1, PE8.

### 8.4.7 Middleware and Software Packs > FREERTOS

- **Interface: CMSIS_V2.**
- **Configuration > Config parameters**:
  - **Memory Allocation**: Dynamic / Static (the only choice with CMSIS_V2; sets
    `configSUPPORT_STATIC_ALLOCATION 1`, which CSP4CMSIS's static allocation needs).
  - **TOTAL_HEAP_SIZE: 4096** bytes. The application allocates nothing from it (8.9 checks this);
    the reserve is for experiments.
  - **CHECK_FOR_STACK_OVERFLOW: Option2.** At each context switch FreeRTOS checks the stack pointer
    and a 16-byte pattern at the end of the stack, and calls `vApplicationStackOverflowHook()` if
    either was violated. This project prints the thread's name and halts (`Core/Src/freertos.c`,
    `Core/Src/console_io.c`).
  - Everything else: default. In particular `configTICK_RATE_HZ` stays 1000 (1 tick = 1 ms) and
    `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` stays 5.
- **Configuration > Advanced settings > Newlib settings > USE_NEWLIB_REENTRANT: Enabled.**
  Every CSP process is a FreeRTOS thread; with this setting each thread gets its own newlib state
  (`errno`, `strtok`, ...). If it stays disabled, CubeMX asks at every code generation whether you
  want to continue.
- **Configuration > Tasks and Queues > defaultTask**, double-click:
  **Stack Size (Words): 128**, **Allocation: Static**, **Buffer Name: defaultTaskBuffer**,
  **Control Block Name: defaultTaskControlBlock**, **OK**.

  CubeMX creates this thread by default (in this project's headless generation it was added back
  even when the `.ioc` listed no thread), with dynamic allocation, i.e. from the FreeRTOS heap. Static
  allocation keeps the heap untouched. The thread does nothing: its body (`StartDefaultTask()` in
  `main.c`) calls `osThreadExit()`. The application runs in its own static MainApp thread (8.8).
- **System Core > NVIC**: check that **Priority Group** is **4 bits for pre-emption priority**,
  and that **Pendable request for system service** and **Time base: System tick timer** have
  priority 15 with **Uses FreeRTOS functions** ticked. CubeMX sets this when FreeRTOS is enabled;
  **Time base: TIM6 global interrupt** stays at 15 (`TICK_INT_PRIORITY`).

---

## 8.5 Project Manager

Tab **Project Manager**:

- **Project > Project Name**: `DISCO_L475VG_IOT01A_Thingspeak` (any name; it becomes the folder
  name). **Project Location**: the folder that will contain the project folder.
- **Application Structure: Advanced**, **Do not generate the main(): unticked**.
- **Toolchain / IDE: STM32CubeIDE**, **Generate Under Root: ticked** (the `.ioc`, `.project`,
  `.cproject` and `Core/` end up in the project folder itself).
- **Linker Settings**: Minimum Heap Size 0x200, Minimum Stack Size 0x400 (defaults; the "stack" is
  `main()`'s stack until the scheduler starts, and then the interrupt stack).
- **Mcu and Firmware Package**: **STM32Cube FW_L4 V1.18.2**, **Use Default Firmware Location: ticked**.
- **Code Generator > STM32Cube MCU packages and embedded software packs:
  Copy only the necessary library files.**
- **Code Generator > Generated files**: **Keep User Code when re-generating: ticked**,
  **Delete previously generated files when not re-generated: ticked**,
  **Generate peripheral initialization as a pair of '.c/.h' files per peripheral: unticked**
  (all initialisation stays in `main.c`, which is easier to read for a project this size).

**GENERATE CODE.** You should now see, in the project folder: `Core/`, `Drivers/`, `Middlewares/`,
`DISCO_L475VG_IOT01A_Thingspeak.ioc`, `.project`, `.cproject`, `.mxproject`,
`STM32L475VGTX_FLASH.ld`, `STM32L475VGTX_RAM.ld`. In `Core/Src/`: `main.c`, `freertos.c`,
`stm32l4xx_hal_msp.c`, `stm32l4xx_hal_timebase_tim.c`, `stm32l4xx_it.c`, `syscalls.c`, `sysmem.c`,
`system_stm32l4xx.c`.

Checks in the generated code:

| File | You should see |
|---|---|
| `Core/Src/main.c` | `RCC_OscInitStruct.PLL.PLLN = 40;`, `FLASH_LATENCY_4`, `hi2c2.Init.Timing = 0x10909CEC;`, `hspi3.Init.DataSize = SPI_DATASIZE_16BIT;`, `huart1.Init.BaudRate = 115200;`, `uint32_t defaultTaskBuffer[ 128 ];` |
| `Core/Inc/FreeRTOSConfig.h` | `configUSE_NEWLIB_REENTRANT 1`, `configSUPPORT_STATIC_ALLOCATION 1`, `configTOTAL_HEAP_SIZE ((size_t)4096)`, `configCHECK_FOR_STACK_OVERFLOW 2`, `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5` |
| `Core/Src/stm32l4xx_hal_timebase_tim.c` | `htim6.Instance = TIM6;` |
| `Core/Src/stm32l4xx_it.c` | `TIM6_DAC_IRQHandler()`, and **no** `SVC_Handler`, `PendSV_Handler`, `SysTick_Handler` (FreeRTOS's port provides them) |
| `Core/Src/stm32l4xx_hal_msp.c` | `HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);` |

---

## 8.6 STM32CubeIDE: import, C++, CSP4CMSIS

This follows CSP4CMSIS's `Documentation/CSP4CMSIS_STM32CubeIDE.md` (sections 2 and 3), with the
values for this board.

1. **Import.** STM32CubeIDE: **File > Import > General > Existing Projects into Workspace > Next**,
   **Select root directory**: the project folder, **Finish**. Do not tick "Copy projects into workspace".
2. **Convert to C++.** Project Explorer: right-click the project **> Convert to C++**, **Continue**.
   This adds the C++ nature to `.project` and moves the linker settings (linker script) to the
   **MCU/MPU G++ Linker**. It does not give the G++ compiler the C compiler's include paths and defines.
3. **Generate again** in STM32CubeMX (same `.ioc`, **GENERATE CODE**). For a C++ project CubeMX now
   writes its include paths and defines into the G++ compiler settings too. In STM32CubeIDE:
   select the project, **File > Refresh** (F5).
4. **Add the library source** (CSP4CMSIS 3.1.0, MIT licence), from a clone of
   <https://github.com/OliverFaust/CSP4CMSIS>:
   ```bash
   mkdir -p lib/csp4cmsis
   git -C <CSP4CMSIS clone> archive v3.1.0 csp4cmsis LICENSE | tar -x -C /tmp/csp
   cp -r /tmp/csp/csp4cmsis/. lib/csp4cmsis/ && cp /tmp/csp/LICENSE lib/csp4cmsis/
   ```
   (or download **Source code** of the v3.1.0 release and copy `csp4cmsis/` and `LICENSE`). Record the
   version: `lib/csp4cmsis/VERSION` in this repository names the tag and commit
   (`ddfcb9e1982fcbb8f2bbd2993ab7d16cd402c38e`).
5. **Source folder.** **Project > Properties > C/C++ General > Paths and Symbols > Source Location >
   Add Folder...**, select **`lib/csp4cmsis/src`**, **OK**.
6. **Compiler settings.** **Project > Properties > C/C++ Build > Settings**, **Configuration:
   [All configurations]**, **Tool Settings**:

   | Where | Setting |
   |---|---|
   | MCU/MPU G++ Compiler > General | **Language standard: GNU++17 (ISO C++17 + gnu extensions) (-std=gnu++17)** |
   | MCU/MPU G++ Compiler > Include paths | add `../lib/csp4cmsis/inc` and `../Drivers/BSP/B-L475E-IOT01` |
   | MCU/MPU G++ Compiler > Preprocessor | add `CSP4CMSIS_MAX_SYSCALL_INTERRUPT_PRIORITY=5` and `CSP4CMSIS_DEVICE_HEADER="stm32l4xx.h"` |
   | MCU/MPU GCC Compiler > Include paths | add `../Drivers/BSP/B-L475E-IOT01` |

   `CSP4CMSIS_MAX_SYSCALL_INTERRUPT_PRIORITY` must equal `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
   in `FreeRTOSConfig.h` (5, unshifted). `stm32l4xx.h` is the CMSIS device header of the STM32L4
   family (it defines `__NVIC_PRIO_BITS` = 4 and includes `core_cm4.h`). **Apply and Close.**

The project keeps all of this when CubeMX regenerates code: CubeMX rewrites only its own include
paths and defines in `.cproject` (checked for this repository: after regeneration `.cproject` differs
only in the order of the source folders).

---

## 8.7 Board support: the sensor drivers

The project was started from the bare MCU, so STM32CubeMX adds no board support files; ST's
B-L475E-IOT01 BSP is copied by hand from
STM32Cube FW_L4 (`~/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Drivers/BSP/`) into the project's
`Drivers/BSP/`, which is already a source folder. Only the sensor parts are needed:

```
Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01.h              (header only)
Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_tsensor.{c,h}  temperature (HTS221)
Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_hsensor.{c,h}  humidity    (HTS221)
Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_psensor.{c,h}  pressure    (LPS22HB)
Drivers/BSP/B-L475E-IOT01/LICENSE.md                      BSD-3-Clause
Drivers/BSP/Components/hts221/hts221.{c,h}
Drivers/BSP/Components/lps22hb/lps22hb.{c,h}
Drivers/BSP/Components/Common/{hsensor,psensor,tsensor}.h
```

`stm32l475e_iot01.c` is **left out on purpose**. Besides LEDs and buttons it contains its own I2C2
set-up (its own handle `hI2cHandler`, pins and timing) behind the functions `SENSOR_IO_Init/Read/Write/
ReadMultiple` that the sensor drivers call. Two initialisations of one peripheral would compete for
it. Instead, `Core/Src/sensor_io.c` implements those functions on `hi2c2`, the handle CubeMX
generates — CubeMX is the single place where I2C2 is configured, and the Sensors process is the
single thread that uses it. `Drivers/BSP/README.md` lists the source and the commits of the copied
files. The BSP initialises both sensors and converts pressure; temperature and humidity are converted
by `Core/Src/hts221_calib.c` instead, because ST's HTS221 driver truncates the calibration points
(chapter 5). STM32CubeMX does not delete them when it regenerates code (checked).

---

## 8.8 Where the application hooks into the generated code

CubeMX regenerates `main.c`, `freertos.c` and `FreeRTOSConfig.h`, but keeps everything between
`/* USER CODE BEGIN x */` and `/* USER CODE END x */`. The application uses only these sections; all
other code is in its own files (`Core/Src/*.cpp`, `console_io.c`, `sensor_io.c`), which CubeMX leaves
alone.

| File, section | Added | Why |
|---|---|---|
| `main.c`, `Includes` | `#include <stdio.h>`, `#include "app.h"` | |
| `main.c`, `2` (after peripheral init) | `setvbuf(stdout, NULL, _IONBF, 0);` | Unbuffered stdout: newlib would otherwise `malloc()` a 1 KB buffer on the first `printf()`. |
| `main.c`, `RTOS_THREADS` | `csp_app_main_init();` | Creates the static MainApp thread (`Core/Src/app.cpp`), which starts the process network once the scheduler runs. |
| `main.c`, `5` (body of `StartDefaultTask`) | `osThreadExit();` | The CubeMX default thread is not used. |
| `FreeRTOSConfig.h`, `1` | `configASSERT` calling `vAssertCalled()` | A failed FreeRTOS assertion prints file and line instead of halting silently. |
| `freertos.c`, `4` | body of `vApplicationStackOverflowHook()` | Prints the name of the thread that overran its stack. |

`printf()` reaches USART1 through `_write()` in `Core/Src/console_io.c`, which replaces the weak
`_write()` of the generated `syscalls.c`.

---

## 8.9 Build, flash, check

Headless build (no IDE window), as used for this repository; `-cleanBuild` builds from scratch:

```bash
~/st/stm32cubeide_2.1.0/headless-build.sh -data /tmp/ws \
    -import "$PWD" -cleanBuild DISCO_L475VG_IOT01A_Thingspeak/Debug
```

You should now see `Build Finished. 0 errors, 0 warnings.` and the size table, for example
(Debug, -O0): `text 54384  data 152  bss 16776`.

Flash, reset and record the console with `tools/flash_and_capture.sh` (chapter 3). After the
capture the script reads two variables over SWD while the program runs:
`__sbrk_heap_end` (newlib's heap; `0x00000000` = `malloc()` never called) and
`xMinimumEverFreeBytesRemaining` (FreeRTOS's heap_4; `0x00000000` = the heap was never initialised,
i.e. nothing was ever allocated from it). Both 0 shows that the program allocates no memory
dynamically. The results of this repository are in `docs/results/`.

---

## 8.10 How this repository's project was actually produced

The steps above are the GUI route. For this repository the same settings were applied with
STM32CubeMX's command-line mode, so that every step is a file that can be checked:

1. `STM32CubeMX -q script.txt` with `load STM32L475VGTx` and `config saveext` wrote a complete
   `.ioc` of the bare MCU (and `loadboard B-L475E-IOT01A1 nomode` one of the board, for its pin
   labels and clock tree).
2. A text `.ioc` with the settings of 8.4–8.5 was written from those two, then loaded with
   `config load` and generated with `project generate`. STM32CubeMX normalises the file when it
   saves it (sorted pins, its own NVIC record format); the committed `.ioc` is that normalised file.
3. Three settings had to be corrected after the first generation, all visible in the generated code:
   CubeMX added the `defaultTask` thread with **dynamic** allocation (fixed: Static, 8.4.7); it set
   SPI3 **NSSP Mode** to *Enabled* (fixed: Disabled, as in ST's driver; it has no effect with
   software NSS, but the `.ioc` should say what the driver uses); and the I2C2 pull-ups were missing
   (added, 8.4.4: in the `.ioc` the parameter of an I2C pin is `GPIO_Pu`, not `GPIO_PuPd` as for
   other pins, and CubeMX ignores the wrong one without a warning).
4. The C++ conversion was done in the files: the nature `org.eclipse.cdt.core.ccnature` in
   `.project` (what **Convert to C++** adds), a further `project generate`, then the settings of 8.6
   in `.cproject`, and a final regeneration to confirm that CubeMX keeps them.

Opening the committed `.ioc` in the STM32CubeMX GUI shows the same configuration as the GUI route.
