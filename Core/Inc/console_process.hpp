// console_process.hpp -- Console: the only process that prints.
//
// Waits in one ALT for whichever comes first:
//   - a Sample from the Sensors process: one line per sample;
//   - the one-off Hts221Check from the Sensors process: calibration and conversion report.
// Output goes to the ST-LINK virtual COM port (115200 8N1).
#pragma once

#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "hts221_check.hpp"

class ConsoleProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack (printf)
public:
    ConsoleProcess(csp::Chanin<Sample> samples, csp::Chanin<Hts221Check> check)
        : samples_(samples), check_(check) {}
    const char* name() const override { return "Console"; }
    void run() override;

private:
    void printSample(const Sample& s);
    void printCheck(const Hts221Check& c);

    csp::Chanin<Sample> samples_;
    csp::Chanin<Hts221Check> check_;
    uint32_t lastSeq_ = 0;
};
