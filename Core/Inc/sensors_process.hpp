// sensors_process.hpp -- Sensors: the only process that uses I2C2.
//
// Reads temperature and humidity (HTS221) and pressure (LPS22HB), once immediately and then
// every `period`, and writes each reading as a Sample to `out` (Console) and to `uplink` (Uplink). The sensors are initialised by
// ST's BSP; pressure is read through the BSP; temperature and humidity are converted with the
// datasheet formula from the HTS221's calibration, read once at start-up (hts221_calib.c),
// because ST's driver truncates the calibration points.
// Both are meant to be BufferedChannel<Sample, 1, KeepNewest>: the writes never block, so a slow
// reader can never delay sampling; it simply gets the newest sample. Two channels, because each
// buffered channel element is taken by exactly one reader.
//
// Once, after the sensors first initialise, it also reads the HTS221's calibration registers and
// one raw sample and writes them to `check` (a Hts221Check message, printed by the Console).
#pragma once

#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "hts221_check.hpp"
#include "hts221_calib.h"

class SensorsProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack
public:
    SensorsProcess(csp::Chanout<Sample> out, csp::Chanout<Sample> uplink, csp::Chanout<Hts221Check> check,
                   csp::Time period)
        : out_(out), uplink_(uplink), check_(check), period_(period) {}
    const char* name() const override { return "Sensors"; }
    void run() override;

private:
    bool initSensors();
    Sample read(uint32_t seq);
    Hts221Check readHts221Check();

    csp::Chanout<Sample> out_;
    csp::Chanout<Sample> uplink_;
    csp::Chanout<Hts221Check> check_;
    csp::Time period_;
    Hts221Calib calib_{};    // HTS221 factory calibration, read in initSensors()
    bool ready_ = false;     // all three sensors initialised, calibration read
    bool checked_ = false;   // Hts221Check sent
};
