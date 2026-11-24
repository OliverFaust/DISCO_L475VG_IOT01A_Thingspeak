/*
 * wifi_ism43362_pins.c -- the pin functions Arm's ISM43362 driver needs
 * (Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362_HW.h), on the pins STM32CubeMX configures
 * (MX_GPIO_Init() in main.c, labels in main.h):
 *
 *   PE8  ISM43362_RST        reset, active low       output, starts low (module held in reset)
 *   PE0  ISM43362_SPI3_CSN   chip select, active low output, starts high (not selected)
 *   PE1  ISM43362_DRDY_EXTI1 data ready, active high input (EXTI1 rising edge configured, its
 *                                                     interrupt not enabled in the NVIC)
 *   PB12 ISM43362_BOOT0      low: boot the module's firmware (left as CubeMX sets it)
 *   PB13 ISM43362_WAKEUP     low (left as CubeMX sets it)
 *
 * The driver's convention is "1 = active state", so RSTN(1) and SSN(1) drive their lines LOW.
 *
 * DATARDY is polled: WiFi_ISM43362_Pin_DATARDY_IRQ() is not called, so the driver finds no
 * data-ready event during WiFi_Initialize() and checks the pin every 1 ms instead (see
 * SPI_WaitReady() in WiFi_ISM43362.c). With ST's CMSIS-RTOS2 wrapper an interrupt-driven DATARDY
 * would set the driver's event flags with xEventGroupSetBitsFromISR(), which defers the work to the
 * FreeRTOS timer task (priority 2 in this project, below every other thread) and needs further
 * FreeRTOS options -- not worth it for a module that is queried every few seconds.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#include "WiFi_ISM43362_HW.h"
#include "main.h"
#include "app_config.h"

/* The pins are configured by MX_GPIO_Init() before the scheduler starts. */
void WiFi_ISM43362_Pin_Initialize(void)
{
}

void WiFi_ISM43362_Pin_Uninitialize(void)
{
}

#if APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_DRIVER_TEST
/* Driver test (Core/Src/wifi_driver_test.cpp): count the driver's calls, and the calls after which
   PE8's output register really holds the requested level. wifi_test_hold_reset = 1 keeps PE8 low
   (module in reset) whatever the driver requests; clearing it leaves PE8 as it is, so only the
   driver's next RSTN(0) releases the module. */
volatile uint32_t wifi_test_rstn_assert;      /* RSTN(1): PE8 low  */
volatile uint32_t wifi_test_rstn_release;     /* RSTN(0): PE8 high */
volatile uint32_t wifi_test_rstn_driven;      /* calls after which ODR matched the request */
volatile uint8_t  wifi_test_hold_reset;
#include "WiFi_ISM43362_Config.h"
#ifndef WIFI_ISM43362_HW_RESET
#define WIFI_ISM43362_HW_RESET 0                /* the driver's default */
#endif
const int wifi_test_hw_reset = WIFI_ISM43362_HW_RESET;
#endif

void WiFi_ISM43362_Pin_RSTN(uint8_t rstn)
{
#if APP_TEST_WIFI_INIT_FAIL
  (void)rstn;   /* test: keep the module in reset, so that the driver's Initialize() fails */
  HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, GPIO_PIN_RESET);
#elif APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_DRIVER_TEST
  const GPIO_PinState level = (rstn || wifi_test_hold_reset) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, level);
#else
  HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, rstn ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
#if APP_DIAGNOSTIC_MODE == APP_DIAG_WIFI_DRIVER_TEST
  if (rstn) { ++wifi_test_rstn_assert; } else { ++wifi_test_rstn_release; }
  const uint8_t low = ((ISM43362_RST_GPIO_Port->ODR & ISM43362_RST_Pin) == 0U) ? 1U : 0U;
  if (low == (rstn ? 1U : 0U)) { ++wifi_test_rstn_driven; }
#endif
}

void WiFi_ISM43362_Pin_SSN(uint8_t ssn)
{
  HAL_GPIO_WritePin(ISM43362_SPI3_CSN_GPIO_Port, ISM43362_SPI3_CSN_Pin, ssn ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

uint8_t WiFi_ISM43362_Pin_DATARDY(void)
{
  return (HAL_GPIO_ReadPin(ISM43362_DRDY_EXTI1_GPIO_Port, ISM43362_DRDY_EXTI1_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}
