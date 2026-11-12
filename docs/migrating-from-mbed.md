# Migrating from the Mbed OS version

For lecturers who used the Mbed OS version of this project (branch `mbed-os-legacy`, tag
`mbed-os-final`, file `main.cpp`). Line numbers below refer to that file.

## Differences in the data

### Temperature and humidity in old ThingSpeak channels read low

The Mbed version read the HTS221 through ST's BSP (`BSP_TSENSOR_ReadTemp()`,
`BSP_HSENSOR_ReadHumidity()`). ST's driver truncates the sensor's factory calibration points to whole
degrees and whole percent before converting, and therefore reads too low. On the lab board used for
this port the error is **about -0.5 °C and -0.7 %RH** at room temperature (-0.52 °C and -0.66 %RH at
15 °C; the size depends on each board's calibration and on the reading). ThingSpeak data recorded
with the Mbed version contains this error.

How this was established: the truncation (`T0_degC = T0_degC_x8_u16 >> 3`, `H0_rh = buffer[0] >> 1`) is
in ST's current driver (STM32Cube FW_L4 V1.18.2) and in the Mbed library `BSP_B-L475E-IOT01` as
vendored by another Mbed project (github.com/edgeimpulse/example-dataforwarder-mbed,
`BSP_B-L475E-IOT01/Drivers/BSP/Components/hts221/hts221.c`, lines 135–136 and 214–215). The exact
revision the legacy project pins (`BSP_B-L475E-IOT01.lib`: `bfe8272ced90`) could not be checked:
os.mbed.com, which hosted it, now redirects to GitHub and no longer serves the code.

The current version converts with the datasheet formula (`Core/Src/hts221_calib.c`), so the same board
reads about 0.5 °C and 0.7 %RH higher than before. A step of that size in a channel that continues
from Mbed-era data is this correction, not a change in the room. Details: `docs/05-code-walkthrough.md`
(section on the HTS221 conversion) and `docs/results/2026-11-12_sensor_check.md`.

Pressure (LPS22HB) is converted as before.

## From one loop in `main()` to three processes

The Mbed version did everything in `main()`: connect, then a `while(true)` loop that opened a socket,
read the three sensors, sent one request, printed the answer and slept 60 s. The current version
splits this into three processes that share nothing but channels (`Core/Src/app.cpp`,
`docs/05-code-walkthrough.md`).

| Mbed OS version (`main.cpp`) | Current version |
|---|---|
| `WiFiInterface::get_default_instance()`, `wifi->connect(SSID, PASSWORD, NSAPI_SECURITY_WPA_WPA2)` (l. 137) | Uplink process: `Driver_WiFi0.Initialize()`, `Activate()` with WPA2 (Arm CMSIS-Driver for the ISM43362) |
| `MBED_CONF_APP_WIFI_SSID` etc. in `mbed_app.json` (committed, empty) | `Core/Inc/app_secrets.h` (git-ignored; `app_secrets.h.template` committed) |
| `scan_demo()` with `new WiFiAccessPoint[count]` (l. 75) at every start | diagnostic mode `APP_DIAG_WIFI_BRINGUP`, static scan list, only when asked for |
| NTP time request at start (l. 149) | dropped: ThingSpeak time-stamps each entry itself (`created_at`) |
| `gethostbyname` once at start | `SocketGetHostByName()` before every upload |
| `socket.open()` / `connect()` / `send()` / `recv()` / `close()` in the loop (l. 165–221) | `UplinkProcess::upload()`: create, connect, send-all loop, bounded receive, close |
| `BSP_TSENSOR_ReadTemp()` etc. in the same loop (l. 181–183) | Sensors process, sole owner of I2C2, every 5 s; datasheet conversion (`hts221_calib.c`) |
| `printf` from `main()` | Console process, the only printer; Uplink sends `UplinkStatus` messages |
| `ThisThread::sleep_for(60s)` after each upload (l. 222) | relative upload timer, 60 s after the end of an attempt (`APP_UPLOAD_PERIOD_S`, at least 15) |
| `return -1` / `return NULL` on any network error (l. 127–176, 201) | never ends: report, back-off, rejoin, module reset (`UplinkProcess::run()`) |
| fields as a JSON body of a `GET` request with `Content-Type: application/json` (l. 187) | fields in the query string: `GET /update?api_key=...&field1=..&field2=..&field3=.. HTTP/1.1`, `Connection: close` |
| response printed, not checked | HTTP status and entry id parsed (`tsParseResponse()`); entry id 0 reported as rejected |

## Bugs fixed

1. **Negative values** (`floatToStr()`, l. 107): integer and fractional part were split with casts,
   so -0.5 became `0.-50`. Now `format_fixed()` (`Core/Src/fixed_point.cpp`), tested on the host.
2. **Partial sends** (l. 196–205): `socket.send(sbuffer + response, size)` advanced the buffer by the
   size of the *last* chunk only; after two partial sends it sent the wrong part of the request. Now
   the offset is the running total (`sent += k`).
3. **Unterminated receive buffer** (l. 210–218): `recv` into `char rbuffer[64]` filled all 64 bytes
   with no terminating `'\0'`, and `strstr(rbuffer, "\r\n") - rbuffer` gave a meaningless length if
   no line end was found (NULL minus a pointer). Now a 2 KB buffer that always keeps room for the
   terminator, and the status line is found only after termination.
4. **Program ended on the first network error** (`return -1` from `main()`); a lost Wi-Fi connection
   stopped all uploads until reset. Now every error is reported and retried.
5. **Temperature and humidity read low** (ST's HTS221 driver; see above).
6. **Dynamic allocation** (`new WiFiAccessPoint[]`): none in the application now (`malloc()` is
   never called; checked over SWD after every recorded run).
