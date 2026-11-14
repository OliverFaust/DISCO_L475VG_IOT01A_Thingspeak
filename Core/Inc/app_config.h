/*
 * app_config.h -- build-time choices of the application.
 *
 * APP_DIAGNOSTIC_MODE selects what the MainApp thread starts:
 *   APP_DIAG_NONE          the application: Sensors, Console and Uplink (default)
 *   APP_DIAG_WIFI_BRINGUP  Wi-Fi bring-up test only: module firmware, MAC address, access point
 *                          scan, FreeRTOS heap used by the driver (Core/Src/wifi_bringup_test.cpp)
 *   APP_DIAG_WIFI_ONE_UPLOAD  one sample, one ThingSpeak update, entry id; needs
 *                          Core/Inc/app_secrets.h (Core/Src/wifi_upload_test.cpp)
 * Change the value, rebuild, flash. Each diagnostic mode is a single process and the only one
 * that prints. docs/07-troubleshooting.md explains when to use which.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_DIAG_NONE           0
#define APP_DIAG_WIFI_BRINGUP   1
#define APP_DIAG_WIFI_ONE_UPLOAD 2

#ifndef APP_DIAGNOSTIC_MODE
#define APP_DIAGNOSTIC_MODE     APP_DIAG_NONE
#endif

/* ThingSpeak upload period in seconds (Uplink process). ThingSpeak rejects updates of a channel
   less than 15 s apart (free accounts); smaller values do not compile. The period is measured from
   the end of one upload attempt to the start of the next, so two requests are never closer than
   this, even when one of them was slow. */
#ifndef APP_UPLOAD_PERIOD_S
#define APP_UPLOAD_PERIOD_S     60
#endif

/* Test switch for the recovery tests (docs/results): 1 = the Uplink joins with a deliberately wrong
   Wi-Fi password instead of APP_WIFI_PASSWORD. Must be 0 in normal use. */
#ifndef APP_TEST_WRONG_PASSWORD
#define APP_TEST_WRONG_PASSWORD 0
#endif

/* After this many consecutive restarts because the Wi-Fi driver could not be initialised, the board
   stops restarting and runs Sensors and Console without the Uplink (docs/07, 7.1). A power-on, the
   RESET button, a debugger reset or a successful Wi-Fi start sets the count back to 0. */
#ifndef APP_WIFI_MAX_RESTARTS
#define APP_WIFI_MAX_RESTARTS   3
#endif

/* Test switch for the start-up failure test (docs/results): 1 = the Wi-Fi module is held in reset
   (Core/Src/wifi_ism43362_pins.c), so the driver's Initialize() fails and the Uplink restarts the
   board every few seconds. Must be 0 in normal use. */
#ifndef APP_TEST_WIFI_INIT_FAIL
#define APP_TEST_WIFI_INIT_FAIL 0
#endif

#endif /* APP_CONFIG_H */
