/*
 * cmsis_driver_spi3.c -- a CMSIS-Driver SPI interface (ARM_DRIVER_SPI Driver_SPI3) on SPI3 as
 * configured by STM32CubeMX (hspi3 in main.c: master, 16-bit frames, CPOL 0 / CPHA 0, MSB first,
 * software NSS, PC10/PC11/PC12).
 *
 * Only what Arm's ISM43362 Wi-Fi driver (Drivers/CMSIS-Driver/WiFi/ISM43362) uses is supported:
 * master mode with frame format CPOL0_CPHA0, 16 data bits, MSB first, slave select not used (the
 * Wi-Fi driver drives the module's chip select itself through WiFi_ISM43362_Pin_SSN()), bus speed,
 * default transmit value, abort; Send, Receive, Transfer.
 *
 * Transfers are blocking and polled (HAL_SPI_Transmit/Receive/TransmitReceive). No SPI interrupt
 * is used, so there is no interrupt priority to coordinate with FreeRTOS. Each operation returns
 * when the data has been moved, after calling the event callback with
 * ARM_SPI_EVENT_TRANSFER_COMPLETE -- from the calling thread, never from an interrupt. The Wi-Fi
 * driver waits for that event with osEventFlagsWait() after Send/Receive; here the flag is already
 * set when it starts waiting. At 10 Mbit/s a 32-byte block takes about 26 us of busy-waiting.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#include <string.h>
#include "Driver_SPI.h"
#include "main.h"

extern SPI_HandleTypeDef hspi3;          /* main.c, configured by MX_SPI3_Init() */

#define SPI3_DRV_VERSION   ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)
#define SPI3_TIMEOUT_MS    1000U         /* per Send/Receive call (largest transfers: ~2 KB, < 2 ms) */

static ARM_SPI_SignalEvent_t cb_event;
static uint16_t tx_default;              /* sent while receiving (ISM43362: 0x0A0A) */
static uint32_t data_count;              /* items moved by the last operation */
static uint32_t bus_speed;               /* bps actually set */

static const ARM_DRIVER_VERSION version = { ARM_SPI_API_VERSION, SPI3_DRV_VERSION };
static const ARM_SPI_CAPABILITIES capabilities = { 0U, 0U, 0U, 0U, 0U };

static ARM_DRIVER_VERSION GetVersion(void) { return version; }
static ARM_SPI_CAPABILITIES GetCapabilities(void) { return capabilities; }

/* SPI3 is clocked from PCLK1 (APB1). */
static uint32_t speed_for_prescaler(uint32_t presc_shift)        /* 0 -> /2 ... 7 -> /256 */
{
  return HAL_RCC_GetPCLK1Freq() >> (presc_shift + 1U);
}

