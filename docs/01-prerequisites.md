# 1. Prerequisites

What you need before you build the project: the board, a suitable Wi-Fi network, the tools, access
to the board's USB interfaces, and a serial terminal. Every step ends with a check ("You should now
see ..."); do not go on until it passes.

**Tested on:** Ubuntu 25.04 (x86-64), November 2026. **Windows:** the ST tools exist for Windows and
are used in the same way in their GUI; the shell scripts in `tools/` need Git Bash or WSL. Windows was
**not tested**; differences that are known are noted.

---

## 1.1 Hardware

| Item | Notes |
|---|---|
| **B-L475E-IOT01A** Discovery kit for IoT node (STM32L475VG) | Tested with one board whose on-board ST-LINK/V2-1 reports firmware `V2J46M31`. |
| USB cable, USB-A (or USB-C) to **micro-B** | Must carry data, not only power. Connect it to **CN7**, the ST-LINK's micro-B socket, not to CN13 (the STM32's own USB OTG socket). |

Through CN7 the board gets power and the PC gets two interfaces of the on-board ST-LINK: the debug
probe (flashing, debugging) and a virtual COM port (VCP) connected to the STM32's USART1, on which the
program prints.

The board carries an **Inventek ISM43362** Wi-Fi module. The reference module firmware (lab board,
2026-11-12) is

```
ISM43362-M3G-L44-SPI,C3.5.2.7.STM,v3.5.2,v1.4.0.rc1,v8.2.1,120000000,Inventek eS-WiFi
```

The program prints the version of your module only in its Wi-Fi bring-up mode (chapter 7). If it
differs and the Wi-Fi does not work, compare with this string first.

---

## 1.2 Wi-Fi network: what the module supports

The ISM43362 is old and limited. Your network must be:

| Requirement | Why |
|---|---|
| **2.4 GHz** | The module has no 5 GHz radio. |
| **Channel 1 to 11** | In tests the module found networks on channels 1-11 only; a network on channel 13 was not seen (`docs/results/2026-11-14_ap_off.md`). |
| **WPA2-Personal** (or WPA-Personal), i.e. one shared password | The module supports neither WPA3 nor WPA2-*Enterprise* (user name + password, certificates). |

Consequences:

- **eduroam and most university networks do not work** (WPA2-Enterprise).
- **WPA3-only networks do not work.** Mixed "WPA2/WPA3" mode does.
- **In a lab, use a 2.4 GHz WPA2 hotspot on channel 1-11**: a phone hotspot or a small travel router.
  Some phones offer a 5 GHz hotspot only (one of the phones tried for this project could not be set
  to 2.4 GHz at all); others choose a channel themselves -- if yours does not work, check its band
  and channel with another device (e.g. `nmcli -f SSID,CHAN,FREQ,SECURITY dev wifi` on a Linux
  laptop).
- The network needs DHCP and access to the internet (HTTP, port 80, to `api.thingspeak.com`).

You should now have: an SSID and a password of a 2.4 GHz WPA2-Personal network on channel 1-11 that
can reach the internet.

---

## 1.3 Software

| Tool | Version used | Needed for |
|---|---|---|
| Ubuntu | 25.04 | -- |
| **STM32CubeIDE** | **2.1.0** | building, flashing, debugging (chapter 3). Contains the compiler (GNU Tools for STM32 14.3.rel1), the ST-LINK GDB server and a copy of STM32CubeProgrammer 2.22.0. |
| git | 2.48 | cloning the repository |
| a serial terminal | minicom 2.x (or picocom, screen) | reading the console (1.5) |
| Python 3 | 3.13 (standard library only) | the scripts in `tools/` |
| curl | any | `tools/thingspeak_feed.sh` (checking uploads from the PC) |
| STM32CubeProgrammer | 2.22.0 (optional) | the CLI is also inside STM32CubeIDE; `tools/` finds either |
| STM32CubeMX | 6.17.0 with STM32Cube FW_L4 V1.18.2 (optional) | only to change the hardware configuration (`.ioc`) and regenerate code (chapter 8). STM32CubeIDE 2.x no longer contains CubeMX. |
| Google Chrome or Chromium | 141 | the trace viewer <https://ui.perfetto.dev> (chapter 6, optional) |
| ProB (`probcli`) | 1.16.1 (optional) | conformance checks of traces (chapter 6, optional) |

