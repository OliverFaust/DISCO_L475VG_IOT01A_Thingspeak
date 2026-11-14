# Phase 3, step 3: the Uplink process -- soak and recovery (2026-11-12)

Network: Sensors (I2C2, every 5 s) -> Console, Sensors -> Uplink (Wi-Fi, ThingSpeak every 60 s),
Uplink -> Console (status). All channels buffered, KeepNewest. Uplink at `osPriorityLow`, Sensors and
Console at `osPriorityBelowNormal`. Code: `Core/Src/uplink_process.cpp`, walkthrough
`docs/05-code-walkthrough.md` (5.2). Debug build, 0 errors, 0 warnings (text 107204, data 152,
bss 48272). Logs masked with `tools/mask_log.py` (the Uplink never prints the SSID or the key).

## 1. Soak: 31 minutes (18:40:07-19:11:07 UTC)

`2026-11-12_uplink_soak_console.txt`, `2026-11-12_uplink_soak_feed.txt`;
`tools/compare_uploads.py <console> <feed>`:

```
device: 31 uploads OK, 0 rejected, 0 failed
feed: 31 entries with ids 235578..235608
RESULT: all uploads match the feed
```

- **Uploads sent = entries on ThingSpeak:** 31 = 31, ids 235578..235608 without gaps, field1..3 of
  every entry equal to the values the Console printed for that sample. No failed or rejected upload.
- **One join** (8.8 s), no reconnect. 375 samples, none overwritten, no sensor error.
- **Upload interval 60.325..60.376 s** (mean 60.340 s) = 60 s + the request time (325..376 ms, mean
  340 ms): the period runs from the end of an attempt (relative timeout, exercise 9.1).
- **FreeRTOS heap, minimum ever free: 640 of 1024 bytes** at 60, 660, 1260 and 1860 s, and over SWD
  after the run (`xMinimumEverFreeBytesRemaining = 0x280`). The driver's 384 bytes are the only use;
  opening and closing 31 sockets did not change it. `__sbrk_heap_end = 0` (no `malloc()`).
