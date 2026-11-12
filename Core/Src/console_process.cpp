// console_process.cpp -- see console_process.hpp.
#include "console_process.hpp"

#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"                 // tskKERNEL_VERSION_NUMBER
#include "cmsis_os2.h"
#include "csp/csp_version.h"
#include "fixed_point.hpp"
#include "sensor_io.h"
#include "app_config.h"
#include "stm32l4xx_hal.h"        // SystemCoreClock

using namespace csp;

namespace {
void printTime(uint32_t ms)
{
    std::printf("[%5lu.%03lu s]", static_cast<unsigned long>(ms / 1000UL), static_cast<unsigned long>(ms % 1000UL));
}
} // namespace

void ConsoleProcess::run()
{
    std::printf("\r\n"
                "DISCO_L475VG_IOT01A_Thingspeak -- Sensors, Console, Uplink (ThingSpeak every %u s)\r\n"
                "CSP4CMSIS %d.%d.%d, FreeRTOS %s (CMSIS-RTOS2), SYSCLK %lu MHz\r\n",
                static_cast<unsigned>(APP_UPLOAD_PERIOD_S),
                CSP4CMSIS_VERSION_MAJOR, CSP4CMSIS_VERSION_MINOR, CSP4CMSIS_VERSION_PATCH,
                tskKERNEL_VERSION_NUMBER, static_cast<unsigned long>(SystemCoreClock / 1000000UL));

    uint32_t reportAt = kStackReportFirstMs;     // absolute deadline, in ticks since boot
    Sample s{};
    Hts221Check c{};
    UplinkStatus u{};
    for (;;) {
        // The report time is an absolute deadline; the ALT needs a relative timeout, so it is
        // recomputed on every pass (signed difference: correct across the tick counter's wrap).
        const int32_t left = static_cast<int32_t>(reportAt - osKernelGetTickCount());   // 1 tick = 1 ms
        RelTimeoutGuard reportTime(Milliseconds(left > 0 ? static_cast<uint32_t>(left) : 0U));
        Alternative alt(samples_ | s, check_ | c, uplink_ | u, reportTime);
        switch (alt.priSelect()) {
            case 0: printSample(s); break;
            case 1: printCheck(c); break;
            case 2: printUplink(u); break;
            default:
                printStackReport();
                reportAt += kStackReportPeriodMs;
                break;
        }
    }
}

void ConsoleProcess::printSample(const Sample& s)
{
    // KeepNewest overwrites a sample the Console has not read yet; seq shows it.
    if (lastSeq_ != 0 && s.seq != lastSeq_ + 1) {
        printTime(s.time_ms);
        std::printf(" (%lu sample(s) overwritten before they were printed)\r\n",
                    static_cast<unsigned long>(s.seq - lastSeq_ - 1));
    }
    lastSeq_ = s.seq;

    printTime(s.time_ms);
    if (!s.valid) {
        std::printf(" #%lu  sensor read failed (I2C errors so far: %lu)\r\n",
                    static_cast<unsigned long>(s.seq), static_cast<unsigned long>(sensor_io_error_count()));
        return;
    }
    char t[16], h[16], p[16];
    format_fixed(t, sizeof t, s.temperature_c, 2);
    format_fixed(h, sizeof h, s.humidity_pct, 2);
    format_fixed(p, sizeof p, s.pressure_hpa, 2);
    std::printf(" #%lu  T = %s C  RH = %s %%  p = %s hPa\r\n", static_cast<unsigned long>(s.seq), t, h, p);
}

void ConsoleProcess::printCheck(const Hts221Check& c)
{
    const Hts221Conversion v = hts221_convert(c.cal, c.h_out, c.t_out);
    char a[16], b[16], d[16], e[16];

    printTime(c.time_ms);
    std::printf(" HTS221 check (I2C transfers %s)\r\n", c.ok ? "all OK" : "FAILED");
    std::printf("  WHO_AM_I: HTS221 @0xBE: 0x%02X (expect 0xBC), LPS22HB @0xBA: 0x%02X (expect 0xB1)\r\n",
                c.hts221_who_am_i, c.lps22hb_who_am_i);
    std::printf("  HTS221 CTRL_REG1: 0x%02X (expect 0x85: active, BDU, 1 Hz)\r\n", c.ctrl_reg1);
    std::printf("  calibration 0x30..0x3F:");
    for (unsigned i = 0; i < sizeof c.cal; ++i) std::printf(" %02X", c.cal[i]);
    std::printf("\r\n  same 16 registers read one at a time: %s\r\n", c.cal_bytewise_equal ? "identical" : "DIFFERENT");

    format_fixed(a, sizeof a, v.h0_rh_x2 / 2.0f, 1);
    format_fixed(b, sizeof b, v.h1_rh_x2 / 2.0f, 1);
    std::printf("  H0_rH_x2 = %u -> H0 = %s %%RH, H1_rH_x2 = %u -> H1 = %s %%RH\r\n", v.h0_rh_x2, a, v.h1_rh_x2, b);
    format_fixed(a, sizeof a, v.t0_degc_x8 / 8.0f, 3);
    format_fixed(b, sizeof b, v.t1_degc_x8 / 8.0f, 3);
    std::printf("  T0_degC_x8 = %u -> T0 = %s C, T1_degC_x8 = %u -> T1 = %s C\r\n", v.t0_degc_x8, a, v.t1_degc_x8, b);
    std::printf("  H0_T0_OUT = %d, H1_T0_OUT = %d, T0_OUT = %d, T1_OUT = %d\r\n",
                v.h0_t0_out, v.h1_t0_out, v.t0_out, v.t1_out);
    std::printf("  raw sample: H_OUT = %d, T_OUT = %d\r\n", c.h_out, c.t_out);

    format_fixed(a, sizeof a, v.temperature_c, 3);
    format_fixed(b, sizeof b, v.st_temperature_c, 3);
    format_fixed(d, sizeof d, c.bsp_temperature_c, 3);
    std::printf("  T : datasheet formula (samples) %s C, ST driver formula %s C, BSP_TSENSOR_ReadTemp() %s C\r\n", a, b, d);
    format_fixed(a, sizeof a, v.humidity_pct, 3);
    format_fixed(b, sizeof b, v.st_humidity_pct, 3);
    format_fixed(e, sizeof e, c.bsp_humidity_pct, 3);
    std::printf("  RH: datasheet formula (samples) %s %%, ST driver formula %s %%, BSP_HSENSOR_ReadHumidity() %s %%\r\n", a, b, e);
}