### Install

1. **STM32CubeIDE 2.1.0** from <https://www.st.com/en/development-tools/stm32cubeide.html> (st.com
   may ask you to log in to a free myST account): a Linux installer for your distribution. Run it and
   accept the licences. You also need ST's **ST-LINK udev rules** (Debian package
   `st-stlink-udev-rules`; on the test machine version 1.0.3, installed with ST's tools) -- 1.4 checks
   them. On the test machine the IDE is in `~/st/stm32cubeide_2.1.0`; the scripts in `tools/` look there and in
   `/opt/st/stm32cubeide_2.1.0`. If yours is elsewhere, set `CUBEIDE` to its folder
   (`export CUBEIDE=/path/to/stm32cubeide_2.1.0`) before using the scripts.
2. The rest from Ubuntu's packages:
   ```bash
   sudo apt install git minicom curl python3
   ```

You should now see:

```bash
$ ls ~/st/stm32cubeide_2.1.0/stm32cubeide        # (or your installation folder)
/home/<you>/st/stm32cubeide_2.1.0/stm32cubeide
$ git --version; python3 --version
git version 2.48.1
Python 3.13.3
```

**Windows:** install STM32CubeIDE with its Windows installer; it installs the ST-LINK USB driver.
Use PuTTY or Tera Term as serial terminal. Not tested.

---

## 1.4 Access to the board's USB interfaces (Linux)

Two permissions are needed: the debug probe (udev rules, 1.3) and the serial port (group `dialout`).

1. Check that the udev rules are installed:
   ```bash
   ls /etc/udev/rules.d | grep -i stlink
   ```
   You should now see `49-stlinkv2-1.rules` among others. If not, install the package
   `st-stlink-udev-rules` (it comes with STM32CubeIDE's and STM32CubeProgrammer's installers), or
   create `/etc/udev/rules.d/49-stlinkv2-1.rules` with the rule for the board's ST-LINK/V2-1
   (USB ID `0483:374b`):
   ```
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="660", GROUP="plugdev", TAG+="uaccess", ENV{ID_MM_DEVICE_IGNORE}="1"
   ```
   and reload: `sudo udevadm control --reload-rules && sudo udevadm trigger`.
2. Allow yourself to use serial ports:
   ```bash
   sudo usermod -aG dialout "$USER"
   ```
   then **log out and in again** (or reboot). Check: `id -nG` lists `dialout`.
3. Connect the board (CN7). Its LED LD1 (*COM*) lights up.
   ```bash
   ls -l /dev/serial/by-id/
   ~/st/stm32cubeide_2.1.0/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*/tools/bin/STM32_Programmer_CLI -l st-link
   ```
   You should now see a link `usb-STMicroelectronics_STM32_STLink_<serial number>-if02 -> ../../ttyACM0`
   (the VCP; the number after `ttyACM` may differ) and, from the programmer, a probe with
   `ST-LINK FW  : V2J..` and `Board Name  : STM32L4IO`.

If the programmer reports no probe: another cable or USB port; the board plugged into CN7; the udev
rules reloaded; ModemManager can grab the VCP for a few seconds after plugging in -- the rule's
`ID_MM_DEVICE_IGNORE` prevents that.

---

## 1.5 Serial terminal

The program prints on the VCP with **115200 baud, 8 data bits, no parity, 1 stop bit (8N1), no flow
control**. Use the stable by-id name instead of `/dev/ttyACM0`, which can change when other devices
are plugged in:

```bash
minicom -D /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_*-if02 -b 115200
```

Leave minicom with **Ctrl-A**, then **X**, **Enter**. Alternatives: `picocom -b 115200 <port>` (leave
with Ctrl-A Ctrl-X) or `screen <port> 115200` (Ctrl-A K); the scripts in `tools/` read the port
themselves.

Press the black **RESET** button (B2) on the board. If the board already holds this project's program,
you should now see its start banner (chapter 3); with any other program, whatever it prints, or
nothing. If nothing ever appears, check the port name and that you are in the group `dialout` (1.4).

Next: chapter 2 (the ThingSpeak channel).
