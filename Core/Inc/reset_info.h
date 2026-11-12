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
  uint32_t    wifiRestarts;   /* restarts by the Uplink since the last power-on */
} BootInfo;

/* Call first thing in main() (USER CODE 1): reads and clears the reset flags, (re)initialises the
   counter after power-on or brown-out, when the .noinit RAM holds no valid data. */
void reset_info_capture(void);

/* The values captured at start-up. */
BootInfo reset_info_get(void);

/* Counts one more restart and resets the board. Does not return. */
void reset_info_restart_for_wifi(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* RESET_INFO_H */
