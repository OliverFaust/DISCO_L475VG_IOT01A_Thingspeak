# 7. Troubleshooting

Symptoms, what they mean, and what to do. Each entry quotes the console output you see (ST-LINK
virtual COM port, 115200 8N1). This chapter grows with the project.

---

## 7.1 "Wi-Fi module initialisation FAILED" and restarts; then "Wi-Fi unavailable after 3 attempts"

**You see** up to three restarts, about 6 s apart, each one counted:

```
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 2 of 3
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED (FreeRTOS heap free 640, minimum ever 640 bytes): restarting the board in 5 s
```

and then, if the module still does not answer, a board that samples and prints but does not upload:

```
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console (Uplink NOT started: Wi-Fi unavailable after 3 failed starts)
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 3 of 3
[    0.114 s] Wi-Fi unavailable after 3 attempts - see docs/07 section 7.1 - press RESET to retry
```

repeated once a minute, between the normal sample lines.

**What it means.** The Wi-Fi driver could not talk to the ISM43362 module at start-up. The Uplink then
restarts the whole board on purpose instead of retrying in the same run. This works around two
defects of Arm's driver, reported as [ARM-software/CMSIS-Driver#60](https://github.com/ARM-software/CMSIS-Driver/issues/60)
(details: `docs/05-code-walkthrough.md`, "When the Wi-Fi module does not start"). The restart can go
once Arm fixes the driver and the project uses the fixed version. After three such restarts in a
row it gives up until you intervene, so that a broken module does not keep the board restarting for
ever. One restart that is followed by `Wi-Fi module ready` and `joined` was a hiccup; the counter
then goes back to 0.

**What to do:**

1. Press RESET (black button) or power-cycle the board (unplug the USB cable for a few seconds).
   Either sets the counter to 0, and the board tries up to three more times. A power-cycle also cuts
   the module's supply, which a reset does not.
2. Check that the firmware was built with `APP_TEST_WIFI_INIT_FAIL 0` in `Core/Inc/app_config.h`.
   With 1 the module is held in reset deliberately (a test switch), and you see exactly this sequence.
3. Run the Wi-Fi bring-up test (`APP_DIAGNOSTIC_MODE APP_DIAG_WIFI_BRINGUP` in `app_config.h`,
   rebuild, flash). If its `Initialize` also fails, the module does not respond on SPI3: check that
   the board is a B-L475E-IOT01A (the module is on the board, not on a shield) and that the pin
   configuration in the `.ioc` was not changed (PE0, PE1, PE8, PB12, PB13, PC10-PC12;
   `docs/08-how-it-was-made.md`, 8.4.5-8.4.6).
4. If the bring-up test reports a module firmware other than
   `ISM43362-M3G-L44-SPI,C3.5.2.7.STM,...`, note it: only the STM firmware variant is supported by
   Arm's driver (any version); non-STM firmware must be 6.2.1.7.

**What does not help:** waiting. After three failed starts the board does not try again by itself.

---

## 7.2 "REJECTED by ThingSpeak": HTTP 400 or HTTP 200 with entry 0

The console line `sample #N REJECTED by ThingSpeak (HTTP <status>, entry 0)` means: the request got
through, ThingSpeak answered, and it did not store the update. The HTTP status tells you why.

### HTTP 400, entry 0: the write key is wrong

**You see** joining, DNS and connecting succeed, then `REJECTED by ThingSpeak (HTTP 400, entry 0)`
(status line `HTTP/1.1 400 Bad Request`, body `0`), on every upload.

**What it means.** ThingSpeak did not accept the write key: it is wrong, belongs to another channel, or
was regenerated (a new key invalidates the old one). ThingSpeak answers a made-up key in exactly the
same way. (Measured: `docs/results/2026-11-12_one_upload.md`, attempt 1.)

**What to do.** Copy the *Write API Key* from the channel's *API Keys* tab into
`APP_THINGSPEAK_WRITE_KEY` in `Core/Inc/app_secrets.h`, rebuild, flash.

