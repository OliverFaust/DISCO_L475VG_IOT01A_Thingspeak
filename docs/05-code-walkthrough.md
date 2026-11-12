# 5. Code walkthrough

This chapter explains the source, process by process. It grows with the project; this version
covers the HTS221 conversion in the Sensors process (5.x) and the Uplink process (5.y).

---

## 5.x Temperature and humidity: the HTS221 conversion

Files: `Core/Inc/hts221_calib.h`, `Core/Src/hts221_calib.c`, used by `Core/Src/sensors_process.cpp`.

### What the sensor delivers

The HTS221 does not output degrees or percent. It outputs two raw 16-bit numbers, `T_OUT` and
`H_OUT`, and stores two **calibration points** per quantity, measured at the factory, in its
registers 0x30..0x3F. A reading is converted by linear interpolation between the two points (HTS221
datasheet, "Temperature/humidity conversion"):

```
T  = T0 + (T_OUT - T0_OUT)    * (T1 - T0) / (T1_OUT - T0_OUT)
RH = H0 + (H_OUT - H0_T0_OUT) * (H1 - H0) / (H1_T0_OUT - H0_T0_OUT)
```

The calibration temperatures are stored in eighths of a degree (`T0_degC_x8`, 10 bits), the
calibration humidities in halves of a percent (`H0_rH_x2`, 8 bits). So T0 = `T0_degC_x8` / 8 and
H0 = `H0_rH_x2` / 2, with fractions.

### What ST's driver does

ST's driver (`Drivers/BSP/Components/hts221/hts221.c`, used through `BSP_TSENSOR_ReadTemp()` and
`BSP_HSENSOR_ReadHumidity()`) applies the same interpolation, but first **truncates** the calibration
points to whole units:

```c
T0_degC = T0_degC_x8_u16 >> 3;   /* 163 >> 3 = 20, but 163 / 8 = 20.375 */
H0_rh   = buffer[0] >> 1;        /* 139 >> 1 = 69, but 139 / 2 = 69.5   */
```

Shifting right by 3 divides by 8 and throws the remainder away. The interpolation line then goes
through the wrong points, and every reading is off by an amount that depends on the board's
calibration and on the reading itself: up to 0.875 °C in a calibration temperature, up to 0.5 %RH
in a calibration humidity.

### On the lab board

Calibration registers of the board used for this project (`docs/results/2026-11-12_sensor_check.md`):

| | Stored | Datasheet | ST's driver |
|---|---|---|---|
| T0 | `T0_degC_x8` = 163 | 20.375 °C | 20 °C |
| T1 | `T1_degC_x8` = 272 | 34.000 °C | 34 °C |
| H0 | `H0_rH_x2` = 62 | 31.0 %RH | 31 %RH |
| H1 | `H1_rH_x2` = 139 | 69.5 %RH | 69 %RH |

For the raw sample H_OUT = -16528, T_OUT = -291 (T0_OUT = -2, T1_OUT = 768, H0_T0_OUT = 6,
H1_T0_OUT = -12603):

```
datasheet:  T  = 20.375 + (-291 + 2) * (34.000 - 20.375) / 770   = 15.261 °C
ST driver:  T  = 20     + (-291 + 2) * (34     - 20    ) / 770   = 14.745 °C    (-0.52 °C)
datasheet:  RH = 31.0 + (-16528 - 6) * (69.5 - 31.0) / -12609    = 81.484 %
ST driver:  RH = 31   + (-16528 - 6) * (69   - 31  ) / -12609    = 80.829 %     (-0.66 %RH)
```

Because T1 happens to be a whole number on this board, the temperature error is zero at 34 °C and
grows towards lower temperatures (-0.375 °C at 20.375 °C, -0.52 °C at 15 °C). Another board has other
calibration values and a different error.

### What this project does

`hts221_calib.c` keeps ST's driver for what it does correctly (identifying and configuring the
sensor in `BSP_TSENSOR_Init()`/`BSP_HSENSOR_Init()`), and replaces only the conversion:

