// console_process.cpp -- see console_process.hpp.
#include "console_process.hpp"

#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"                 // tskKERNEL_VERSION_NUMBER
#include "cmsis_os2.h"
#include "csp/csp_version.h"
#include "fixed_point.hpp"
#include "sensor_io.h"
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
                "DISCO_L475VG_IOT01A_Thingspeak -- phase 1: Sensors + Console\r\n"
                "CSP4CMSIS %d.%d.%d, FreeRTOS %s (CMSIS-RTOS2), SYSCLK %lu MHz\r\n",
                CSP4CMSIS_VERSION_MAJOR, CSP4CMSIS_VERSION_MINOR, CSP4CMSIS_VERSION_PATCH,
                tskKERNEL_VERSION_NUMBER, static_cast<unsigned long>(SystemCoreClock / 1000000UL));

    Sample s{};
    Hts221Check c{};
    for (;;) {
        Alternative alt(samples_ | s, check_ | c);
        switch (alt.priSelect()) {
            case 0: printSample(s); break;
            default: printCheck(c); break;
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
    std::printf("  T : datasheet formula %s C, ST driver formula %s C, BSP_TSENSOR_ReadTemp() %s C\r\n", a, b, d);
    format_fixed(a, sizeof a, v.humidity_pct, 3);
    format_fixed(b, sizeof b, v.st_humidity_pct, 3);
    format_fixed(e, sizeof e, c.bsp_humidity_pct, 3);
    std::printf("  RH: datasheet formula %s %%, ST driver formula %s %%, BSP_HSENSOR_ReadHumidity() %s %%\r\n", a, b, e);
}
