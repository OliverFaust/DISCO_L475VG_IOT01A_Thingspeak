# 3. Build and run

From an empty folder to a board that sends its measurements to ThingSpeak. You need chapter 1
(tools, board access, serial terminal) and chapter 2 (channel and write key) done.

**Tested on:** Ubuntu 25.04 with STM32CubeIDE 2.1.0, November 2026 (`docs/results/` holds the
records). **Windows:** the STM32CubeIDE steps are the same; use `copy` instead of `cp`, PuTTY or Tera
Term instead of minicom, and Git Bash or WSL for the scripts. Not tested.

---

## 3.1 Get the project

```bash
cd ~                                        # or wherever you keep your projects
git clone https://github.com/OliverFaust/DISCO_L475VG_IOT01A_Thingspeak.git
cd DISCO_L475VG_IOT01A_Thingspeak
```

You should now see (`ls`): `Core  DISCO_L475VG_IOT01A_Thingspeak.ioc  docs  Drivers  lib
LICENSE  Middlewares  README.md  STM32L475VGTX_FLASH.ld  ...  tools`. The `.ioc` file and all code
that STM32CubeMX generated from it are in the repository, so you do **not** need STM32CubeMX to build.

Path names: keep the folder path free of spaces and special characters; Eclipse-based tools are not
reliably tested with them.

---

## 3.2 Enter your secrets

The Wi-Fi password and the ThingSpeak write key are not in the repository. Copy the template **exactly
like this** (in the project folder):

```bash
cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h
```

Open `Core/Inc/app_secrets.h` in an editor and replace the three values, keeping the quotes:

```c
#define APP_WIFI_SSID            "your-ssid"          /* your 2.4 GHz WPA2 network (chapter 1.2) */
#define APP_WIFI_PASSWORD        "your-password"
#define APP_THINGSPEAK_WRITE_KEY "XXXXXXXXXXXXXXXX"   /* Write API Key (chapter 2.2) */
```

The SSID is case-sensitive and must match the network's name exactly, including spaces.

`app_secrets.h` is listed in `.gitignore`: git does not offer it for commits. Check:

```bash
git status --short
```

You should now see **no** line with `app_secrets.h`. (If you forget the copy step, the build stops
with `#error "Core/Inc/app_secrets.h is missing. In the project folder run:  cp
Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h ..."`.)

Optional, for checking uploads from the PC (chapter 2.4):

```bash
cp tools/thingspeak_host.env.template tools/thingspeak_host.env
```

and fill in the channel ID and read key.

---

## 3.3 Import into STM32CubeIDE

1. Start STM32CubeIDE. In the **Workspace** dialog choose a **new, empty** folder **outside** the
   project folder, e.g. `~/stm32ws`, and click **Launch**. (The workspace holds only the IDE's
   settings; the project stays where you cloned it.)
2. Close the *Information Center* tab if it opens.
3. **File > Import... > General > Existing Projects into Workspace > Next**.
4. **Select root directory > Browse...**: the cloned folder `DISCO_L475VG_IOT01A_Thingspeak`, **Open**.
5. Under **Projects** you should now see `DISCO_L475VG_IOT01A_Thingspeak` ticked. **Do not** tick
   *Copy projects into workspace*. Click **Finish**.

You should now see the project in the **Project Explorer** on the left, with the folders `Core`,
`Drivers`, `lib`, `Middlewares`, ... (and `Includes`).

---

## 3.4 Build

1. Select the project in the Project Explorer.
2. **Project > Build Configurations > Set Active > Debug** (the default).
3. **Project > Build Project** (or the hammer icon in the toolbar).

The **Console** view at the bottom shows the compiler calls. At the end you should now see

```
   text	   data	    bss	    dec	    hex	filename
 109664	    152	  48376	 158192	  269f0	DISCO_L475VG_IOT01A_Thingspeak.elf
Finished building: default.size.stdout
...
Build Finished. 0 errors, 0 warnings.
```

The numbers can differ by a few bytes if you changed the secrets (the SSID, password and key are
part of the program). **Release** (optimised, `-Os`) gives a smaller program, about `text 63984`.
**Debug-Trace** is the tracing build of chapter 6.

**Without the GUI** (same result; the workspace folder must not be the project folder):

```bash
~/st/stm32cubeide_2.1.0/headless-build.sh -data ~/stm32ws -import "$PWD" -build DISCO_L475VG_IOT01A_Thingspeak/Debug
```

(After the first `-import`, use `-build` alone; `-cleanBuild` builds from scratch.)

---

## 3.5 Open the console

Connect the board (CN7) and start the serial terminal of chapter 1.5 **before** you flash, so that
you see the program from its first line:

```bash
minicom -D /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_*-if02 -b 115200
```

(When the terminal opens, it may first show a few old lines that the PC had received earlier; they
end at the next start banner.)

---

## 3.6 Flash and run

1. In STM32CubeIDE: select the project, **Run > Run As > STM32 C/C++ Application**.
2. The first time, a dialog **Edit Configuration** opens: leave the defaults (*Debugger* tab: ST-LINK
   GDB server, SWD) and click **OK** (or **Run**).
3. If STM32CubeIDE offers to upgrade the ST-LINK firmware, you may accept or decline; the project was
   tested with ST-LINK firmware V2J46M31.

The Console view shows the programmer's output; you should now see
`Download verified successfully` and the board restarts with the new program.

**Without the GUI:** `tools/flash_and_capture.sh` flashes, resets the board and records the console
for a given time:

```bash
tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 90 run.txt
```

(Stop minicom first: only one program can read the serial port. Then `less run.txt`.)

---

## 3.7 What you should see

The serial terminal shows (values, times and numbers will differ):

```
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
CSP4CMSIS 3.1.0, FreeRTOS V10.3.1 (CMSIS-RTOS2), SYSCLK 80 MHz
Reset cause: reset pin (button or debugger); consecutive restarts after a failed Wi-Fi start: 0 of 3
[    0.029 s] HTS221 check (I2C transfers all OK)
  WHO_AM_I: HTS221 @0xBE: 0xBC (expect 0xBC), LPS22HB @0xBA: 0xB1 (expect 0xB1)
  ...                                       (8 more lines of the sensor check)
[    0.037 s] #1  T = 14.23 C  RH = 82.01 %  p = 982.52 hPa
[    0.926 s] Uplink: Wi-Fi module ready  [ok 0, rejected 0, failed 0, joins 0]
[    5.041 s] #2  T = 14.22 C  RH = 82.06 %  p = 982.47 hPa
[    5.723 s] Uplink: joined (4789 ms), IP 192.168.1.28  [ok 0, rejected 0, failed 0, joins 1]
[    7.066 s] Uplink: sample #2 -> entry 235693 (335 ms)  [ok 1, rejected 0, failed 0, joins 1]
[   10.044 s] #3  T = 14.29 C  RH = 82.06 %  p = 982.46 hPa
...
[   60.000 s] Stack high-water marks (smallest free stack so far):
  FreeRTOS heap: free 640 of 1024 bytes, minimum ever free 640 bytes
  Sensors   512 words, min free  301 words, max used  211 words (41 %)
  Console   512 words, min free  237 words, max used  275 words (53 %)
  Uplink   1024 words, min free  761 words, max used  263 words (25 %)
[   60.074 s] #13  T = 14.89 C  RH = 81.58 %  p = 982.41 hPa
[   65.077 s] #14  T = 14.91 C  RH = 81.45 %  p = 982.43 hPa
[   67.394 s] Uplink: sample #14 -> entry 235694 (328 ms)  [ok 2, rejected 0, failed 0, joins 1]
```

Check, line by line:

| Line | Meaning | If not |
|---|---|---|
| `Reset cause: ...` | why the board started: `reset pin (button or debugger)` after flashing or the RESET button, `power-on/brown-out` after plugging in, `software reset` after a restart by the program or `tools/flash_and_capture.sh` | -- |
| `HTS221 check (I2C transfers all OK)`, `WHO_AM_I ... (expect ...)` equal | the sensors answer on I2C2 | chapter 5 (HTS221) |
| `#1  T = ... RH = ... p = ...` every 5 s | Sensors and Console work | -- |
| `Uplink: Wi-Fi module ready` within about 1 s | the Wi-Fi module started | chapter 7, 7.1 |
| `Uplink: joined (... ms), IP ...` within about 10 s | your network accepted the board | chapter 7, 7.4 (and chapter 1.2) |
| `Uplink: sample #N -> entry M` | ThingSpeak stored the update as entry M | chapter 7, 7.2 (`REJECTED`) |
| further `-> entry` lines every 60 s | the upload period | -- |
| stack report at 60 s, then every 10 minutes | free stack of each process; `min free` must stay well above 0 | chapter 7 |

Then check on ThingSpeak: the channel's *Private View* shows one new point per minute in each of the
three charts; or from the PC: `tools/thingspeak_feed.sh 3` (chapter 2.4) -- the newest `entry_id`
equals the last `-> entry` on the console.

---

## 3.8 Next

- Chapter 4 explains the program's structure, chapter 5 the code.
- Chapter 6 shows the program at work in a trace (build configuration Debug-Trace).
- Chapter 7 helps when something above did not work.
- Chapter 9 has exercises.