1. `hts221_calib_read()` reads the 16 calibration registers in one I2C transfer and
   `hts221_calib_decode()` turns them into a `Hts221Calib` struct, as fractions:
   `c->t0_degc = (float)t0x8 / 8.0f;`. The Sensors process calls it once, in `initSensors()`; the
   calibration never changes, so there is no need to read it again for every sample (ST's driver
   does, which costs six of its eight I2C transfers per reading).
2. For each sample, `hts221_read_raw()` reads `H_OUT` and `T_OUT` in one 4-byte transfer, and
   `hts221_calib_temperature()` / `hts221_calib_humidity()` apply the datasheet formula.

The module keeps no state of its own: the `Hts221Calib` lives in the Sensors process object, the
only thread that uses I2C2. The decoding and conversion are pure functions, so they can be compiled
and tested on a PC (`docs/results/2026-11-12_hts221_fix.md`, section 1).

At start-up the Sensors process also sends a one-off check (`readHts221Check()`), which the Console
prints: the calibration bytes, one raw sample, and the result of both formulas side by side. On the
lab board:

```
  T : datasheet formula (samples) 15.562 C, ST driver formula 15.055 C, BSP_TSENSOR_ReadTemp() 15.055 C
  RH: datasheet formula (samples) 80.752 %, ST driver formula 80.106 %, BSP_HSENSOR_ReadHumidity() 80.106 %
```

