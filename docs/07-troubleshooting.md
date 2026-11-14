# 7. Troubleshooting

Symptoms, what they mean, and what to do. Each entry quotes the console output you see (ST-LINK
virtual COM port, 115200 8N1). This chapter grows with the project.

---

## 7.1 The board restarts every few seconds: "Wi-Fi module initialisation FAILED"

**You see**, repeating about every 6 s:

```
DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every 60 s)
Reset cause: software reset; restarts after a failed Wi-Fi start since power-on: 7
[    0.922 s] Uplink: Wi-Fi module initialisation FAILED (FreeRTOS heap free 640, minimum ever 640 bytes): restarting the board in 5 s
```

**What it means.** The Wi-Fi driver could not talk to the ISM43362 module at start-up. The Uplink
then restarts the whole board on purpose instead of retrying (why: `docs/05-code-walkthrough.md`,
"When the Wi-Fi module does not start"). The counter shows how many restarts happened since the board
was last powered on; it starts at 0 after a power-on. One restart that is followed by `Wi-Fi module
ready` and `joined` was a hiccup. A counter that keeps rising means the module never answers.

**What to do:**

1. Power-cycle the board: unplug the USB cable for a few seconds. (A press of the reset button resets
   the microcontroller and, through the driver, the module, but not their power supply.)
2. Check that the firmware was built with `APP_TEST_WIFI_INIT_FAIL 0` in `Core/Inc/app_config.h`.
   With 1 the module is held in reset deliberately (a test switch), and you see exactly this loop.
3. Run the Wi-Fi bring-up test (`APP_DIAGNOSTIC_MODE APP_DIAG_WIFI_BRINGUP` in `app_config.h`,
   rebuild, flash). If its `Initialize` also fails, the module does not respond on SPI3: check that
   the board is a B-L475E-IOT01A (the module is on the board, not on a shield) and that the pin
   configuration in the `.ioc` was not changed (PE0, PE1, PE8, PB12, PB13, PC10-PC12;
   `docs/08-how-it-was-made.md`, 8.4.5-8.4.6).
4. If the bring-up test reports a module firmware other than
   `ISM43362-M3G-L44-SPI,C3.5.2.7.STM,...`, note it: only the STM firmware variant is supported by
   Arm's driver (any version); non-STM firmware must be 6.2.1.7.

**What does not help:** waiting. The board restarts until the module answers.

---

## 7.2 "ThingSpeak rejected the update (entry id 0)" / "REJECTED by ThingSpeak (HTTP 400, entry 0)"

**You see** joining, DNS and connecting succeed, then `HTTP/1.1 400 Bad Request` and entry id 0.

**What it means.** ThingSpeak did not accept the write key: it is wrong, belongs to another channel, or
was regenerated (a new key invalidates the old one). ThingSpeak answers a made-up key in exactly the
same way. (Measured: `docs/results/2026-11-12_one_upload.md`, attempt 1.)

**What to do.** Copy the *Write API Key* from the channel's *API Keys* tab into
`APP_THINGSPEAK_WRITE_KEY` in `Core/Inc/app_secrets.h`, rebuild, flash.

Entry id 0 can also mean that the update came less than 15 s after the previous one of the same
channel (ThingSpeak's rate limit; the HTTP status in that case was not measured for this project).
With the default period of 60 s this happens only if two boards write to one channel.

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
[  367.153 s] Uplink: too many failed uploads: leaving the network, joining again
[  383.188 s] Uplink: join FAILED (code -3, after 16025 ms), retry in 5 s
   ... (join FAILED, retry every 60 s)
[ 1170.548 s] Uplink: joined (4680 ms), IP 10.88.160.48
[ 1170.950 s] Uplink: sample #234 -> entry 235681 (394 ms)
```

**What it means.** Nothing is wrong with the board: it lost the access point and reconnected by itself
when it came back -- at most one retry interval (60 s) later. Samples taken meanwhile are not sent
later (the Uplink always sends the newest one), so the channel has a gap, but no duplicate or wrong
entries. Sampling and the console carry on throughout. Only if the failures never stop, see 7.4.
