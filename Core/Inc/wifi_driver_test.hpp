// wifi_driver_test.hpp -- test of CMSIS-Driver PR #64 (ISM43362 driver V1.17) on the board.
//
// Branch test/cmsis-driver-pr64 only. A process that calls the driver's Initialize() and
// Uninitialize() in the sequences of the test plan and prints, for each call, the return value, its
// duration, how often the driver drove the module's reset line (RSTN, PE8), and the FreeRTOS heap.
// Started by MainApp instead of the network when APP_DIAGNOSTIC_MODE is APP_DIAG_WIFI_DRIVER_TEST.
//
//   APP_TEST_WIFI_INIT_FAIL 1: test b (module held in reset; Initialize() 5 times, no restart).
//   APP_TEST_WIFI_INIT_FAIL 0: tests d (Uninitialize() without Initialize(), twice), c (Initialize,
//                              Uninitialize, Initialize) and e (recover a module that does not
//                              answer by Uninitialize/Initialize, with join, DNS and TCP connect).
#pragma once

#include "csp/csp4cmsis.h"

class WifiDriverTest : public csp::CSProcessStatic<768> {   // 768 words = 3 KB (printf)
public:
    const char* name() const override { return "DrvTest"; }
    void run() override;
};
