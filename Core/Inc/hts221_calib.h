/*
 * hts221_calib.h -- HTS221 temperature and humidity with the datasheet conversion.
 *
 * ST's driver (Drivers/BSP/Components/hts221/hts221.c, left unmodified) truncates the factory
 * calibration points to whole degrees and whole percent before converting (T0_degC_x8 >> 3,
 * H0_rH_x2 >> 1); on the lab board this reads 0.52 C and 0.66 %RH low
 * (docs/results/2026-11-12_sensor_check.md). These functions use the calibration points as the
 * datasheet defines them: T = x8 / 8 degC, H = x2 / 2 %RH.
 *
 * Use: after the BSP has initialised the sensor (BSP_TSENSOR_Init/BSP_HSENSOR_Init set CTRL_REG1:
 * active, block data update, 1 Hz), read the calibration once with hts221_calib_read(); then, per
 * sample, hts221_read_raw() and the two conversion functions. No state is kept here: the caller
 * owns the Hts221Calib (in this project the Sensors process, the only user of I2C2).
 */
#ifndef HTS221_CALIB_H
#define HTS221_CALIB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float   t0_degc, t1_degc;       /* calibration temperatures, T0_degC_x8 / 8, T1_degC_x8 / 8 */
  float   h0_rh, h1_rh;           /* calibration humidities, H0_rH_x2 / 2, H1_rH_x2 / 2 */
  int16_t t0_out, t1_out;         /* raw TEMP_OUT at T0, T1 */
  int16_t h0_t0_out, h1_t0_out;   /* raw HUMIDITY_OUT at H0, H1 */
} Hts221Calib;

/* Decodes the 16 calibration registers 0x30..0x3F (cal[0] = register 0x30). Pure, no I/O. */
void hts221_calib_decode(const uint8_t cal[16], Hts221Calib *c);

/* Reads registers 0x30..0x3F over I2C2 (one auto-increment read) and decodes them.
   Returns false if the transfer failed (c is then not changed). */
bool hts221_calib_read(Hts221Calib *c);

/* Reads HUMIDITY_OUT and TEMP_OUT (0x28..0x2B, one auto-increment read). False on failure. */
bool hts221_read_raw(int16_t *h_out, int16_t *t_out);

/* Datasheet linear interpolation between the two calibration points. Humidity clamped to 0..100. */
float hts221_calib_temperature(const Hts221Calib *c, int16_t t_out);
float hts221_calib_humidity(const Hts221Calib *c, int16_t h_out);

#ifdef __cplusplus
}
#endif

#endif /* HTS221_CALIB_H */
