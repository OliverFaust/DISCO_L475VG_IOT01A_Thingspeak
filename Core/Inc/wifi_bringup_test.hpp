// wifi_bringup_test.hpp -- phase 3, step 1: bring up the ISM43362 Wi-Fi module.
//
// A process that initialises Arm's CMSIS-Driver for the ISM43362 (Driver_WiFi0 on SPI3), prints
// the module's firmware information and MAC address, scans for access points and lists them, and
// reports how much of the FreeRTOS heap the driver took. It is started by MainApp instead of the
// Sensors/Console network when APP_DIAGNOSTIC_MODE is APP_DIAG_WIFI_BRINGUP (Core/Inc/app_config.h),
// so it is the only printer.
#pragma once

#include "csp/csp4cmsis.h"

class WifiBringUpTest : public csp::CSProcessStatic<768> {   // 768 words = 3 KB (printf, scan list)
public:
    const char* name() const override { return "WiFiTest"; }
    void run() override;
};