- **Stack high-water marks** (the same at all four reports, i.e. reached in the first minute):

  | Process | Stack | Max used | Min free |
  |---|---|---|---|
  | Sensors | 512 words | 189 (36 %) | 323 |
  | Console | 512 words | 251 (49 %) | 261 |
  | Uplink  | 1024 words | 227 (22 %) | 797 |

  Compared with the pre-Wi-Fi baseline (`2026-11-12_stack_baseline.md`): Sensors unchanged, Console
  +16 words (the Uplink status lines), Uplink new. The Uplink's HTTP buffers (256 + 2048 bytes) are
  members of the process object, not on its stack. Not measured: the Wi-Fi driver's own thread
  (1 KB static stack, Arm's default).

The soak ran the first version of the Uplink, which re-initialised the driver after five failed joins
(section 3). That path did not run during the soak (no failed join); the final version differs only
there. A 12-minute run of the final version (`2026-11-12_uplink_final_12min_*`, 19:27:44 UTC):
12 uploads OK, 12 matching feed entries (235610..235621), heap minimum 640, stack peaks Sensors 189,
Console 251, Uplink 225 words.

### TOTAL_HEAP_SIZE

Recorded before deciding: minimum ever free after the soak 640 of 1024 bytes (384 used, by the
Wi-Fi driver). **Kept at 1024** (2.7 times the need). The failure paths of the final version allocate
nothing more (section 3: 7 minutes of failed joins, minimum ever free still 640).

## 2. Wrong password (`APP_TEST_WRONG_PASSWORD 1`), final version

`2026-11-12_uplink_wrong_password_console.txt` (7 minutes, 19:20:17 UTC):

```
[   16.951 s] Uplink: join FAILED (code -3, after 16025 ms), retry in 5 s
[   23.261 s] Uplink: join FAILED (code -1, after 1301 ms), retry in 10 s
[   49.296 s] Uplink: join FAILED (code -3, after 16025 ms), retry in 20 s
[   69.895 s] Uplink: join FAILED (code -1, after 589 ms), retry in 40 s
[  125.937 s] Uplink: join FAILED (code -3, after 16032 ms), retry in 60 s
[  186.536 s] Uplink: join FAILED (code -1, after 589 ms), retry in 60 s
...
[  399.205 s] Uplink: join FAILED (code -3, after 16025 ms), retry in 60 s
```

- The device keeps running: 85 samples in 420 s, none overwritten; the Console prints every failure;
  the Uplink keeps retrying, with back-off 5, 10, 20, 40, then 60 s.
- Failed joins alternate between `ARM_DRIVER_ERROR_TIMEOUT` (-3) after 16 s and `ARM_DRIVER_ERROR`
  (-1) after 0.6..1.3 s (the module's answer to the join command).
- Heap minimum ever free: 640 bytes throughout (SWD after the run: 0x280).

## 3. Finding: CMSIS-Driver ISM43362 2.11.0 and re-initialisation

The first version of the Uplink called `Uninitialize()` after five failed joins, so that the next
`Initialize()` would reset a stuck module. With the wrong password
(`2026-11-12_uplink_wrong_password_v1_console.txt`, 19:11:54 UTC):

```
[  185.964 s] Uplink: too many failed joins: resetting the Wi-Fi module
[  185.985 s] Uplink: Wi-Fi module initialisation FAILED, retry in 60 s
[  246.013 s] Uplink: Wi-Fi module ready
#   xMinimumEverFreeBytesRemaining = 0x00000108
```

The FreeRTOS heap's minimum ever free fell from 640 to **264 bytes**. In the driver source
(`Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c`, unmodified):

1. `WiFi_Initialize()` cleans up after a failure by calling `WiFi_Uninitialize()`, which returns at
   once because `driver_initialized` is still 0: the two mutexes and five event flag objects it just
   created (384 bytes) are never deleted. Each failed `Initialize()` leaks 384 bytes; with a 1024-byte
   heap the third attempt cannot create its objects any more.
2. `module_initialized` is set once and never cleared, so `Initialize()` after `Uninitialize()` skips
   the hardware reset of the module: re-initialising the driver does not reset the module.

Consequence in the final version: the driver is initialised once and never uninitialised; failed joins
are retried for ever (section 2). Remaining exposure: if `Initialize()` fails at start-up, every retry
leaks 384 bytes (reported as `Wi-Fi module initialisation FAILED`). Not seen in any run so far.
Possible fixes, not applied (the driver is kept unmodified): a two-line change in the driver (set
`driver_initialized = 1U` before the cleanup call; clear `module_initialized` in `WiFi_Uninitialize()`),
or a module reset through `WiFi_ISM43362_Pin_RSTN()` from the project's own code.

*Update, same day:* decided to keep the driver unmodified and not to retry a failed `Initialize()` in
the same run: the Uplink now reports it, waits 5 s and restarts the board (`NVIC_SystemReset()`), and a
counter in a `.noinit` RAM section makes repeated restarts visible at every boot. Test with the module
held in reset: `2026-11-12_wifi_init_fail_restart_console.txt` (15 restarts in 90 s, FreeRTOS heap the
same at every failure: 640 of 1024 bytes free). Draft upstream issue:
`docs/upstream/cmsis-driver-ism43362-init-leak.md`.

## 4. Not yet done

*Update 2026-11-14:* done, see `2026-11-14_ap_off.md` (two outages, 2 and 13 minutes; recovered both
times; 6 uploads = 6 feed entries). The plan as written on 2026-11-12:

- **Access point off for about 2 minutes, then on again:** needs the access point switched by hand;
  not possible at the time of this test. To do: run the normal build with
  `tools/flash_and_capture.sh ... 720 ...`, switch the AP off after about 2 minutes for about 2
  minutes, then compare with the feed. Expected from the code: `upload FAILED` (DNS or connect),
  retries every 15 s, `Rejoining` after three failures, then `join FAILED` with back-off until the AP
  is back, then `joined` and uploads again.
