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
