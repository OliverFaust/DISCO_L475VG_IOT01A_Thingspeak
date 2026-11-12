// uplink_process.hpp -- Uplink: the only process that uses the Wi-Fi module.
//
// Keeps the newest Sample from the Sensors process and sends it to ThingSpeak once per upload
// period. Joins the access point in Core/Inc/app_secrets.h, reconnects with back-off, and never
// ends because of a network error. Everything it has to say goes to the Console as UplinkStatus
// messages; it prints nothing itself.
//
// Channels:
//   samples  in : BufferedChannel<Sample, 1, KeepNewest>        (from Sensors; never blocks Sensors)
//   status   out: BufferedChannel<UplinkStatus, 8, KeepNewest>  (to Console; never blocks Uplink)
//
// Runs at osPriorityLow, below Sensors and Console: the Wi-Fi driver waits for the module by
// polling (osDelay(1) loops, short busy-waits during SPI transfers), and a slow or stalled network
// must never delay sampling or printing.
#pragma once

#include <cstddef>
#include "csp/csp4cmsis.h"
#include "sample.hpp"
#include "uplink_status.hpp"
#include "thingspeak_http.hpp"

class UplinkProcess : public csp::CSProcessStatic<1024> {   // 1024 words = 4 KB; buffers are members
public:
    // ThingSpeak's minimum interval between updates of a channel (free accounts).
    static constexpr uint32_t kMinPeriodMs = 15000;

    UplinkProcess(csp::Chanin<Sample> samples, csp::Chanout<UplinkStatus> status, uint32_t periodMs)
        : samples_(samples), status_(status), periodMs_(periodMs < kMinPeriodMs ? kMinPeriodMs : periodMs) {}
    const char* name() const override { return "Uplink"; }
    osPriority_t taskPriority() const override { return osPriorityLow; }
    void run() override;

private:
    bool initModule();
    bool join(uint32_t& durationMs, int32_t& code);
    void leave();
    // One update with the given sample. Returns the event (Uploaded, Rejected or UploadFailed).
    UplinkEvent upload(const Sample& s, UplinkStatus& st);
    // Waits `ms`, meanwhile taking every new sample into latest_.
    void waitKeepingLatest(uint32_t ms);
    void report(UplinkStatus st);
    void reportEvent(UplinkEvent e);

    csp::Chanin<Sample> samples_;
    csp::Chanout<UplinkStatus> status_;
    const uint32_t periodMs_;

    Sample latest_{};
    bool haveSample_ = false;
    uint32_t uploadsOk_ = 0, uploadsRejected_ = 0, uploadsFailed_ = 0, joins_ = 0;

    char request_[256];
    char response_[2048];      // ThingSpeak's response headers are about 0.9 KB
};
