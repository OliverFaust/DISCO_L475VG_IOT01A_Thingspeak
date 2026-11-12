// sensors_process.hpp -- Sensors: the only process that uses I2C2.
//
// Reads temperature and humidity (HTS221) and pressure (LPS22HB) through ST's BSP, once
// immediately and then every `period`, and writes each reading as a Sample to `out`.
// `out` is meant to be a BufferedChannel<Sample, 1, KeepNewest>: the write never blocks,
// so a slow reader can never delay sampling; it simply gets the newest sample.
#pragma once

#include "csp/csp4cmsis.h"
#include "sample.hpp"

class SensorsProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack
public:
    SensorsProcess(csp::Chanout<Sample> out, csp::Time period) : out_(out), period_(period) {}
    const char* name() const override { return "Sensors"; }
    void run() override;

private:
    bool initSensors();
    Sample read(uint32_t seq);

    csp::Chanout<Sample> out_;
    csp::Time period_;
    bool ready_ = false;     // all three sensors initialised
};
