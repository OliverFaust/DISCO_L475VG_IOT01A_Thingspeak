# Phase 3, step 2: one ThingSpeak update (2026-11-12)

Goal: outside the process network, join the Wi-Fi network, resolve api.thingspeak.com, send one
update with three fields, print the entry id; check the entry on ThingSpeak from Ubuntu.

## Set-up

- Diagnostic mode `APP_DIAG_WIFI_ONE_UPLOAD` (`Core/Inc/app_config.h`): one process,
  `Core/Src/wifi_upload_test.cpp`, the only user of I2C2 and the Wi-Fi module in this mode.
- Request and response: `Core/Src/thingspeak_http.cpp` (`tsBuildUpdate`, `tsParseResponse`,
  `tsMaskKey`), host-tested (10 cases: request text with a negative value `-0.50`, buffer too small,
  key masking, 200 with entry id, 400 with `0`, partial headers and body, chunked body, garbage) and
  against a real ThingSpeak 400 response (903 bytes, captured with curl and an invalid key).
- Secrets: Wi-Fi and write key from `Core/Inc/app_secrets.h` (git-ignored); channel ID and read key
  for the check from `tools/thingspeak_host.env` (git-ignored, host only).
- Never printed or recorded: the password; the write key except its first 4 characters (firmware
  masks it, `tools/mask_log.py` checks); the SSID except its first 3 characters; the channel ID except
  its first 2 digits; the read key. `tools/check_secrets.sh` before every commit.
- Plain HTTP on port 80: the ISM43362 setup used here has no TLS, so the write key crosses the network
  unencrypted (as in the Mbed version).
- Device side: `tools/flash_and_capture.sh`, then `tools/mask_log.py`. ThingSpeak side:
  `tools/thingspeak_feed.sh N` (HTTPS; prints entry id, time and fields 1..3 only).

## Four attempts

| # | UTC | Channel state | Device: HTTP status, entry id | Feed read right after |
|---|---|---|---|---|
| 1 | 18:18 | old write key in `app_secrets.h` | **400 Bad Request, 0** | no new entry |
| 2 | 18:21 | key corrected; only field1 enabled | 200 OK, **235571** | 235571: field1 = 17.07, field2/3 = None |
| 3 | 18:27 | only field1 enabled (change not saved yet) | 200 OK, **235572** | 235572: field1 = 17.14, field2/3 = None |
| 4 | 18:30 | fields 1..4 enabled (settings saved 18:29:17) | 200 OK, **235573** | 235573: **17.15, 74.21, 979.19**; 235571 and 235572 now also show fields 2 and 3 |

Files: `2026-11-12_one_upload_attempt<N>_console.txt` (device, masked) and
`2026-11-12_one_upload_attempt<N>_feed.txt` (ThingSpeak) for N = 2, 3, 4; attempt 1 has the device
log only.

### Attempt 4: all three fields

Device (`2026-11-12_one_upload_attempt4_console.txt`, capture started 18:29:58 UTC):

```
[    0.014 s] sample: T = 17.15 C  RH = 74.21 %  p = 979.19 hPa
[    0.833 s] Wi-Fi Initialize/PowerControl: OK
[    5.636 s] join "Vod***********" (WPA2): OK (4800 ms)
[    5.754 s] resolve api.thingspeak.com: OK -> 44.214.53.171 (109 ms)
[    5.856 s] connect 44.214.53.171:80: OK
    GET /update?api_key=CJJZ************&field1=17.15&field2=74.21&field3=979.19 HTTP/1.1
    Host: api.thingspeak.com
    Connection: close
[    6.011 s] status line: HTTP/1.1 200 OK
[    6.015 s] ThingSpeak entry id: 235573
[    6.030 s] FreeRTOS heap: free 640 of 1024 bytes, minimum ever free 640 bytes
```

ThingSpeak (`2026-11-12_one_upload_attempt4_feed.txt`):

```
# channel 87****, last 4 entries, read 2026-11-12 18:30:28 UTC
last_entry_id: 235573
entry_id  created_at            field1     field2     field3
235570    2022-01-08T19:46:06Z  1          88         None
235571    2026-11-12T18:21:16Z  17.07      74.08      978.90
235572    2026-11-12T18:27:31Z  17.14      74.17      979.14
235573    2026-11-12T18:30:04Z  17.15      74.21      979.19
```

- **Same entry id on both sides** (235573), created 18:30:04 UTC = capture start 18:29:58 + about
  6 s to the request. Fields equal to the values the device printed.
- Heap: unchanged, 384 bytes used by the driver after a socket was opened and closed;
  `__sbrk_heap_end = 0` in all four runs (application allocates nothing).

## What the failed and partial attempts show (for docs/07)

1. **Wrong or outdated write key** (attempt 1): ThingSpeak answers `400 Bad Request` with body `0`,
   exactly as for a made-up key. Everything before it (join, DNS, TCP, send) succeeds, so the device
   log shows the problem only in the status line and entry id 0. Check the key on the channel's
   *API Keys* tab; a regenerated key invalidates the old one.
2. **Fields not enabled** (attempts 2 and 3): the update is accepted (200, new entry id) but the read
   API (`feeds.json`) returns only the fields enabled in *Channel Settings*. ThingSpeak had stored the
   values nonetheless: after fields 2 and 3 were enabled and the settings **saved**, the earlier
   entries 235571 and 235572 showed them too. Ticking the boxes without *Save Channel* changes
   nothing (attempt 3; the channel's `updated_at` stayed at 2022-01-08 until the save at 18:29:17).

## Legacy bugs fixed, exercised here

The whole request is sent even if `SocketSend` takes less (loop with offset); the receive buffer is
bounded and NUL-terminated before the status line is searched (no `strstr` on unterminated data);
numbers are formatted by `format_fixed()` (negative values tested on the host). A missing
`Core/Inc/app_secrets.h` stops the build with
`#error "Core/Inc/app_secrets.h is missing. In the project folder run:  cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h  ..."`
(checked by building without the file).
