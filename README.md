# DISCO_L475VG_IOT01A_Thingspeak

The B-L475E-IOT01A Discovery kit measures **temperature, humidity and air pressure** every 5 s and
sends them over its **Wi-Fi module** to a **ThingSpeak** channel once a minute. The program is written
as a network of communicating processes with **CSP4CMSIS** on **FreeRTOS** (CMSIS-RTOS2), built with
**STM32CubeIDE**.

Author: Dr Oliver Faust. Licence: MIT (`LICENSE`); third-party parts: `THIRD_PARTY_LICENSES.md`.

> **Looking for the Mbed OS version?** Mbed OS reached its end of life in July 2026. The original Mbed
> Studio project is kept, unchanged, on the branch
> [`mbed-os-legacy`](https://github.com/OliverFaust/DISCO_L475VG_IOT01A_Thingspeak/tree/mbed-os-legacy)
> (tag `mbed-os-final`). What changed, and why: [docs/migrating-from-mbed.md](docs/migrating-from-mbed.md).

---

## Hardware

- **B-L475E-IOT01A** (STM32L475VG, Arm Cortex-M4, 80 MHz) with its on-board sensors HTS221
  (temperature, humidity) and LPS22HB (pressure), the Inventek **ISM43362** Wi-Fi module and the
  ST-LINK debugger with a virtual serial port.
- A USB cable (micro-B) to the board's **ST-LINK** socket, CN7.
- A **2.4 GHz WPA2-Personal** Wi-Fi network on channel 1-11 with internet access. The module supports
  neither 5 GHz, WPA3-only, nor WPA2-Enterprise -- **eduroam does not work**; use a phone hotspot or a
  small router ([docs/01](docs/01-prerequisites.md), 1.2).
- A free [ThingSpeak](https://thingspeak.mathworks.com) account.

## What you learn

- Structuring an embedded program as **processes that share nothing but channels** (CSP), instead of
  one loop with shared variables -- and why that keeps the sensors running while the network fails.
- **ALTs with timeouts**: waiting for several events at once without polling.
- **Ownership** of hardware (I2C, SPI, UART) by exactly one process; no mutexes.
- **Buffered channels with KeepNewest**: no writer ever waits for a slow reader.
- An **STM32CubeMX/STM32CubeIDE** project with FreeRTOS, static allocation and C++17.
- Using a **Wi-Fi module and a REST API** robustly: join, DNS, HTTP, response check, back-off, rate limit.
- **Tracing** the running program and reading the result as a timeline in Perfetto.

## Quick start

Details and checks for every step: [docs/03-build-and-run.md](docs/03-build-and-run.md).

1. Install STM32CubeIDE 2.1.0, a serial terminal and git; allow access to the board
   ([docs/01](docs/01-prerequisites.md)).
2. Create a ThingSpeak channel with **fields 1, 2 and 3 enabled**; note its Write API Key
   ([docs/02](docs/02-thingspeak.md)).
3. Clone:
   ```bash
   git clone https://github.com/OliverFaust/DISCO_L475VG_IOT01A_Thingspeak.git
   cd DISCO_L475VG_IOT01A_Thingspeak
   ```
4. Enter your secrets -- copy the template, then fill in SSID, password and write key:
   ```bash
   cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h
   ```
   Without this step the build stops with an `#error` that prints this command.
   `app_secrets.h` is git-ignored; never commit it.
5. Optional, to check uploads from the PC with `tools/thingspeak_feed.sh` -- copy and fill in the
   channel ID and read key:
   ```bash
   cp tools/thingspeak_host.env.template tools/thingspeak_host.env
   ```
6. STM32CubeIDE: **File > Import > General > Existing Projects into Workspace**, select the cloned
   folder, **Finish**; **Project > Build Project** (`Build Finished. 0 errors, 0 warnings.`).
7. Open a serial terminal on the ST-LINK port with 115200 8N1, e.g.
   `minicom -D /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_*-if02 -b 115200`.
8. **Run > Run As > STM32 C/C++ Application**. You should now see a sample every 5 s, `Uplink: joined`
   within about 10 s, and `Uplink: sample #N -> entry M` once a minute -- and the values on your
   ThingSpeak channel.

## Documentation

| Chapter | Contents |
|---|---|
| [01 Prerequisites](docs/01-prerequisites.md) | board, Wi-Fi requirements, tools and versions, USB access, serial terminal |
| [02 ThingSpeak](docs/02-thingspeak.md) | channel, fields, keys, rate limit, checking uploads from the PC |
| [03 Build and run](docs/03-build-and-run.md) | clone, secrets, import, build, flash, expected console output |
| [04 Architecture](docs/04-architecture.md) | the process network, channels, ownership, KeepNewest, priorities, memory |
| [05 Code walkthrough](docs/05-code-walkthrough.md) | the processes explained against the source |
| [06 Tracing](docs/06-tracing.md) | Debug-Trace build, ring dump, decoding, Perfetto, how to read the timeline |
| [07 Troubleshooting](docs/07-troubleshooting.md) | Wi-Fi start and join problems, rejected uploads, diagnostic modes |
| [08 How it was made](docs/08-how-it-was-made.md) | the CubeMX/CubeIDE project from scratch, for your own projects |
| [09 Exercises](docs/09-exercises.md) | graded extensions with checks |
| [Migrating from Mbed](docs/migrating-from-mbed.md) | old construct -> new construct, bugs fixed |

Records of every test on the board (console logs, ThingSpeak feeds, traces): [docs/results/](docs/results/).
Notes for upstream projects: [docs/upstream/](docs/upstream/).

## Repository layout

| Path | Contents |
|---|---|
| `Core/` | application (`app.cpp`, `*_process.cpp`, ...) and the code STM32CubeMX generated |
| `DISCO_L475VG_IOT01A_Thingspeak.ioc` | STM32CubeMX configuration (pins, clocks, FreeRTOS) |
| `Drivers/`, `Middlewares/` | ST HAL, CMSIS, board support, Arm's ISM43362 Wi-Fi driver, FreeRTOS |
| `lib/csp4cmsis/` | CSP4CMSIS 3.1.0 (Debug and Release); `lib/csp4cmsis-trace/`: tracing version (Debug-Trace) |
| `tools/` | flash and capture, ThingSpeak feed, trace dump and decode, secrets check |
| `docs/` | the documentation above |