/* Highest speed not above `bps`; returns 0 if even /256 is too fast. */
static int32_t set_bus_speed(uint32_t bps)
{
  static const uint32_t presc[8] = {
    SPI_BAUDRATEPRESCALER_2,  SPI_BAUDRATEPRESCALER_4,  SPI_BAUDRATEPRESCALER_8,  SPI_BAUDRATEPRESCALER_16,
    SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64, SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256 };
  for (uint32_t i = 0U; i < 8U; ++i) {
    if (speed_for_prescaler(i) <= bps) {
      if (hspi3.Init.BaudRatePrescaler != presc[i]) {
        hspi3.Init.BaudRatePrescaler = presc[i];
        if (HAL_SPI_Init(&hspi3) != HAL_OK) {
          return ARM_DRIVER_ERROR;
        }
      }
      bus_speed = speed_for_prescaler(i);
      return ARM_DRIVER_OK;
    }
  }
  return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t Initialize(ARM_SPI_SignalEvent_t cb)
{
  cb_event   = cb;
  tx_default = 0U;
  data_count = 0U;
  bus_speed  = speed_for_prescaler((hspi3.Init.BaudRatePrescaler >> SPI_CR1_BR_Pos) & 7U);
  return ARM_DRIVER_OK;
}

static int32_t Uninitialize(void)
{
  cb_event = NULL;
  return ARM_DRIVER_OK;
}

/* Clock, pins and peripheral are set up by CubeMX's MX_SPI3_Init() before the scheduler starts. */
static int32_t PowerControl(ARM_POWER_STATE state)
{
  return (state == ARM_POWER_LOW) ? ARM_DRIVER_ERROR_UNSUPPORTED : ARM_DRIVER_OK;
}

static int32_t finish(HAL_StatusTypeDef st, uint32_t num)
{
  if (st != HAL_OK) {
    data_count = 0U;
    return ARM_DRIVER_ERROR;      /* no event: the caller's wait times out and it aborts */
  }
  data_count = num;
  if (cb_event != NULL) {
    cb_event(ARM_SPI_EVENT_TRANSFER_COMPLETE);
  }
  return ARM_DRIVER_OK;
}

/* num = number of 16-bit items (the configured data size). */
static int32_t Send(const void *data, uint32_t num)
{
  if ((data == NULL) || (num == 0U) || (num > 0xFFFFU)) {
    return ARM_DRIVER_ERROR_PARAMETER;
  }
  return finish(HAL_SPI_Transmit(&hspi3, (uint8_t *)(uintptr_t)data, (uint16_t)num, SPI3_TIMEOUT_MS), num);
}

/* While receiving, the master must send something: the default transmit value. The buffer is
   filled with it and HAL_SPI_Receive() (2-line master) sends the buffer while overwriting it
   with the received data (HAL_SPI_TransmitReceive(hspi, pData, pData, ...)). */
static int32_t Receive(void *data, uint32_t num)
{
  if ((data == NULL) || (num == 0U) || (num > 0xFFFFU)) {
    return ARM_DRIVER_ERROR_PARAMETER;
  }
  uint16_t *p = (uint16_t *)data;
  for (uint32_t i = 0U; i < num; ++i) {
    p[i] = tx_default;
  }
  return finish(HAL_SPI_Receive(&hspi3, (uint8_t *)data, (uint16_t)num, SPI3_TIMEOUT_MS), num);
}

static int32_t Transfer(const void *data_out, void *data_in, uint32_t num)
{
  if ((data_out == NULL) || (data_in == NULL) || (num == 0U) || (num > 0xFFFFU)) {
    return ARM_DRIVER_ERROR_PARAMETER;
  }
  return finish(HAL_SPI_TransmitReceive(&hspi3, (uint8_t *)(uintptr_t)data_out, (uint8_t *)data_in,
                                        (uint16_t)num, SPI3_TIMEOUT_MS), num);
}

static uint32_t GetDataCount(void)
{
  return data_count;
}

static int32_t Control(uint32_t control, uint32_t arg)
{
  switch (control & ARM_SPI_CONTROL_Msk) {
    case ARM_SPI_MODE_MASTER:
      if ((control & ARM_SPI_FRAME_FORMAT_Msk) != ARM_SPI_CPOL0_CPHA0)        return ARM_SPI_ERROR_FRAME_FORMAT;
      if ((control & ARM_SPI_DATA_BITS_Msk)    != ARM_SPI_DATA_BITS(16))      return ARM_SPI_ERROR_DATA_BITS;
      if ((control & ARM_SPI_BIT_ORDER_Msk)    != ARM_SPI_MSB_LSB)            return ARM_SPI_ERROR_BIT_ORDER;
      if ((control & ARM_SPI_SS_MASTER_MODE_Msk) != ARM_SPI_SS_MASTER_UNUSED) return ARM_SPI_ERROR_SS_MODE;
      return set_bus_speed(arg);
    case ARM_SPI_SET_BUS_SPEED:
      return set_bus_speed(arg);
    case ARM_SPI_GET_BUS_SPEED:
      return (int32_t)bus_speed;
    case ARM_SPI_SET_DEFAULT_TX_VALUE:
      tx_default = (uint16_t)arg;
      return ARM_DRIVER_OK;
    case ARM_SPI_ABORT_TRANSFER:
      (void)HAL_SPI_Abort(&hspi3);
      data_count = 0U;
      return ARM_DRIVER_OK;
    default:
      return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
}

static ARM_SPI_STATUS GetStatus(void)
{
  ARM_SPI_STATUS s;
  memset(&s, 0, sizeof s);        /* transfers are blocking: never busy between calls */
  return s;
}

ARM_DRIVER_SPI Driver_SPI3 = {
  GetVersion, GetCapabilities, Initialize, Uninitialize, PowerControl,
  Send, Receive, Transfer, GetDataCount, Control, GetStatus
};
