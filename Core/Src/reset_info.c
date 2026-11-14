/*
 * reset_info.c -- see reset_info.h.
 *
 * The counter lives in the .noinit section (STM32L475VGTX_FLASH.ld): the start-up code neither
 * copies nor zeroes it, so it keeps its value across NVIC_SystemReset(). After power-on its content
 * is random; a magic word tells valid from random. The counter survives only a restart made by
 * reset_info_restart_for_wifi() (marker `pending`, software-reset flag): every other kind of reset
 * clears it.
 */
#include "reset_info.h"
#include "main.h"          /* stm32l4xx_hal.h: RCC registers, NVIC_SystemReset() */

#include "app_config.h"            /* APP_WIFI_MAX_RESTARTS */

#define RESET_INFO_MAGIC   0x52535443UL /* "RSTC": the block below is valid */
#define RESET_INFO_PENDING 0x57494649UL /* "WIFI": the last reset was reset_info_restart_for_wifi() */

typedef struct {
  uint32_t magic;
  uint32_t wifiRestarts;              /* consecutive restarts after a failed Wi-Fi start */
  uint32_t pending;                   /* RESET_INFO_PENDING right before such a restart */
} NoInitData;

static NoInitData noinit __attribute__((section(".noinit")));
static uint32_t   boot_csr;            /* RCC->CSR as found at start-up */

void reset_info_capture(void)
{
  boot_csr = RCC->CSR;
  RCC->CSR |= RCC_CSR_RMVF;            /* clear the flags for the next start */
  const int ours = noinit.magic == RESET_INFO_MAGIC && noinit.pending == RESET_INFO_PENDING &&
                   (boot_csr & RCC_CSR_SFTRSTF) != 0U && (boot_csr & RCC_CSR_BORRSTF) == 0U;
  if (!ours) {                         /* power-on, brown-out, button, debugger, watchdog, ... */
    noinit.magic = RESET_INFO_MAGIC;
    noinit.wifiRestarts = 0U;
  }
  noinit.pending = 0U;
}

void reset_info_wifi_started(void)
{
  noinit.wifiRestarts = 0U;
}

BootInfo reset_info_get(void)
{
  BootInfo b;
  /* Several flags can be set at once (an internal reset also pulls the NRST pin): most specific first. */
  if      (boot_csr & RCC_CSR_BORRSTF)                       b.cause = "power-on/brown-out";
  else if (boot_csr & RCC_CSR_SFTRSTF)                       b.cause = "software reset";
  else if (boot_csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) b.cause = "watchdog";
  else if (boot_csr & RCC_CSR_LPWRRSTF)                      b.cause = "low-power reset";
  else if (boot_csr & (RCC_CSR_OBLRSTF | RCC_CSR_FWRSTF))    b.cause = "option bytes/firewall";
  else if (boot_csr & RCC_CSR_PINRSTF)                       b.cause = "reset pin (button or debugger)";
  else                                                       b.cause = "unknown";
  b.wifiRestarts = noinit.wifiRestarts;
  b.wifiDisabled = noinit.wifiRestarts >= APP_WIFI_MAX_RESTARTS;
  return b;
}

void reset_info_restart_for_wifi(void)
{
  noinit.wifiRestarts++;
  noinit.pending = RESET_INFO_PENDING;
  __DSB();
  NVIC_SystemReset();
  for (;;) { }
}
