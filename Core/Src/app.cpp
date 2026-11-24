// app.cpp -- the process network: Sensors, Console, Uplink.
//
//                 samples (Sample, 1, KeepNewest)
//   +---------+ ----------------------------------------------> +---------+
//   | Sensors | ----------------------------------------------> | Console |  only printer
//   |  I2C2   |   check (Hts221Check, 1, KeepNewest; once)       +---------+
//   +---------+                                                       ^
//        |   uplinkSamples (Sample, 1, KeepNewest)                    | status (UplinkStatus, 8, KeepNewest)
//        v                                                            |
//   +---------+ ------------------------------------------------------+
//   | Uplink  |  Wi-Fi module (SPI3), ThingSpeak; osPriorityLow
//   +---------+
//
// Every channel is buffered with KeepNewest: no writer ever waits for a slower reader.
// Everything is statically allocated: the MainApp thread's stack and control block below,
// the channels and the processes as function-local statics (each CSProcessStatic<N> holds
// its own stack and control block). The application takes nothing from the FreeRTOS heap; the
// Wi-Fi driver takes 384 bytes for its mutexes and event flags (docs/results).
#include "app.h"
#include "app_config.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "hts221_check.hpp"
#include "sensors_process.hpp"
#include "console_process.hpp"
#include "uplink_process.hpp"
#include "uplink_status.hpp"
#include "reset_info.h"
#include "wifi_bringup_test.hpp"
#include "wifi_upload_test.hpp"

using namespace csp;

namespace {

constexpr uint32_t kSamplePeriodMs = 5000;   // Phase 1: print T/H/P every 5 s

// MainApp builds the network and exits; it runs above the network so that it finishes
// before any process starts. All processes run at the one network priority given to Run().
constexpr osPriority_t kMainAppPriority = osPriorityNormal;
constexpr osPriority_t kNetworkPriority = osPriorityBelowNormal;

alignas(8) uint32_t mainAppStack[256];       // 1 KB
StaticTask_t mainAppControlBlock;

void MainApp_Task(void* argument)
{
    (void)argument;

    // Diagnostic modes (app_config.h): one test process instead of the network.
    if constexpr (APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_BRINGUP) {
        static WifiBringUpTest wifiTest;
        Run(InParallel(wifiTest), ExecutionMode::StaticNetwork, kNetworkPriority);
        osThreadExit();
    } else if constexpr (APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_ONE_UPLOAD) {
        static WifiUploadTest uploadTest;
        Run(InParallel(uploadTest), ExecutionMode::StaticNetwork, kNetworkPriority);
        osThreadExit();
    }

    static BufferedChannel<Sample, 1, BufferPolicy::KeepNewest> samples;        // Sensors -> Console
    static BufferedChannel<Sample, 1, BufferPolicy::KeepNewest> uplinkSamples;  // Sensors -> Uplink
    static BufferedChannel<Hts221Check, 1, BufferPolicy::KeepNewest> check;     // Sensors -> Console
    static BufferedChannel<UplinkStatus, 8, BufferPolicy::KeepNewest> status;   // Uplink -> Console
    static SensorsProcess sensors(samples.writer(), uplinkSamples.writer(), check.writer(),
                                  Milliseconds(kSamplePeriodMs));
    const BootInfo boot = reset_info_get();
    static ConsoleProcess console(samples.reader(), check.reader(), status.reader(), boot);
    static UplinkProcess uplink(uplinkSamples.reader(), status.writer(), APP_UPLOAD_PERIOD_S * 1000U);
    console.watchStack(sensors);
    console.watchStack(console);

    if (boot.wifiDisabled) {
        // APP_WIFI_MAX_RESTARTS failed Wi-Fi starts in a row: no further restarts, no Uplink.
        // Sensors' writes to uplinkSamples never block (KeepNewest), so it needs no reader.
        Run(InParallel(sensors, console), ExecutionMode::StaticNetwork, kNetworkPriority);
    } else {
        console.watchStack(uplink);
        // One network priority for all; Uplink overrides it with osPriorityLow (taskPriority()).
        Run(InParallel(sensors, console, uplink), ExecutionMode::StaticNetwork, kNetworkPriority);
    }
    osThreadExit();
}

} // namespace

extern "C" void csp_app_main_init(void)
{
    osThreadAttr_t attr = {};
    attr.name       = "MainApp";
    attr.stack_mem  = mainAppStack;
    attr.stack_size = sizeof(mainAppStack);
    attr.cb_mem     = &mainAppControlBlock;
    attr.cb_size    = sizeof(mainAppControlBlock);
    attr.priority   = kMainAppPriority;
    if (osThreadNew(MainApp_Task, nullptr, &attr) == nullptr) {
        csp4cmsis_fatal_error("MainApp: osThreadNew() failed");
    }
}
