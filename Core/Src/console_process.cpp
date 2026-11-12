// console_process.cpp -- see console_process.hpp.
#include "console_process.hpp"

#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"                 // tskKERNEL_VERSION_NUMBER
#include "csp/csp_version.h"
#include "fixed_point.hpp"
#include "sensor_io.h"
#include "stm32l4xx_hal.h"        // SystemCoreClock

using namespace csp;

void ConsoleProcess::run()
{
    std::printf("\r\n"
                "DISCO_L475VG_IOT01A_Thingspeak -- phase 1: Sensors + Console\r\n"
                "CSP4CMSIS %d.%d.%d, FreeRTOS %s (CMSIS-RTOS2), SYSCLK %lu MHz\r\n",
                CSP4CMSIS_VERSION_MAJOR, CSP4CMSIS_VERSION_MINOR, CSP4CMSIS_VERSION_PATCH,
                tskKERNEL_VERSION_NUMBER, static_cast<unsigned long>(SystemCoreClock / 1000000UL));

    Sample s{};
    for (;;) {
        samples_ >> s;            // blocks until the Sensors process has written a sample
        printSample(s);
    }
}

void ConsoleProcess::printSample(const Sample& s)
{
    const unsigned long sec = s.time_ms / 1000UL, ms = s.time_ms % 1000UL;

    // KeepNewest overwrites a sample the Console has not read yet; seq shows it.
    if (lastSeq_ != 0 && s.seq != lastSeq_ + 1) {
        std::printf("[%5lu.%03lu s] (%lu sample(s) overwritten before they were printed)\r\n",
                    sec, ms, static_cast<unsigned long>(s.seq - lastSeq_ - 1));
    }
    lastSeq_ = s.seq;

    if (!s.valid) {
        std::printf("[%5lu.%03lu s] #%lu  sensor read failed (I2C errors so far: %lu)\r\n",
                    sec, ms, static_cast<unsigned long>(s.seq),
                    static_cast<unsigned long>(sensor_io_error_count()));
        return;
    }

    char t[16], h[16], p[16];
    format_fixed(t, sizeof t, s.temperature_c, 2);
    format_fixed(h, sizeof h, s.humidity_pct, 2);
    format_fixed(p, sizeof p, s.pressure_hpa, 2);
    std::printf("[%5lu.%03lu s] #%lu  T = %s C  RH = %s %%  p = %s hPa\r\n",
                sec, ms, static_cast<unsigned long>(s.seq), t, h, p);
}
