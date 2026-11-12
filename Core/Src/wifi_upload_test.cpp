// wifi_upload_test.cpp -- see wifi_upload_test.hpp.
#include "wifi_upload_test.hpp"

#include <cstdio>
#include <cstring>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "Driver_WiFi.h"
#include "app_secrets_include.h"
#include "fixed_point.hpp"
#include "hts221_calib.h"
#include "sensor_io.h"
#include "thingspeak_http.hpp"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"

extern "C" ARM_DRIVER_WIFI Driver_WiFi0;     // Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c

using namespace csp;

namespace {

constexpr uint32_t kRecvTimeoutMs = 10000;   // per SocketRecv call
char request[256];                            // static: not on the stack
char printable[256];
char response[2048];                          // ThingSpeak's headers are about 0.9 KB

void printTime()
{
    const uint32_t ms = osKernelGetTickCount();
    std::printf("[%5lu.%03lu s] ", static_cast<unsigned long>(ms / 1000U), static_cast<unsigned long>(ms % 1000U));
}

const char* result(int32_t r)
{
    switch (r) {
        case ARM_DRIVER_OK:                return "OK";
        case ARM_DRIVER_ERROR_TIMEOUT:     return "ERROR_TIMEOUT";
        case ARM_DRIVER_ERROR_PARAMETER:   return "ERROR_PARAMETER";
        case ARM_DRIVER_ERROR_UNSUPPORTED: return "ERROR_UNSUPPORTED";
        case ARM_SOCKET_EAGAIN:            return "EAGAIN (timed out)";
        case ARM_SOCKET_ETIMEDOUT:         return "ETIMEDOUT";
        case ARM_SOCKET_ENOTCONN:          return "ENOTCONN";
        case ARM_SOCKET_ECONNRESET:        return "ECONNRESET";
        case ARM_SOCKET_ECONNABORTED:      return "ECONNABORTED";
        case ARM_SOCKET_EHOSTNOTFOUND:     return "EHOSTNOTFOUND";
        default:                           return (r < 0) ? "error" : "OK";
    }
}

// Prints text that contains CR LF line by line, each line indented.
void printLines(const char* s, std::size_t len)
{
    std::printf("    ");
    for (std::size_t i = 0; i < len; ++i) {
        if (s[i] == '\r') continue;
        if (s[i] == '\n') { std::printf("\r\n    "); continue; }
        std::putchar(s[i]);
    }
    std::printf("\r\n");
}

bool readSample(float& t, float& h, float& p)
{
    Hts221Calib calib;
    int16_t h_out = 0, t_out = 0;
    const uint32_t errors_before = sensor_io_error_count();
    const bool ok = BSP_TSENSOR_Init() == TSENSOR_OK && BSP_HSENSOR_Init() == HSENSOR_OK &&
                    BSP_PSENSOR_Init() == PSENSOR_OK && hts221_calib_read(&calib) &&
                    hts221_read_raw(&h_out, &t_out);
    t = hts221_calib_temperature(&calib, t_out);
    h = hts221_calib_humidity(&calib, h_out);
    p = BSP_PSENSOR_ReadPressure();
    return ok && sensor_io_error_count() == errors_before;
}

} // namespace

