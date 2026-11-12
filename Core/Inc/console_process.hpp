// console_process.hpp -- Console: the only process that prints.
//
// Phase 1: reads Samples from the Sensors process and prints one line per sample on the
// ST-LINK virtual COM port (115200 8N1).
#pragma once

#include "csp/csp4cmsis.h"
#include "sample.hpp"

class ConsoleProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack (printf)
public:
    explicit ConsoleProcess(csp::Chanin<Sample> samples) : samples_(samples) {}
    const char* name() const override { return "Console"; }
    void run() override;

private:
    void printSample(const Sample& s);

    csp::Chanin<Sample> samples_;
    uint32_t lastSeq_ = 0;
};
