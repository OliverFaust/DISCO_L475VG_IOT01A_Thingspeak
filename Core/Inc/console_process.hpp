// console_process.hpp -- Console: the only process that prints.
//
// Waits in one ALT for whichever comes first:
//   - a Sample from the Sensors process: one line per sample;
//   - the one-off Hts221Check from the Sensors process: calibration and conversion report;
//   - an UplinkStatus from the Uplink process: one line per event (joined, uploaded, failed, ...);
//   - the report time (kStackReportFirstMs after boot, then every kStackReportPeriodMs): the
//     smallest free stack each watched process has had so far (its stack high-water mark) and
//     the FreeRTOS heap's current and minimum-ever free space.
// Output goes to the ST-LINK virtual COM port (115200 8N1).
#pragma once

#include <cstddef>
#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "hts221_check.hpp"
#include "uplink_status.hpp"

class ConsoleProcess : public csp::CSProcessStatic<512> {   // 512 words = 2 KB stack (printf)
public:
    static constexpr uint32_t kStackReportFirstMs  = 60000;    // first report 60 s after boot,
    static constexpr uint32_t kStackReportPeriodMs = 600000;   // then every 10 minutes
    static constexpr std::size_t kMaxWatched = 4;

    ConsoleProcess(csp::Chanin<Sample> samples, csp::Chanin<Hts221Check> check, csp::Chanin<UplinkStatus> uplink)
        : samples_(samples), check_(check), uplink_(uplink) {}
    const char* name() const override { return "Console"; }
    void run() override;

    // Processes to include in the stack report (call before Run()). Reading a process's stack
    // high-water mark asks the RTOS about that thread; it shares no data with the process.
    void watchStack(const csp::CSProcess& p) { if (nWatched_ < kMaxWatched) watched_[nWatched_++] = &p; }

private:
    void printSample(const Sample& s);
    void printCheck(const Hts221Check& c);
    void printStackReport();
    void printUplink(const UplinkStatus& u);

    csp::Chanin<Sample> samples_;
    csp::Chanin<Hts221Check> check_;
    csp::Chanin<UplinkStatus> uplink_;
    const csp::CSProcess* watched_[kMaxWatched] = {};
    std::size_t nWatched_ = 0;
    uint32_t lastSeq_ = 0;
};
