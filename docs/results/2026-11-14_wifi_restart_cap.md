# Restart cap after failed Wi-Fi starts (2026-11-14)

Change: after `APP_WIFI_MAX_RESTARTS` (3) consecutive restarts because the Wi-Fi driver could not be
initialised, the board stops restarting and runs Sensors and Console without the Uplink, printing
"Wi-Fi unavailable after 3 attempts - see docs/07 section 7.1 - press RESET to retry" once a minute.
The counter survives only the Uplink's own restarts (marker in `.noinit` plus software-reset flag);
every other start, and a successful `Initialize()`, sets it to 0. Code: `Core/Src/reset_info.c`,
`Core/Src/app.cpp` (`MainApp_Task()`), `Core/Src/console_process.cpp`; walkthrough
`docs/05-code-walkthrough.md` ("When the Wi-Fi module does not start").

## 1. Module held in reset (`APP_TEST_WIFI_INIT_FAIL 1`)

`tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 135 <raw>`, masked:
`2026-11-14_wifi_restart_cap_console.txt` (Debug, 0 errors, 0 warnings).

```
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 0 of 3
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED (FreeRTOS heap free 640, minimum ever 640 bytes): restarting the board in 5 s
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 1 of 3
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED (FreeRTOS heap free 640, minimum ever 640 bytes): restarting the board in 5 s
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 2 of 3
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED (FreeRTOS heap free 640, minimum ever 640 bytes): restarting the board in 5 s
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console (Uplink NOT started: Wi-Fi unavailable after 3 failed starts)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 3 of 3
[    0.114 s] Wi-Fi unavailable after 3 attempts - see docs/07 section 7.1 - press RESET to retry
[   60.000 s] Wi-Fi unavailable after 3 attempts - see docs/07 section 7.1 - press RESET to retry
  FreeRTOS heap: never used (1024 bytes)
```

- **Exactly 3 restarts**, then Sensors and Console only; no further restart until the end of the
  capture (a first run of the same test, 180 s, showed the notice at 0.1, 60 and 120 s and 33 samples
  in the sensors-only phase).
- **The first start after flashing counts 0:** the programmer's reset is not one of the Uplink's
  restarts (no marker), although it also sets the software-reset flag.
- **Heap:** at each failure 640 of 1024 bytes free (the 384 bytes the failed `Initialize()` leaked,
  see `2026-11-12_uplink_recovery.md`), the same every time; in the sensors-only phase the FreeRTOS
  heap is never used (`xMinimumEverFreeBytesRemaining = 0` over SWD: heap_4 not even initialised).
  `__sbrk_heap_end = 0` (no `malloc()`).

## 2. Reset from the sensors-only state

Board left in the state of section 1 ("3 of 3"), then reset by the debugger without reflashing
(`STM32_Programmer_CLI -c port=SWD -hardRst`): `2026-11-14_wifi_restart_cap_retry_console.txt`.

```
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 0 of 3
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED ...: restarting the board in 5 s
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 1 of 3
```

The counter starts again at 0 and the board makes three new attempts, as "press RESET to retry"
promises. The RESET button itself was not pressed for this test; it is another reset without the
marker and takes the same path.

## 3. Rejoin reason on the console

The `Rejoining` message now says why: `connection lost (driver reports not connected): leaving the
network, joining again` when `IsConnected()` is false after a failed upload, and `3 failed uploads in a
row: leaving the network, joining again` otherwise. Not re-recorded on the board with an access point
outage; the AP-off run (`2026-11-14_ap_off.md`) took the first path both times.
