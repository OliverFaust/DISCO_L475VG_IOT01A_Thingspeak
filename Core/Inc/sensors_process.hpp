// sensors_process.hpp -- Sensors: the only process that uses I2C2.
//
// Reads temperature and humidity (HTS221) and pressure (LPS22HB) through ST's BSP, once
// immediately and then every `period`, and writes each reading as a Sample to `out`.
// `out` is meant to be a BufferedChannel<Sample, 1, KeepNewest>: the write never blocks,
// so a slow reader can never delay sampling; it simply gets the newest sample.
//
// Once, after the sensors first initialise, it also reads the HTS221's calibration registers and
// one raw sample and writes them to `check` (a Hts221Check message, printed by the Console).
#pragma once

#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "hts221_check.hpp"

class SensorsProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack
public:
    SensorsProcess(csp::Chanout<Sample> out, csp::Chanout<Hts221Check> check, csp::Time period)
        : out_(out), check_(check), period_(period) {}
    const char* name() const override { return "Sensors"; }
    void run() override;

private:
    bool initSensors();
    Sample read(uint32_t seq);
    Hts221Check readHts221Check();

    csp::Chanout<Sample> out_;
    csp::Chanout<Hts221Check> check_;
    csp::Time period_;
    bool ready_ = false;     // all three sensors initialised
    bool checked_ = false;   // Hts221Check sent
};
