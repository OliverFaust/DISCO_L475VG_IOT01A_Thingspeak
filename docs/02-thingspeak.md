# 2. The ThingSpeak channel

The board sends its measurements to a channel on ThingSpeak (<https://thingspeak.mathworks.com>), a
free IoT data service of MathWorks. This chapter creates the channel and collects the values the
build needs. Done once per student (or per board).

**Tested:** November 2026, free ThingSpeak account, in Chrome on Ubuntu. ThingSpeak's web pages
change from time to time; if a button has another name, look for the same function.

---

## 2.1 Account and channel

1. Sign in at <https://thingspeak.mathworks.com> with a MathWorks account (create one if needed).
2. **Channels > My Channels > New Channel**.
3. Fill in:

   | Field | Value |
   |---|---|
   | Name | e.g. `B-L475E-IOT01A <your name>` |
   | Field 1 | `Temperature` (tick the box) |
   | Field 2 | `Humidity` (**tick the box**) |
   | Field 3 | `Pressure` (**tick the box**) |

   The program sends temperature in °C as `field1`, relative humidity in % as `field2` and pressure
   in hPa as `field3`. **All three fields must be ticked (enabled).** ThingSpeak stores values of
   fields that are not enabled, but does not show or return them (chapter 7, 7.3).
4. Scroll down and click **Save Channel**.

You should now see the channel's page with the tabs *Private View*, *Public View*, *Channel
Settings*, *Sharing*, *API Keys*, ..., and three empty charts: *Field 1 Chart*, *Field 2 Chart*,
*Field 3 Chart*. If you see only one chart, go to *Channel Settings*, tick *Field 2* and *Field 3*,
and click **Save Channel** again.

---

## 2.2 The keys and the channel ID

Open the tab **API Keys**.

| Value | Where | Goes into | Used by |
|---|---|---|---|
| **Write API Key** (16 characters, upper case letters and digits) | *API Keys > Write API Key* | `APP_THINGSPEAK_WRITE_KEY` in `Core/Inc/app_secrets.h` (chapter 3) | the board, to write |
| **Channel ID** (a number) | top of the channel page, *Channel ID: ...* | `THINGSPEAK_CHANNEL_ID` in `tools/thingspeak_host.env` | the PC, to check uploads (optional) |
| **Read API Key** (16 characters) | *API Keys > Read API Keys* | `THINGSPEAK_READ_KEY` in `tools/thingspeak_host.env` (empty for a public channel) | the PC, to check uploads (optional) |

Treat the keys like passwords: anyone with the write key can write into your channel. Both files are
listed in `.gitignore` -- never commit them, never paste a key into a report or forum post. If a key
was exposed, click **Generate New Write API Key**: the old key stops working at once (and the board
needs the new one).

You should now have written down: the write key (and, if you want to check uploads from the PC, the
channel ID and the read key).

---

## 2.3 How the board writes, and the rate limit

Every upload is one HTTP request (plain HTTP, port 80; the ISM43362's TLS support is not used):

```
GET /update?api_key=<write key>&field1=21.53&field2=45.10&field3=1009.87 HTTP/1.1
Host: api.thingspeak.com
Connection: close
```

ThingSpeak answers with the number of the new entry (`entry_id`, counting from 1 in each channel), or
with `0` if it did not store the update. The program checks this number: the console line
`Uplink: sample #14 -> entry 235694 (328 ms)` means the update was stored as entry 235694.

**Rate limit.** A free ThingSpeak account accepts at most one update per channel every **15 s**; an
update that comes sooner is rejected (entry 0). The program uploads every **60 s** by default
(`APP_UPLOAD_PERIOD_S` in `Core/Inc/app_config.h`); values below 15 do not compile. The period is
measured from the end of one upload to the start of the next, so two uploads are never less than one
period apart, even after a reconnect. Two boards writing into one channel break the limit -- give each
board its own channel.

Free accounts also have a yearly message quota; at one update per minute a board uses about 525 600
messages a year. Check your account's current limits under *Account > My Account*.

**What a rejection looks like:** a wrong or regenerated write key is answered with `HTTP/1.1 400 Bad
Request` and entry 0; the console shows `REJECTED by ThingSpeak (HTTP 400, entry 0)` (chapter 7, 7.2).

---

## 2.4 Checking uploads from the PC (optional)

```bash
cp tools/thingspeak_host.env.template tools/thingspeak_host.env
```

Edit `tools/thingspeak_host.env` and fill in `THINGSPEAK_CHANNEL_ID=` and `THINGSPEAK_READ_KEY=`
(no quotes, no spaces). After the board has uploaded (chapter 3):

```bash
tools/thingspeak_feed.sh 3
```

You should now see the channel's last entry id and the last three entries, each with
`entry_id`, `created_at` and `field1`..`field3`, the latest matching the `-> entry ...` lines on the
board's console. The channel ID is printed with its first two digits only, the read key never.

Next: chapter 3 (build and run).
