// sensors_process.cpp -- see sensors_process.hpp.
#include "sensors_process.hpp"

#include "cmsis_os2.h"
#include "sensor_io.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"

using namespace csp;

void SensorsProcess::run()
{
    uint32_t seq = 0;
    for (;;) {
        // (Re)initialise until all sensors answer; after a failed reading, start again.
        if (!ready_) ready_ = initSensors();

        Sample s = ready_ ? read(++seq) : Sample{++seq, osKernelGetTickCount(), 0.0f, 0.0f, 0.0f, false};
        if (!s.valid) ready_ = false;
        out_ << s;            // KeepNewest, capacity 1: never blocks

        // Wait for the next period. An ALT with only a timeout guard behaves like
        // SleepFor(period_); written as an ALT so that further guards (for example a
        // "sample now" request channel) can be added without changing the structure.
        // The timeout is relative: each period is `period_` plus the time a reading
        // takes (about 2 ms), so the samples drift slowly against wall-clock time.
        RelTimeoutGuard next(period_);
        Alternative alt(next);
        (void)alt.priSelect();
    }
}

bool SensorsProcess::initSensors()
{
    // Each BSP_xSENSOR_Init() checks the sensor's WHO_AM_I register and configures it
    // (HTS221: 1 Hz continuous conversion; LPS22HB: 25 Hz with low-pass filter).
    const bool t = BSP_TSENSOR_Init() == TSENSOR_OK;   // temperature from the HTS221
    const bool h = BSP_HSENSOR_Init() == HSENSOR_OK;
    const bool p = BSP_PSENSOR_Init() == PSENSOR_OK;
    return t && h && p;
}

Sample SensorsProcess::read(uint32_t seq)
{
    const uint32_t errors_before = sensor_io_error_count();
    Sample s{};
    s.seq           = seq;
    s.time_ms       = osKernelGetTickCount();
    s.temperature_c = BSP_TSENSOR_ReadTemp();
    s.humidity_pct  = BSP_HSENSOR_ReadHumidity();
    s.pressure_hpa  = BSP_PSENSOR_ReadPressure();
    s.valid         = sensor_io_error_count() == errors_before;
    return s;
}
