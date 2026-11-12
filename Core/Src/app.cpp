// app.cpp -- the process network (phase 1: Sensors -> Console).
//
//   +---------+   samples: BufferedChannel<Sample, 1, KeepNewest>   +---------+
//   | Sensors | -------------------------------------------------> | Console |
//   +---------+                                                     +---------+
//    owns I2C2                                                       only printer
//
// Everything is statically allocated: the MainApp thread's stack and control block below,
// the channel and the processes as function-local statics (each CSProcessStatic<N> holds
// its own stack and control block). The FreeRTOS heap is not used by the application.
#include "app.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "sensors_process.hpp"
#include "console_process.hpp"

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

    static BufferedChannel<Sample, 1, BufferPolicy::KeepNewest> samples;
    static SensorsProcess sensors(samples.writer(), Milliseconds(kSamplePeriodMs));
    static ConsoleProcess console(samples.reader());

    Run(InParallel(sensors, console), ExecutionMode::StaticNetwork, kNetworkPriority);
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
