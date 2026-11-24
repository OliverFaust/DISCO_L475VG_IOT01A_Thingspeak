/*
 * sensor_io.h -- I2C2 access for ST's sensor drivers (HTS221, LPS22HB).
 *
 * ST's sensor drivers (Drivers/BSP/Components/hts221, lps22hb) call SENSOR_IO_Init/Read/Write/
 * ReadMultiple, declared as extern in hts221.h and lps22hb.h. Core/Src/sensor_io.c implements
 * them on hi2c2, the I2C2 handle that STM32CubeMX generates and initialises in main.c. Only the Sensors process calls the drivers,
 * so only one thread ever uses I2C2 and no lock is needed.
 *
 * The SENSOR_IO_* functions have no way to report an error to the driver (they return the byte
 * read, or nothing). They count failed transfers instead; the Sensors process compares the
 * count before and after a reading and marks the sample invalid if it changed.
 */
#ifndef SENSOR_IO_H
#define SENSOR_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of failed I2C2 transfers since reset. */
uint32_t sensor_io_error_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_IO_H */
