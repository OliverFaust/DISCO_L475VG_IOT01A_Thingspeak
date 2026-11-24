// wifi_driver_test.cpp -- see wifi_driver_test.hpp. Branch test/cmsis-driver-pr64 only.
#include "wifi_driver_test.hpp"
#include "app_config.h"

#if APP_DIAGNOSTIC_MODE != APP_DIAG_WIFI_DRIVER_TEST
void WifiDriverTest::run() {}                // not selected: no references to the test hooks
#else

#include <cstdio>
#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "Driver_WiFi.h"
#include "app_secrets_include.h"
#include "thingspeak_http.hpp"

extern "C" ARM_DRIVER_WIFI Driver_WiFi0;     // Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c

extern "C" {                                 // Core/Src/wifi_ism43362_pins.c
extern volatile uint32_t wifi_test_rstn_assert;
extern volatile uint32_t wifi_test_rstn_release;
extern volatile uint32_t wifi_test_rstn_driven;
extern volatile uint8_t  wifi_test_hold_reset;
extern const int        wifi_test_hw_reset;   // WIFI_ISM43362_HW_RESET as the driver sees it
}

using namespace csp;

namespace {

ARM_DRIVER_WIFI& wifi = Driver_WiFi0;

uint32_t now() { return osKernelGetTickCount(); }   // 1 tick = 1 ms

const char* result(int32_t r)
{
    switch (r) {
        case ARM_DRIVER_OK:                 return "ARM_DRIVER_OK";
        case ARM_DRIVER_ERROR:              return "ARM_DRIVER_ERROR";
        case ARM_DRIVER_ERROR_BUSY:         return "ARM_DRIVER_ERROR_BUSY";
        case ARM_DRIVER_ERROR_TIMEOUT:      return "ARM_DRIVER_ERROR_TIMEOUT";
        case ARM_DRIVER_ERROR_UNSUPPORTED:  return "ARM_DRIVER_ERROR_UNSUPPORTED";
        case ARM_DRIVER_ERROR_PARAMETER:    return "ARM_DRIVER_ERROR_PARAMETER";
        default:                            return "other";
    }
}

void heap(const char* tag)
{
    std::printf("    heap %-28s free %5u  min-ever %5u  (of %u)\r\n", tag,
                static_cast<unsigned>(xPortGetFreeHeapSize()),
                static_cast<unsigned>(xPortGetMinimumEverFreeHeapSize()),
                static_cast<unsigned>(configTOTAL_HEAP_SIZE));
}

struct Rstn { uint32_t a, r, d; };
Rstn rstn() { return { wifi_test_rstn_assert, wifi_test_rstn_release, wifi_test_rstn_driven }; }

// One driver call, timed, with the RSTN calls it made.
template <typename F>
int32_t call(const char* what, F f)
{
    const Rstn before = rstn();
    const uint32_t t0 = now();
    const int32_t r = f();
    const uint32_t ms = now() - t0;
    const Rstn after = rstn();
    std::printf("  %-26s -> %ld %s (%lu ms; RSTN assert %lu, release %lu, level as requested %lu)\r\n",
                what, static_cast<long>(r), result(r), static_cast<unsigned long>(ms),
                static_cast<unsigned long>(after.a - before.a), static_cast<unsigned long>(after.r - before.r),
                static_cast<unsigned long>(after.d - before.d));
    return r;
}

// The module's info string contains CR/LF; print it on one line.
[[maybe_unused]] void moduleInfo()
{
    static char info[128];
    const int32_t r = wifi.GetModuleInfo(info, sizeof info);
    std::printf("  GetModuleInfo              -> %s: ", result(r));
    if (r == ARM_DRIVER_OK) {
        for (const char* s = info; *s != '\0'; ++s) std::putchar((*s == '\r' || *s == '\n') ? ' ' : *s);
    }
    std::printf("\r\n");
}

// Join, DNS, TCP connect to ThingSpeak and close: the network path of an upload, without an update.
[[maybe_unused]] bool networkPath(const char* tag)
{
    bool ok = true;
    ARM_WIFI_CONFIG_t cfg{};
    cfg.ssid = APP_WIFI_SSID;
    cfg.pass = APP_WIFI_PASSWORD;
    cfg.security = ARM_WIFI_SECURITY_WPA2;
    cfg.wps_method = ARM_WIFI_WPS_METHOD_NONE;
    if (call("Activate (join)", [&] { return wifi.Activate(0U, &cfg); }) != ARM_DRIVER_OK) ok = false;
    uint8_t ip[4] = {};
    uint32_t len = sizeof ip;
    const int32_t d = call("SocketGetHostByName", [&] {
        return wifi.SocketGetHostByName(kThingSpeakHost, ARM_SOCKET_AF_INET, ip, &len); });
    if (d == 0) {
        std::printf("    %s -> %u.%u.%u.%u\r\n", kThingSpeakHost, ip[0], ip[1], ip[2], ip[3]);
        const int32_t s = wifi.SocketCreate(ARM_SOCKET_AF_INET, ARM_SOCKET_SOCK_STREAM, ARM_SOCKET_IPPROTO_TCP);
        if (s < 0) {
            std::printf("  SocketCreate -> %ld\r\n", static_cast<long>(s));
            ok = false;
        } else {
            if (call("SocketConnect :80", [&] { return wifi.SocketConnect(s, ip, 4U, kThingSpeakPort); }) != 0) ok = false;
            (void)call("SocketClose", [&] { return wifi.SocketClose(s); });
        }
    } else {
        ok = false;
    }
    std::printf("  RESULT %s network path (join, DNS, TCP connect): %s\r\n", tag, ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

void WifiDriverTest::run()
{
    const ARM_DRIVER_VERSION v = wifi.GetVersion();
    std::printf("\r\nDISCO_L475VG_IOT01A_Thingspeak -- ISM43362 driver test (CMSIS-Driver PR #64)\r\n");
    std::printf("ISM43362 driver %u.%u, WIFI_ISM43362_HW_RESET %d, APP_TEST_WIFI_INIT_FAIL %d\r\n",
                v.drv >> 8, v.drv & 0xFFU, wifi_test_hw_reset, APP_TEST_WIFI_INIT_FAIL);
    heap("at start");

#if APP_TEST_WIFI_INIT_FAIL
    // b: module held in reset by the test switch; Initialize() 5 times without a board restart.
    std::printf("\r\n[b] module held in reset; Initialize() 5 times\r\n");
    unsigned freeAfter[5] = {};
    for (int k = 0; k < 5; ++k) {
        char what[32];
        std::snprintf(what, sizeof what, "Initialize #%d", k + 1);
        (void)call(what, [] { return wifi.Initialize(nullptr); });
        heap("after it");
        freeAfter[k] = static_cast<unsigned>(xPortGetFreeHeapSize());
    }
    bool same = true;
    for (int k = 1; k < 5; ++k) same = same && (freeAfter[k] == freeAfter[0]);
    std::printf("  RESULT b: heap free after each failed Initialize %s (%u %u %u %u %u)\r\n",
                same ? "IDENTICAL" : "DIFFERENT", freeAfter[0], freeAfter[1], freeAfter[2], freeAfter[3], freeAfter[4]);
#else
    // d: Uninitialize() without a prior Initialize(), twice.
    std::printf("\r\n[d1] Uninitialize() without a prior Initialize(), twice\r\n");
    (void)call("Uninitialize (no Init) #1", [] { return wifi.Uninitialize(); });
    (void)call("Uninitialize (no Init) #2", [] { return wifi.Uninitialize(); });
    heap("after d1");

    // c: Initialize, Uninitialize, Initialize.
    std::printf("\r\n[c] Initialize, Uninitialize, Initialize\r\n");
    const int32_t c1 = call("Initialize #1", [] { return wifi.Initialize(nullptr); });
    if (c1 == ARM_DRIVER_OK) { (void)wifi.PowerControl(ARM_POWER_FULL); moduleInfo(); }
    heap("after Initialize #1");
    (void)call("Uninitialize", [] { return wifi.Uninitialize(); });
    heap("after Uninitialize");
    const int32_t c2 = call("Initialize #2", [] { return wifi.Initialize(nullptr); });
    if (c2 == ARM_DRIVER_OK) { (void)wifi.PowerControl(ARM_POWER_FULL); moduleInfo(); }
    heap("after Initialize #2");
    std::printf("  RESULT c: Initialize #1 %s, Initialize #2 %s\r\n", result(c1), result(c2));

    // d: Uninitialize() twice in a row after a successful Initialize().
    std::printf("\r\n[d2] Uninitialize() twice in a row (after Initialize #2)\r\n");
    (void)call("Uninitialize #1", [] { return wifi.Uninitialize(); });
    (void)call("Uninitialize #2", [] { return wifi.Uninitialize(); });
    heap("after d2");

    // e: a module that stops answering, recovered by Uninitialize/Initialize in the same run.
    std::printf("\r\n[e] recovery in the same run\r\n");
    const int32_t e1 = call("Initialize", [] { return wifi.Initialize(nullptr); });
    if (e1 == ARM_DRIVER_OK) (void)wifi.PowerControl(ARM_POWER_FULL);
    const bool before = (e1 == ARM_DRIVER_OK) && networkPath("e before");
    heap("module working");
    // The module stops answering: hold it in reset. Only the driver's next RSTN(0) releases it.
    wifi_test_hold_reset = 1U;
    HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, GPIO_PIN_RESET);
    std::printf("  module held in reset (PE8 low): it does not answer any more\r\n");
    uint8_t ip[4] = {};
    uint32_t len = sizeof ip;
    (void)call("SocketGetHostByName (stuck)", [&] {
        return wifi.SocketGetHostByName(kThingSpeakHost, ARM_SOCKET_AF_INET, ip, &len); });
    (void)call("IsConnected (stuck)", [] { return static_cast<int32_t>(wifi.IsConnected()); });
    wifi_test_hold_reset = 0U;              // PE8 stays low until the driver releases it
    std::printf("  recovery: Uninitialize, Initialize (PE8 still low)\r\n");
    (void)call("Uninitialize", [] { return wifi.Uninitialize(); });
    heap("after Uninitialize");
    const int32_t e2 = call("Initialize", [] { return wifi.Initialize(nullptr); });
    bool after = false;
    if (e2 == ARM_DRIVER_OK) {
        (void)wifi.PowerControl(ARM_POWER_FULL);
        moduleInfo();
        after = networkPath("e after");
    }
    heap("after recovery");
    std::printf("  RESULT e: before %s; recovery Initialize %s; after %s\r\n",
                before ? "OK" : "FAILED", result(e2), after ? "OK" : "FAILED");
    if (e2 == ARM_DRIVER_OK) (void)wifi.Deactivate(0U);
#endif

    heap("at the end");
    std::printf("Driver test finished.\r\n");
}

#endif // APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_DRIVER_TEST