void WifiUploadTest::run()
{
    std::printf("\r\nDISCO_L475VG_IOT01A_Thingspeak -- phase 3 step 2: one ThingSpeak update\r\n");

    // 1. One sample.
    float t = 0, h = 0, p = 0;
    if (!readSample(t, h, p)) { printTime(); std::printf("sensor read failed\r\n"); return; }
    char ts[16], hs[16], ps[16];
    format_fixed(ts, sizeof ts, t, 2); format_fixed(hs, sizeof hs, h, 2); format_fixed(ps, sizeof ps, p, 2);
    printTime(); std::printf("sample: T = %s C  RH = %s %%  p = %s hPa\r\n", ts, hs, ps);

    // 2. Wi-Fi module.
    int32_t r = Driver_WiFi0.Initialize(nullptr);
    if (r == ARM_DRIVER_OK) r = Driver_WiFi0.PowerControl(ARM_POWER_FULL);
    printTime(); std::printf("Wi-Fi Initialize/PowerControl: %s\r\n", result(r));
    if (r != ARM_DRIVER_OK) return;

    // 3. Join (WPA2-Personal). The SSID is shown with its first 3 characters only.
    char ssid_shown[40];
    std::snprintf(ssid_shown, sizeof ssid_shown, "%.3s", APP_WIFI_SSID);
    for (std::size_t i = std::strlen(ssid_shown); i < std::strlen(APP_WIFI_SSID) && i + 1 < sizeof ssid_shown; ++i) {
        ssid_shown[i] = '*'; ssid_shown[i + 1] = '\0';
    }
    ARM_WIFI_CONFIG_t cfg{};
    cfg.ssid = APP_WIFI_SSID;
    cfg.pass = APP_WIFI_PASSWORD;
    cfg.security = ARM_WIFI_SECURITY_WPA2;
    cfg.ch = 0;                                  // any channel
    cfg.wps_method = ARM_WIFI_WPS_METHOD_NONE;
    uint32_t t0 = osKernelGetTickCount();
    r = Driver_WiFi0.Activate(0U, &cfg);         // interface 0 = station
    printTime(); std::printf("join \"%s\" (WPA2): %s (%lu ms)\r\n", ssid_shown, result(r),
                             static_cast<unsigned long>(osKernelGetTickCount() - t0));
    if (r != ARM_DRIVER_OK) return;
    uint8_t ip[4] = {};
    uint32_t len = sizeof ip;
    if (Driver_WiFi0.GetOption(0U, ARM_WIFI_IP, ip, &len) == ARM_DRIVER_OK) {
        printTime(); std::printf("IP address (DHCP): %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
    }

    // 4. DNS.
    uint8_t server[4] = {};
    len = sizeof server;
    t0 = osKernelGetTickCount();
    r = Driver_WiFi0.SocketGetHostByName(kThingSpeakHost, ARM_SOCKET_AF_INET, server, &len);
    printTime(); std::printf("resolve %s: %s -> %u.%u.%u.%u (%lu ms)\r\n", kThingSpeakHost, result(r),
                             server[0], server[1], server[2], server[3],
                             static_cast<unsigned long>(osKernelGetTickCount() - t0));

    // 5. TCP connection, request, response.
    int32_t sock = -1;
    if (r == ARM_DRIVER_OK) {
        sock = Driver_WiFi0.SocketCreate(ARM_SOCKET_AF_INET, ARM_SOCKET_SOCK_STREAM, ARM_SOCKET_IPPROTO_TCP);
        const uint32_t timeout = kRecvTimeoutMs;
        if (sock >= 0) (void)Driver_WiFi0.SocketSetOpt(sock, ARM_SOCKET_SO_RCVTIMEO, &timeout, sizeof timeout);
        r = (sock >= 0) ? Driver_WiFi0.SocketConnect(sock, server, 4U, kThingSpeakPort) : sock;
        printTime(); std::printf("connect %u.%u.%u.%u:%u: %s\r\n", server[0], server[1], server[2], server[3],
                                 kThingSpeakPort, result(r));
    }
    if (r == ARM_DRIVER_OK) {
        const std::size_t n = tsBuildUpdate(request, sizeof request, APP_THINGSPEAK_WRITE_KEY, t, h, p);
        char key_shown[40];
        tsMaskKey(key_shown, sizeof key_shown, APP_THINGSPEAK_WRITE_KEY);
        const std::size_t np = tsBuildUpdate(printable, sizeof printable, key_shown, t, h, p);
        printTime(); std::printf("request (%u bytes, key masked):\r\n", static_cast<unsigned>(n));
        printLines(printable, np);

        // Send all of it: SocketSend may accept less than asked for (the Mbed version ignored this).
        std::size_t sent = 0;
        while (sent < n) {
            const int32_t s = Driver_WiFi0.SocketSend(sock, request + sent, static_cast<uint32_t>(n - sent));
            if (s <= 0) { r = s; break; }
            sent += static_cast<std::size_t>(s);
        }
        printTime(); std::printf("sent %u of %u bytes: %s\r\n", static_cast<unsigned>(sent), static_cast<unsigned>(n),
                                 sent == n ? "OK" : result(r));

        // Receive until the response is complete, the server closes, or a timeout. The buffer is
        // never overrun and is always NUL-terminated (the Mbed version did neither).
        std::size_t got = 0;
        TsResponse resp;
        t0 = osKernelGetTickCount();
        while (sent == n && got < sizeof response - 1) {
            const int32_t k = Driver_WiFi0.SocketRecv(sock, response + got, static_cast<uint32_t>(sizeof response - 1 - got));
            if (k <= 0) { r = k; break; }
            got += static_cast<std::size_t>(k);
            resp = tsParseResponse(response, got);
            if (resp.bodyComplete) break;
        }
        response[got] = '\0';
        printTime(); std::printf("received %u bytes in %lu ms%s%s\r\n", static_cast<unsigned>(got),
                                 static_cast<unsigned long>(osKernelGetTickCount() - t0),
                                 resp.bodyComplete ? "" : ", last SocketRecv: ", resp.bodyComplete ? "" : result(r));
        const char* eol = std::strstr(response, "\r\n");      // response is terminated: strstr is safe
        printTime(); std::printf("status line: %.*s\r\n", eol ? static_cast<int>(eol - response) : static_cast<int>(got), response);
        printTime();
        if (resp.entryId > 0)       std::printf("ThingSpeak entry id: %ld\r\n", resp.entryId);
        else if (resp.entryId == 0) std::printf("ThingSpeak rejected the update (entry id 0)\r\n");
        else                        std::printf("no entry id in the response\r\n");
    }
    if (sock >= 0) {
        r = Driver_WiFi0.SocketClose(sock);
        printTime(); std::printf("SocketClose: %s\r\n", result(r));
    }
    r = Driver_WiFi0.Deactivate(0U);
    printTime(); std::printf("Deactivate: %s\r\n", result(r));
    printTime(); std::printf("FreeRTOS heap: free %u of %u bytes, minimum ever free %u bytes\r\n",
                             static_cast<unsigned>(xPortGetFreeHeapSize()), static_cast<unsigned>(configTOTAL_HEAP_SIZE),
                             static_cast<unsigned>(xPortGetMinimumEverFreeHeapSize()));
    printTime(); std::printf("One-upload test finished.\r\n");
}