**Try it:** run the project on your own board and compare its calibration bytes and its two
temperature values with the ones above. How large is the truncation error on your board, and at
which temperature is it zero (if anywhere in the sensor's range)?

---

## 5.y The Uplink process: Wi-Fi and ThingSpeak

Files: `Core/Inc/uplink_process.hpp`, `Core/Src/uplink_process.cpp`, `Core/Inc/uplink_status.hpp`,
`Core/Src/thingspeak_http.cpp`; wiring in `Core/Src/app.cpp`; messages printed by
`ConsoleProcess::printUplink()`.

### Its place in the network

```
Sensors --uplinkSamples (Sample, 1, KeepNewest)--> Uplink --status (UplinkStatus, 8, KeepNewest)--> Console
```

The Uplink is the **only** process that calls the Wi-Fi driver (`Driver_WiFi0`), just as the Sensors
process is the only one that uses I2C2. It prints nothing: every event becomes an `UplinkStatus`
message for the Console. Both channels are buffered with `KeepNewest`, so neither the Sensors process
nor the Uplink ever waits for anyone: if the Uplink is busy joining a network for ten seconds, the
Sensors process keeps overwriting the one buffered sample, and when the Uplink looks again it gets the
newest one. A sample is a measurement *now*; an old one is worth less than a new one, so dropping
the older one is the right policy here (contrast: a command channel, where every message counts,
would use `Block`).

The Sensors process writes each sample twice, once per channel (`out_ << s; uplink_ << s;`): an
element of a buffered channel is taken by exactly one reader, so two readers need two channels.

### Priority

`UplinkProcess::taskPriority()` returns `osPriorityLow`, below the network priority
(`osPriorityBelowNormal`) that `Run()` gives the Sensors and the Console. The Wi-Fi driver waits for
the module by polling: it checks the data-ready pin every millisecond (`osDelay(1)`) and busy-waits
for the short SPI transfers. A low priority means that this waiting can never delay a sample or a
console line. (The driver's own helper thread runs at `osPriorityAboveNormal`, as Arm's
configuration requires, but it only wakes briefly every 8 ms while a socket is open.)

### The main loop

`UplinkProcess::run()` is a small state machine with three questions, asked in order on every pass:

1. **Is the module initialised?** If not: `Initialize()` and `PowerControl(FULL)`; on failure report
   `ModuleFailed` and wait 60 s.
2. **Is it joined?** If not: `Activate()` with the SSID and password from `app_secrets.h` (WPA2). On
   failure report `JoinFailed` and wait with **back-off**: 5, 10, 20, 40, then every 60 s, for as long
   as it takes. The driver is never uninitialised: in CMSIS-Driver 2.11.0 a second `Initialize()`
   does not reset the module, and a failed `Initialize()` leaks its RTOS objects on the FreeRTOS heap
   (measured; `docs/results/2026-11-12_uplink_recovery.md`). An early version of the Uplink reset the
   driver after five failed joins and lost 384 bytes of heap doing so.
3. **Is it time to upload?** Wait until `nextUploadAt`, then send the newest sample.

The process never returns. No network error leads out of the loop; each one leads to a report and a
wait.

### Waiting without missing samples

All waiting goes through `waitKeepingLatest(ms)`:

```cpp
Sample s{};
RelTimeoutGuard timeout(Milliseconds(left));
Alternative alt(samples_ | s, timeout);
if (alt.priSelect() == 0) { latest_ = s; haveSample_ = true; } else { return; }
```

An ALT over the sample channel and a timeout: whichever comes first. A new sample replaces
`latest_`; the timeout ends the wait. `left` is recomputed from an absolute deadline on every pass, so
samples arriving during the wait do not extend it.

### The upload period, and ThingSpeak's 15 s rule

```cpp
nextUploadAt = now() + periodMs_;     // after the attempt has FINISHED
```

The period (`APP_UPLOAD_PERIOD_S`, default 60 s) is counted from the **end** of an upload attempt,
i.e. it is a relative timeout (exercise 9.1). ThingSpeak rejects updates of a channel that arrive less
than 15 s after the previous one. Measured from the end of the previous attempt, the next request
can never arrive earlier than one period after the previous one did, however slow that one was. An
absolute schedule (every 60 s on the dot) would send the next request sooner after a slow one. The
price is a slow drift of the upload times: in the 31-minute soak test 0.34 s per upload on average,
the duration of a request (`docs/results/2026-11-12_uplink_recovery.md`). `APP_UPLOAD_PERIOD_S` below 15 does not compile (`static_assert`), and the constructor clamps
any smaller run-time value to 15 s.

After a *failed* attempt the next try comes 15 s later, never sooner: a request may have reached
ThingSpeak even if the answer got lost.

### One upload

`UplinkProcess::upload()`:

1. `SocketGetHostByName("api.thingspeak.com")` -- on failure: stage `DNS`.
2. `SocketCreate()` (TCP), receive timeout 10 s, `SocketConnect()` to port 80 -- stages `socket`,
   `connect`.
3. `tsBuildUpdate()` writes `GET /update?api_key=...&field1=..&field2=..&field3=.. HTTP/1.1`,
   `Host:`, `Connection: close`. The values come from `format_fixed()`: correct for negative
   temperatures.
4. **Send everything.** `SocketSend()` may take fewer bytes than offered; the loop advances by what
   it took until all is sent (`sent += k`). The Mbed version passed `sbuffer + response`, where
   `response` was only the size of the *last* chunk, so after two partial sends it resent the wrong
   part of the request.
5. **Receive safely.** `SocketRecv()` into `response_` until `tsParseResponse()` says the body is
   complete, the server closes, or the buffer is full -- one byte is always kept for the terminating
   `'\0'`. The Mbed version received into a 64-byte buffer without a terminator and then called
   `strstr()` on it.
6. **Check the answer.** HTTP 200 and an entry id > 0: `Uploaded`. Anything else from the server
   (for example entry id 0: wrong key, or too soon): `Rejected`. No complete answer: `UploadFailed`.
7. `SocketClose()` in every case.

After three failed uploads in a row, or as soon as the driver says the station is no longer
connected (`IsConnected()`), the Uplink leaves the network and joins again (`Rejoining`).

### What the Console shows

```
[    5.713 s] Uplink: joined (4786 ms), IP 192.168.1.28  [ok 0, rejected 0, failed 0, joins 1]
[    7.061 s] Uplink: sample #2 -> entry 235574 (340 ms)  [ok 1, rejected 0, failed 0, joins 1]
[   67.460 s] Uplink: sample #14 -> entry 235575 (399 ms)  [ok 2, rejected 0, failed 0, joins 1]
```

Each line carries the running counters, so the last line of a run tells how many uploads succeeded,
were rejected or failed. `tools/compare_uploads.py` checks a captured run against the channel feed
(`tools/thingspeak_feed.sh`).