### HTTP 200, entry 0: the key is right, but ThingSpeak did not store this update

**You see** `REJECTED by ThingSpeak (HTTP 200, entry 0)` -- status `200 OK`, body `0`. The key was
accepted (a wrong key gives 400); the update itself was refused.

**What it means**, most likely first:

1. **Too soon after the previous update of the channel** (rate limit: 15 s on a free account,
   chapter 2, 2.3). The program itself never sends sooner: the period counts from the end of an
   upload, and the first upload after a start comes no earlier than 15 s after it, so that a reset
   right after an upload is safe. Measured: an old version, which uploaded about 7 s after start, was
   reset a few seconds after an upload and got `HTTP 200, entry 0` (`docs/results/2026-11-14_trace_trigger/break_counter/console_rejected_run.txt`);
   the current one, reset 3 and 4 s after an upload, was accepted
   (`docs/results/2026-11-14_first_upload_after_boot.md`).
2. **Another device writes to the same channel** -- a second board, a classmate with your key, a
   MATLAB script, an MQTT client. Each one keeps the rate limit by itself, together they do not.
   Check the channel's feed (`tools/thingspeak_feed.sh 10`): entries between the board's
   `-> entry` numbers, or entry ids that jump, come from someone else. Give each device its own
   channel, or generate a new write key (only the board gets it).
3. **No field of the update is enabled in the channel** (not measured). An update whose enabled fields
   are all missing has nothing to store. Note that *some* disabled fields do **not** cause a rejection:
   with only field 1 enabled, ThingSpeak accepted the update (200, a new entry id) and kept fields 2
   and 3 hidden (7.3, measured).

**What to do.** An occasional `HTTP 200, entry 0` costs one sample: the next upload, a period later,
goes through. If it happens regularly, look for a second writer (2.) and check *Channel Settings*
(3.).

---

## 7.3 Uploads succeed, but the channel shows only field 1

**You see** `sample #12 -> entry 235572` on the console, but on ThingSpeak (or with
`tools/thingspeak_feed.sh`) field 2 and field 3 are empty (`None`).

**What it means.** The fields are not enabled in the channel. ThingSpeak stores the values anyway, but
shows and returns only enabled fields. (Measured: `docs/results/2026-11-12_one_upload.md`, attempts 2
and 3.)

**What to do.** *Channel Settings*: tick *Field 2* and *Field 3* (names e.g. Humidity, Pressure),
then **Save Channel** at the bottom of the page -- ticking alone changes nothing. After saving, the
earlier entries show their values too.

---

## 7.4 "join FAILED" again and again, and the network is not in the scan

**You see**, from the start, join failures alternating between two codes, with growing intervals:

```
[   16.956 s] Uplink: join FAILED (code -3, after 16024 ms), retry in 5 s
[   29.978 s] Uplink: join FAILED (code -1, after 8013 ms), retry in 10 s
[   56.019 s] Uplink: join FAILED (code -3, after 16031 ms), retry in 20 s
```

**What it means.** The module could not join the network in `app_secrets.h`. The console looks the
same for a wrong password and for a network the module cannot see at all, so first find out which:
run the bring-up test (`APP_DIAGNOSTIC_MODE APP_DIAG_WIFI_BRINGUP` in `Core/Inc/app_config.h`) and
look for your network in its scan list.

**Not in the list** -- the module cannot receive it. On the lab board this happened with
(`docs/results/2026-11-14_ap_off.md`):

- a **5 GHz** network: the ISM43362 works on 2.4 GHz only. Many phone hotspots default to 5 GHz, and
  some cannot be switched to 2.4 GHz at all;
- a 2.4 GHz network on **channel 12 or 13**: the module only used channels 1 to 11 (the US channels).
  These channels are legal in Europe, and phones and routers there may choose them.

What to do: set the access point to 2.4 GHz and a fixed channel between 1 and 11 (e.g. 1, 6 or 11),
WPA2-Personal, not hidden, and check the scan again. A laptop shows the band and channel of the
networks it sees (Ubuntu: `nmcli -f SSID,CHAN,FREQ,SECURITY dev wifi list`).

