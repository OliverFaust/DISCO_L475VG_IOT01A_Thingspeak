/*
 * sensor_io.c -- SENSOR_IO_* (declared in Drivers/BSP/Components/hts221/hts221.h and
 * lps22hb/lps22hb.h) on the CubeMX-generated I2C2 handle hi2c2. Replaces the same
 * functions in ST's stm32l475e_iot01.c, which is not part of this project (it would
 * initialise I2C2 a second time, with its own handle). See Drivers/BSP/README.md.
 *
 * Called only from the Sensors process (the sole owner of I2C2).
 */
#include "sensor_io.h"
#include "stm32l475e_iot01_hsensor.h"   /* -> hts221.h: SENSOR_IO_* prototypes */
#include "stm32l475e_iot01_psensor.h"   /* -> lps22hb.h */

extern I2C_HandleTypeDef hi2c2;          /* main.c, configured by MX_I2C2_Init() */

#define SENSOR_IO_TIMEOUT_MS  10U        /* one register transfer takes well under 1 ms at 100 kHz */

static volatile uint32_t error_count;

uint32_t sensor_io_error_count(void)
{
  return error_count;
}

/* After a failed transfer: count it and re-initialise the peripheral, as ST's BSP does
   (I2Cx_Error() in stm32l475e_iot01.c). hi2c2.Init still holds CubeMX's settings, and
   HAL_I2C_MspDeInit/MspInit (stm32l4xx_hal_msp.c) release and reconfigure the pins. */
static void on_error(void)
{
  error_count++;
  (void)HAL_I2C_DeInit(&hi2c2);
  (void)HAL_I2C_Init(&hi2c2);
}

/* I2C2 is already initialised by MX_I2C2_Init() before the scheduler starts. */
void SENSOR_IO_Init(void)
{
}

void SENSOR_IO_DeInit(void)
{
}

void SENSOR_IO_Write(uint8_t Addr, uint8_t Reg, uint8_t Value)
{
  if (HAL_I2C_Mem_Write(&hi2c2, Addr, Reg, I2C_MEMADD_SIZE_8BIT, &Value, 1, SENSOR_IO_TIMEOUT_MS) != HAL_OK) {
    on_error();
  }
}

uint8_t SENSOR_IO_Read(uint8_t Addr, uint8_t Reg)
{
  uint8_t value = 0;
  if (HAL_I2C_Mem_Read(&hi2c2, Addr, Reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SENSOR_IO_TIMEOUT_MS) != HAL_OK) {
    on_error();
  }
  return value;
}

uint16_t SENSOR_IO_ReadMultiple(uint8_t Addr, uint8_t Reg, uint8_t *Buffer, uint16_t Length)
{
  if (HAL_I2C_Mem_Read(&hi2c2, Addr, Reg, I2C_MEMADD_SIZE_8BIT, Buffer, Length, SENSOR_IO_TIMEOUT_MS) != HAL_OK) {
    on_error();
    return 1U;
  }
  return 0U;
}

void SENSOR_IO_WriteMultiple(uint8_t Addr, uint8_t Reg, uint8_t *Buffer, uint16_t Length)
{
  if (HAL_I2C_Mem_Write(&hi2c2, Addr, Reg, I2C_MEMADD_SIZE_8BIT, Buffer, Length, SENSOR_IO_TIMEOUT_MS) != HAL_OK) {
    on_error();
  }
}

HAL_StatusTypeDef SENSOR_IO_IsDeviceReady(uint16_t DevAddress, uint32_t Trials)
{
  return HAL_I2C_IsDeviceReady(&hi2c2, DevAddress, Trials, SENSOR_IO_TIMEOUT_MS);
}

void SENSOR_IO_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}
