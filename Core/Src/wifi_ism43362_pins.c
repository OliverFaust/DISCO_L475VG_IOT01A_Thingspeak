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

void WiFi_ISM43362_Pin_RSTN(uint8_t rstn)
{
#if APP_TEST_WIFI_INIT_FAIL
  (void)rstn;   /* test: keep the module in reset, so that the driver's Initialize() fails */
  HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(ISM43362_RST_GPIO_Port, ISM43362_RST_Pin, rstn ? GPIO_PIN_RESET : GPIO_PIN_SET);
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
