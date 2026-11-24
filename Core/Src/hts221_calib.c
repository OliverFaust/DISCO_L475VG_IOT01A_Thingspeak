/*
 * hts221_calib.c -- see hts221_calib.h.
 *
 * HTS221 datasheet, "Temperature/humidity conversion":
 *   T  = T0 + (T_OUT - T0_OUT)    * (T1 - T0) / (T1_OUT - T0_OUT)
 *   RH = H0 + (H_OUT - H0_T0_OUT) * (H1 - H0) / (H1_T0_OUT - H0_T0_OUT)
 * Calibration registers (read-only, written at the factory):
 *   0x30 H0_rH_x2   0x31 H1_rH_x2   0x32 T0_degC_x8 (bits 7..0)   0x33 T1_degC_x8 (bits 7..0)
 *   0x35 bits 1..0 = T0_degC_x8 bits 9..8, bits 3..2 = T1_degC_x8 bits 9..8
 *   0x36/0x37 H0_T0_OUT   0x3A/0x3B H1_T0_OUT   0x3C/0x3D T0_OUT   0x3E/0x3F T1_OUT
 *   (16-bit values little-endian, two's complement; 0x34, 0x38, 0x39 reserved)
 */
#include "hts221_calib.h"
#include "sensor_io.h"
#include "stm32l475e_iot01_hsensor.h"   /* -> hts221.h: HTS221 register names, SENSOR_IO_* */

#define AUTO_INCREMENT  0x80U           /* bit 7 of the register address: multi-byte read */

static int16_t s16(const uint8_t *p)    /* little-endian signed 16 bit */
{
  return (int16_t)(uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void hts221_calib_decode(const uint8_t cal[16], Hts221Calib *c)
{
  const uint8_t  msb  = cal[0x35 - 0x30];
  const uint16_t t0x8 = (uint16_t)(((msb & 0x03U) << 8) | cal[0x32 - 0x30]);
  const uint16_t t1x8 = (uint16_t)(((msb & 0x0CU) << 6) | cal[0x33 - 0x30]);

  c->t0_degc   = (float)t0x8 / 8.0f;              /* not t0x8 >> 3: keep the 1/8 degC */
  c->t1_degc   = (float)t1x8 / 8.0f;
  c->h0_rh     = (float)cal[0x30 - 0x30] / 2.0f;  /* not >> 1: keep the 1/2 %RH */
  c->h1_rh     = (float)cal[0x31 - 0x30] / 2.0f;
  c->h0_t0_out = s16(&cal[0x36 - 0x30]);
  c->h1_t0_out = s16(&cal[0x3A - 0x30]);
  c->t0_out    = s16(&cal[0x3C - 0x30]);
  c->t1_out    = s16(&cal[0x3E - 0x30]);
}

bool hts221_calib_read(Hts221Calib *c)
{
  uint8_t cal[16];
  const uint32_t errors_before = sensor_io_error_count();
  (void)SENSOR_IO_ReadMultiple(HTS221_I2C_ADDRESS, HTS221_H0_RH_X2 | AUTO_INCREMENT, cal, sizeof cal);
  if (sensor_io_error_count() != errors_before) {
    return false;
  }
  hts221_calib_decode(cal, c);
  return true;
}

bool hts221_read_raw(int16_t *h_out, int16_t *t_out)
{
  uint8_t raw[4];   /* HUMIDITY_OUT_L, _H, TEMP_OUT_L, _H */
  const uint32_t errors_before = sensor_io_error_count();
  (void)SENSOR_IO_ReadMultiple(HTS221_I2C_ADDRESS, HTS221_HR_OUT_L_REG | AUTO_INCREMENT, raw, sizeof raw);
  if (sensor_io_error_count() != errors_before) {
    return false;
  }
  *h_out = s16(&raw[0]);
  *t_out = s16(&raw[2]);
  return true;
}

float hts221_calib_temperature(const Hts221Calib *c, int16_t t_out)
{
  return c->t0_degc + (float)(t_out - c->t0_out) * (c->t1_degc - c->t0_degc) /
                      (float)(c->t1_out - c->t0_out);
}

float hts221_calib_humidity(const Hts221Calib *c, int16_t h_out)
{
  const float rh = c->h0_rh + (float)(h_out - c->h0_t0_out) * (c->h1_rh - c->h0_rh) /
                              (float)(c->h1_t0_out - c->h0_t0_out);
  return rh < 0.0f ? 0.0f : (rh > 100.0f ? 100.0f : rh);
}