void ConsoleProcess::printStackReport()
{
    printTime(osKernelGetTickCount());
    std::printf(" Stack high-water marks (smallest free stack so far):\r\n");
    std::printf("  FreeRTOS heap: free %u of %u bytes, minimum ever free %u bytes\r\n",
                static_cast<unsigned>(xPortGetFreeHeapSize()), static_cast<unsigned>(configTOTAL_HEAP_SIZE),
                static_cast<unsigned>(xPortGetMinimumEverFreeHeapSize()));
    for (std::size_t i = 0; i < nWatched_; ++i) {
        const CSProcess& p = *watched_[i];
        const uint32_t size = static_cast<uint32_t>(p.stackWords());
        const uint32_t free = p.stackHighWaterMarkWords();
        if (free == CSP_STACK_HWM_UNAVAILABLE) {
            std::printf("  %-8s %4lu words, not started\r\n", p.name(), static_cast<unsigned long>(size));
            continue;
        }
        std::printf("  %-8s %4lu words, min free %4lu words, max used %4lu words (%lu %%)\r\n",
                    p.name(), static_cast<unsigned long>(size), static_cast<unsigned long>(free),
                    static_cast<unsigned long>(size - free),
                    static_cast<unsigned long>((size - free) * 100UL / size));
    }
}

namespace {
const char* stageName(UplinkStage st)
{
    switch (st) {
        case UplinkStage::Dns:     return "DNS";
        case UplinkStage::Socket:  return "socket";
        case UplinkStage::Connect: return "connect";
        case UplinkStage::Send:    return "send";
        case UplinkStage::Receive: return "receive";
        case UplinkStage::Http:    return "HTTP";
        default:                   return "-";
    }
}
} // namespace

void ConsoleProcess::printUplink(const UplinkStatus& u)
{
    printTime(u.time_ms);
    std::printf(" Uplink: ");
    switch (u.event) {
        case UplinkEvent::ModuleReady:
            std::printf("Wi-Fi module ready");
            break;
        case UplinkEvent::ModuleFailed:
            std::printf("Wi-Fi module initialisation FAILED, retry in %lu s", static_cast<unsigned long>(u.retryInMs / 1000));
            break;
        case UplinkEvent::Joined:
            std::printf("joined (%lu ms), IP %u.%u.%u.%u", static_cast<unsigned long>(u.durationMs),
                        u.ip[0], u.ip[1], u.ip[2], u.ip[3]);
            break;
        case UplinkEvent::JoinFailed:
            std::printf("join FAILED (code %ld, after %lu ms), retry in %lu s", static_cast<long>(u.code),
                        static_cast<unsigned long>(u.durationMs), static_cast<unsigned long>(u.retryInMs / 1000));
            break;
        case UplinkEvent::Uploaded:
            std::printf("sample #%lu -> entry %ld (%lu ms)", static_cast<unsigned long>(u.sampleSeq),
                        static_cast<long>(u.entryId), static_cast<unsigned long>(u.durationMs));
            break;
        case UplinkEvent::Rejected:
            std::printf("sample #%lu REJECTED by ThingSpeak (HTTP %ld, entry %ld)", static_cast<unsigned long>(u.sampleSeq),
                        static_cast<long>(u.code), static_cast<long>(u.entryId));
            break;
        case UplinkEvent::UploadFailed:
            std::printf("sample #%lu upload FAILED at %s (code %ld), retry in %lu s", static_cast<unsigned long>(u.sampleSeq),
                        stageName(u.stage), static_cast<long>(u.code), static_cast<unsigned long>(u.retryInMs / 1000));
            break;
        case UplinkEvent::Rejoining:
            std::printf("too many failed uploads: leaving the network, joining again");
            break;
        case UplinkEvent::SampleInvalid:
            std::printf("sample #%lu invalid, not sent", static_cast<unsigned long>(u.sampleSeq));
            break;
    }
    std::printf("  [ok %lu, rejected %lu, failed %lu, joins %lu]\r\n", static_cast<unsigned long>(u.uploadsOk),
                static_cast<unsigned long>(u.uploadsRejected), static_cast<unsigned long>(u.uploadsFailed),
                static_cast<unsigned long>(u.joins));
}