**In the list** -- check the SSID (exact, case-sensitive, spaces count) and the password in
`app_secrets.h`, and that the network uses WPA2-Personal (or WPA/WPA2). WPA3-only networks and
WPA2-Enterprise networks such as eduroam are not supported by the module.

---

## 7.5 Uploads stop, then "joined" again: the access point was gone for a while

**You see** (an access point switched off for 13 minutes; `docs/results/2026-11-14_ap_off.md`):

```
[  367.141 s] Uplink: sample #74 upload FAILED at DNS (code -16), retry in 15 s
[  367.153 s] Uplink: connection lost (driver reports not connected): leaving the network, joining again
[  383.188 s] Uplink: join FAILED (code -3, after 16025 ms), retry in 5 s
   ... (join FAILED, retry every 60 s)
[ 1170.548 s] Uplink: joined (4680 ms), IP 10.88.160.48
[ 1170.950 s] Uplink: sample #234 -> entry 235681 (394 ms)
```

(Recorded with firmware of 2026-11-14 before the second line said why the Uplink rejoins; that
recording reads "too many failed uploads" there. A rejoin after three failed uploads in a row, with
the station still connected, reads "3 failed uploads in a row: ...".)

**What it means.** Nothing is wrong with the board: it lost the access point and reconnected by itself
when it came back -- at most one retry interval (60 s) later. Samples taken meanwhile are not sent
later (the Uplink always sends the newest one), so the channel has a gap, but no duplicate or wrong
entries. Sampling and the console carry on throughout. Only if the failures never stop, see 7.4.

---

## 7.6 STM32CubeIDE: `No symbol "csp_trace_ring" in current context.`

**You see**, in the **Debugger Console** of an STM32CubeIDE debug session, when you type the dump
command of docs/06 (6.3), or `p &csp_trace_ring`:

```
No symbol "csp_trace_ring" in current context.
```

**What it means.** The session loaded an ELF without the trace ring: the Debug or Release build. Only
Debug-Trace has `csp_trace_ring`. `info files` in the same console shows which ELF it is; the first
line ends in `.../Debug/DISCO_L475VG_IOT01A_Thingspeak.elf` instead of `.../Debug-Trace/...`. This
happens when the session was started with **Run > Debug As**, or with the debug button. Both use the
launch configuration of the active build configuration, which is Debug after an import.

Such a session has also **flashed the Debug build** and reset the board. So the board no longer runs
Debug-Trace, and the ring you wanted is gone.

It also happens with **Debug-Trace (attach, dump)** when its **Main** tab names the Debug ELF. This
was seen with STM32CubeIDE 2.2.0. The attach launch downloads nothing, so the board still runs
Debug-Trace and the ring is still there: correct the Main tab (step 2 below) and attach again.

**What to do.**

1. Terminate the session.
2. Check the **Main** tab of both shared launch configurations (**Run > Debug Configurations...**,
   docs/06, 6.3, step 2, Figure 6.1). **C/C++ Application** must be
   `Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf`, and **Build Configuration** `Debug-Trace`. If
   it shows `Debug/...elf`, enter the project-relative path by hand (not **Browse...**), choose
   `Debug-Trace`, and click **Apply**. STM32CubeIDE 2.2.0 did this for the attach launch on a
   second machine.
3. Flash the trace build once with the shared launch configuration **Debug-Trace (flash and debug)**
   (**Run > Debug Configurations...**).
4. Wait for the event you want, then dump with **Debug-Trace (attach, dump)**. This launch downloads
   nothing and does not reset.

`info files` then names `.../Debug-Trace/...elf`, and `p &csp_trace_ring` gives
`(struct csp_trace_ring *) 0x10000000 <csp_trace_ring>`. Step by step: docs/06, 6.3, *Dump from
STM32CubeIDE by hand*. Afterwards, unplug the USB cable before you press B1 without a debugger
(docs/06, 6.6): the session left the debug-enable bit set.
