/*
 * reset_info.h -- why the board started, and how often the Uplink restarted it.
 *
 * The Uplink restarts the whole board (NVIC_SystemReset) when the Wi-Fi driver cannot be
 * initialised: with CMSIS-Driver 2.11.0 a retry in the same run would leak FreeRTOS heap and would
 * not reset the module (docs/05-code-walkthrough.md, 5.y). A counter in a .noinit RAM section
 * survives that reset, so repeated restarts are visible on the console.
 */
#ifndef RESET_INFO_H
#define RESET_INFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *cause;          /* "power-on/brown-out", "software reset", "reset pin", "watchdog", ... */
  uint32_t    wifiRestarts;   /* consecutive restarts by the Uplink (failed Wi-Fi starts) */
  int         wifiDisabled;   /* nonzero: wifiRestarts reached APP_WIFI_MAX_RESTARTS; no Uplink */
} BootInfo;

/* Call first thing in main() (USER CODE 1): reads and clears the reset flags. The counter is kept
   only if this start follows a restart by reset_info_restart_for_wifi(); any other reset (power-on,
   RESET button, debugger) sets it to 0, so pressing RESET always retries the Wi-Fi start. */
void reset_info_capture(void);

/* The values captured at start-up. */
BootInfo reset_info_get(void);

/* The Wi-Fi driver started: consecutive failures end, the counter goes back to 0. */
void reset_info_wifi_started(void);

/* Counts one more restart and resets the board. Does not return. */
void reset_info_restart_for_wifi(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* RESET_INFO_H */
