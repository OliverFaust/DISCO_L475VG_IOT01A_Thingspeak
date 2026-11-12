// sensors_process.cpp -- see sensors_process.hpp.
#include "sensors_process.hpp"

#include "cmsis_os2.h"
#include "sensor_io.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"   // -> lps22hb.h; hts221.h via hsensor: SENSOR_IO_* (extern "C")

using namespace csp;

void SensorsProcess::run()
{
    uint32_t seq = 0;
    for (;;) {
        // (Re)initialise until all sensors answer; after a failed reading, start again.
        if (!ready_) ready_ = initSensors();
        if (ready_ && !checked_) {
            check_ << readHts221Check();   // once; KeepNewest, never blocks
            checked_ = true;
        }

        Sample s = ready_ ? read(++seq) : Sample{++seq, osKernelGetTickCount(), 0.0f, 0.0f, 0.0f, false};
        if (!s.valid) ready_ = false;
        out_ << s;            // KeepNewest, capacity 1: never blocks

        // Wait for the next period. An ALT with only a timeout guard behaves like
        // SleepFor(period_); written as an ALT so that further guards (for example a
        // "sample now" request channel) can be added without changing the structure.
        // The timeout is relative: each period is `period_` plus the time a reading
        // takes (about 2 ms), so the samples drift slowly against wall-clock time.
        RelTimeoutGuard next(period_);
        Alternative alt(next);
        (void)alt.priSelect();
    }
}

bool SensorsProcess::initSensors()
{
    // Each BSP_xSENSOR_Init() checks the sensor's WHO_AM_I register and configures it
    // (HTS221: 1 Hz continuous conversion; LPS22HB: 25 Hz with low-pass filter).
    const bool t = BSP_TSENSOR_Init() == TSENSOR_OK;   // temperature from the HTS221
    const bool h = BSP_HSENSOR_Init() == HSENSOR_OK;
    const bool p = BSP_PSENSOR_Init() == PSENSOR_OK;
    // The HTS221's calibration is constant (written at the factory): read it once here. It is
    // read again only if initialisation is repeated after an I2C error.
    return t && h && p && hts221_calib_read(&calib_);
}

Sample SensorsProcess::read(uint32_t seq)
{
    const uint32_t errors_before = sensor_io_error_count();
    Sample s{};
    s.seq           = seq;
    s.time_ms       = osKernelGetTickCount();
    int16_t h_out = 0, t_out = 0;
    const bool raw_ok = hts221_read_raw(&h_out, &t_out);
    s.temperature_c = hts221_calib_temperature(&calib_, t_out);   // datasheet formula, not
    s.humidity_pct  = hts221_calib_humidity(&calib_, h_out);      // BSP_TSENSOR_ReadTemp() etc.
    s.pressure_hpa  = BSP_PSENSOR_ReadPressure();
    s.valid         = raw_ok && sensor_io_error_count() == errors_before;
    return s;
}

Hts221Check SensorsProcess::readHts221Check()
{
    // Addresses as the BSP defines them: 8-bit form (7-bit address << 1), which is what
    // HAL_I2C_Mem_Read() expects: HTS221 0xBE = 0x5F << 1, LPS22HB 0xBA = 0x5D << 1.
    // HTS221 multi-byte reads need bit 7 of the register address set (auto-increment).
    constexpr uint8_t kAutoInc = 0x80;
    const uint32_t errors_before = sensor_io_error_count();
    Hts221Check c{};

    c.hts221_who_am_i  = SENSOR_IO_Read(HTS221_I2C_ADDRESS, HTS221_WHO_AM_I_REG);
    c.lps22hb_who_am_i = SENSOR_IO_Read(LPS22HB_I2C_ADDRESS, LPS22HB_WHO_AM_I_REG);
    c.ctrl_reg1        = SENSOR_IO_Read(HTS221_I2C_ADDRESS, HTS221_CTRL_REG1);

    (void)SENSOR_IO_ReadMultiple(HTS221_I2C_ADDRESS, 0x30 | kAutoInc, c.cal, sizeof c.cal);
    c.cal_bytewise_equal = true;
    for (uint8_t i = 0; i < sizeof c.cal; ++i) {
        if (SENSOR_IO_Read(HTS221_I2C_ADDRESS, static_cast<uint8_t>(0x30 + i)) != c.cal[i]) {
            c.cal_bytewise_equal = false;
        }
    }

    // One sample: HUMIDITY_OUT_L/H, TEMP_OUT_L/H (0x28..0x2B) in one read, then the BSP's
    // conversion. With BDU on and a 1 Hz output rate the registers almost always still hold
    // the same sample when the BSP reads them a few milliseconds later.
    uint8_t raw[4] = {};
    (void)SENSOR_IO_ReadMultiple(HTS221_I2C_ADDRESS, HTS221_HR_OUT_L_REG | kAutoInc, raw, sizeof raw);
    c.time_ms = osKernelGetTickCount();
    c.h_out = static_cast<int16_t>(raw[0] | (raw[1] << 8));
    c.t_out = static_cast<int16_t>(raw[2] | (raw[3] << 8));
    c.bsp_temperature_c = BSP_TSENSOR_ReadTemp();
    c.bsp_humidity_pct  = BSP_HSENSOR_ReadHumidity();

    c.ok = sensor_io_error_count() == errors_before;
    return c;
}
