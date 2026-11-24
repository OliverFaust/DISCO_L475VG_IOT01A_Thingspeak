// wifi_bringup_test.cpp -- see wifi_bringup_test.hpp.
#include "wifi_bringup_test.hpp"

#include <cstdio>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "Driver_WiFi.h"

extern "C" ARM_DRIVER_WIFI Driver_WiFi0;     // Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c

using namespace csp;

namespace {

constexpr uint32_t kMaxAccessPoints = 20;
ARM_WIFI_SCAN_INFO_t scanInfo[kMaxAccessPoints];   // 20 x 44 bytes, static (not on the stack)

const char* securityName(uint8_t s)
{
    switch (s) {
        case ARM_WIFI_SECURITY_OPEN: return "open";
        case ARM_WIFI_SECURITY_WEP:  return "WEP";
        case ARM_WIFI_SECURITY_WPA:  return "WPA";
        case ARM_WIFI_SECURITY_WPA2: return "WPA2";
        default:                     return "unknown";
    }
}

const char* result(int32_t r)
{
    switch (r) {
        case ARM_DRIVER_OK:                 return "OK";
        case ARM_DRIVER_ERROR:              return "ERROR";
        case ARM_DRIVER_ERROR_BUSY:         return "ERROR_BUSY";
        case ARM_DRIVER_ERROR_TIMEOUT:      return "ERROR_TIMEOUT";
        case ARM_DRIVER_ERROR_UNSUPPORTED:  return "ERROR_UNSUPPORTED";
        case ARM_DRIVER_ERROR_PARAMETER:    return "ERROR_PARAMETER";
        default:                            return "error";
    }
}

// The module's info string contains CR/LF; print it on one line.
void printOneLine(const char* s)
{
    for (; *s != '\0'; ++s) {
        std::putchar((*s == '\r' || *s == '\n') ? ' ' : *s);
    }
}

void printHeap(const char* when)
{
    std::printf("  FreeRTOS heap %s: free %u of %u bytes, minimum ever free %u bytes\r\n", when,
                static_cast<unsigned>(xPortGetFreeHeapSize()), static_cast<unsigned>(configTOTAL_HEAP_SIZE),
                static_cast<unsigned>(xPortGetMinimumEverFreeHeapSize()));
}

} // namespace

void WifiBringUpTest::run()
{
    std::printf("\r\nDISCO_L475VG_IOT01A_Thingspeak -- phase 3 step 1: Wi-Fi bring-up test\r\n");
    const ARM_DRIVER_VERSION v = Driver_WiFi0.GetVersion();
    std::printf("CMSIS-Driver WiFi API %u.%u, ISM43362 driver %u.%u\r\n",
                v.api >> 8, v.api & 0xFFU, v.drv >> 8, v.drv & 0xFFU);
    printHeap("before WiFi_Initialize");

    uint32_t t0 = osKernelGetTickCount();
    int32_t r = Driver_WiFi0.Initialize(nullptr);
    std::printf("Initialize: %s (%lu ms)\r\n", result(r), static_cast<unsigned long>(osKernelGetTickCount() - t0));
    if (r != ARM_DRIVER_OK) return;
    r = Driver_WiFi0.PowerControl(ARM_POWER_FULL);
    std::printf("PowerControl(FULL): %s\r\n", result(r));
    printHeap("after Initialize");

    static char info[128];
    r = Driver_WiFi0.GetModuleInfo(info, sizeof info);
    std::printf("Module info: %s: ", result(r));
    if (r == ARM_DRIVER_OK) printOneLine(info);
    std::printf("\r\n");

    uint8_t mac[6] = {};
    uint32_t len = sizeof mac;
    r = Driver_WiFi0.GetOption(0U, ARM_WIFI_MAC, mac, &len);   // interface 0 = station
    std::printf("MAC address (station): %s: %02X:%02X:%02X:%02X:%02X:%02X\r\n", result(r),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    t0 = osKernelGetTickCount();
    const int32_t n = Driver_WiFi0.Scan(scanInfo, kMaxAccessPoints);
    std::printf("Scan: %ld access point(s) (%lu ms)\r\n", static_cast<long>(n),
                static_cast<unsigned long>(osKernelGetTickCount() - t0));
    for (int32_t i = 0; i < n; ++i) {
        const ARM_WIFI_SCAN_INFO_t& a = scanInfo[i];
        std::printf("  %2ld  %-32s  %02X:%02X:%02X:%02X:%02X:%02X  ch %3u  %4d dBm  %s\r\n",
                    static_cast<long>(i + 1), a.ssid, a.bssid[0], a.bssid[1], a.bssid[2], a.bssid[3],
                    a.bssid[4], a.bssid[5], a.ch, static_cast<int>(a.rssi) - 255, securityName(a.security));
    }
    printHeap("at the end");
    std::printf("Bring-up test finished.\r\n");
    // Returning ends this process's thread; the Wi-Fi driver's own thread keeps running.
}
