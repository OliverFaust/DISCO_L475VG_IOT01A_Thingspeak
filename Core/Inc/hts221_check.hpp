// hts221_check.hpp -- one-off check of the HTS221 at start-up: calibration registers, one raw
// sample, and its conversion to temperature and humidity, computed two ways.
//
// The Sensors process (sole owner of I2C2) reads the registers and sends a Hts221Check message;
// the Console process prints it. hts221_convert() is pure arithmetic (no I/O), so the same code
// can be checked on a PC against a hand calculation.
//
// HTS221 datasheet (DocID026333), section "Temperature/humidity conversion": each device stores
// two calibration points per quantity in registers 0x30..0x3F; output values are converted by
// linear interpolation between them:
//   T  = T0  + (T_OUT - T0_OUT)    * (T1 - T0)  / (T1_OUT - T0_OUT)       T0 = T0_degC_x8 / 8
//   RH = H0  + (H_OUT - H0_T0_OUT) * (H1 - H0)  / (H1_T0_OUT - H0_T0_OUT)  H0 = H0_rH_x2 / 2
// ST's driver (Drivers/BSP/Components/hts221/hts221.c) uses the same interpolation but first
// truncates T0, T1 to whole degrees (x8 >> 3) and H0, H1 to whole percent (x2 >> 1).
#pragma once

#include <cstdint>
#include <type_traits>

struct Hts221Check {
    uint32_t time_ms;          // kernel tick when the raw sample was read
    bool     ok;               // all transfers succeeded
    uint8_t  hts221_who_am_i;  // expected 0xBC
    uint8_t  lps22hb_who_am_i; // expected 0xB1
    uint8_t  ctrl_reg1;        // HTS221 CTRL_REG1 after the BSP's init (expected 0x85: PD, BDU, 1 Hz)
    uint8_t  cal[16];          // registers 0x30..0x3F, one multi-byte read (auto-increment)
    bool     cal_bytewise_equal; // the same 16 registers read one at a time gave the same bytes
    int16_t  h_out;            // HUMIDITY_OUT (0x28..0x29), raw
    int16_t  t_out;            // TEMP_OUT (0x2A..0x2B), raw
    float    bsp_temperature_c;  // BSP_TSENSOR_ReadTemp(), read right after h_out/t_out
    float    bsp_humidity_pct;   // BSP_HSENSOR_ReadHumidity(), likewise
};
static_assert(std::is_trivially_copyable_v<Hts221Check>, "Hts221Check must be trivially copyable");

// The calibration decoded and one sample converted.
struct Hts221Conversion {
    uint8_t  h0_rh_x2, h1_rh_x2;              // 0x30, 0x31
    uint16_t t0_degc_x8, t1_degc_x8;          // 10 bits: 0x32/0x33 + 2 MSBs each from 0x35
    int16_t  h0_t0_out, h1_t0_out;            // 0x36..0x37, 0x3A..0x3B
    int16_t  t0_out, t1_out;                  // 0x3C..0x3D, 0x3E..0x3F
    float    temperature_c;                   // datasheet formula
    float    humidity_pct;                    // datasheet formula, clamped to 0..100
    float    st_temperature_c;                // ST driver's formula (truncated calibration)
    float    st_humidity_pct;                 // likewise
};

Hts221Conversion hts221_convert(const uint8_t cal[16], int16_t h_out, int16_t t_out);
