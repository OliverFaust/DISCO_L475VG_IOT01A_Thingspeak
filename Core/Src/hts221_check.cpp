// hts221_check.cpp -- see hts221_check.hpp.
#include "hts221_check.hpp"
#include "hts221_calib.h"

namespace {
// cal[] index of register r (cal[0] is register 0x30).
constexpr int idx(uint8_t r) { return r - 0x30; }

int16_t s16(const uint8_t cal[16], uint8_t low_reg) {
    return static_cast<int16_t>(static_cast<uint16_t>(cal[idx(low_reg)]) |
                                static_cast<uint16_t>(cal[idx(low_reg) + 1] << 8));
}

float clamp_rh(float rh) { return rh < 0.0f ? 0.0f : (rh > 100.0f ? 100.0f : rh); }
} // namespace

Hts221Conversion hts221_convert(const uint8_t cal[16], int16_t h_out, int16_t t_out)
{
    Hts221Conversion c{};
    c.h0_rh_x2   = cal[idx(0x30)];
    c.h1_rh_x2   = cal[idx(0x31)];
    const uint8_t msb = cal[idx(0x35)];                       // T1.9 T1.8 T0.9 T0.8 in bits 3..0
    c.t0_degc_x8 = static_cast<uint16_t>(((msb & 0x03u) << 8) | cal[idx(0x32)]);
    c.t1_degc_x8 = static_cast<uint16_t>(((msb & 0x0Cu) << 6) | cal[idx(0x33)]);
    c.h0_t0_out  = s16(cal, 0x36);
    c.h1_t0_out  = s16(cal, 0x3A);
    c.t0_out     = s16(cal, 0x3C);
    c.t1_out     = s16(cal, 0x3E);

    // Datasheet: the conversion the Sensors process uses for every sample (hts221_calib.c).
    Hts221Calib calib;
    hts221_calib_decode(cal, &calib);
    c.temperature_c = hts221_calib_temperature(&calib, t_out);
    c.humidity_pct  = hts221_calib_humidity(&calib, h_out);

    // ST's driver: the same, with the calibration points truncated to integers first.
    const int t0i = c.t0_degc_x8 >> 3, t1i = c.t1_degc_x8 >> 3;
    const int h0i = c.h0_rh_x2 >> 1,   h1i = c.h1_rh_x2 >> 1;
    c.st_temperature_c = static_cast<float>(t_out - c.t0_out) * static_cast<float>(t1i - t0i) /
                         static_cast<float>(c.t1_out - c.t0_out) + static_cast<float>(t0i);
    c.st_humidity_pct  = clamp_rh(static_cast<float>(h_out - c.h0_t0_out) * static_cast<float>(h1i - h0i) /
                                  static_cast<float>(c.h1_t0_out - c.h0_t0_out) + static_cast<float>(h0i));
    return c;
}
