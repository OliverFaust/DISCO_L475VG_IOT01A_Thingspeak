/*
 * app_config.h -- build-time choices of the application.
 *
 * APP_DIAGNOSTIC_MODE selects what the MainApp thread starts:
 *   APP_DIAG_NONE          the application: the Sensors/Console process network (default)
 *   APP_DIAG_WIFI_BRINGUP  Wi-Fi bring-up test only: module firmware, MAC address, access point
 *                          scan, FreeRTOS heap used by the driver (Core/Src/wifi_bringup_test.cpp)
 * Change the value, rebuild, flash. Each diagnostic mode is a single process and the only one
 * that prints. docs/07-troubleshooting.md explains when to use which.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_DIAG_NONE           0
#define APP_DIAG_WIFI_BRINGUP   1

#ifndef APP_DIAGNOSTIC_MODE
#define APP_DIAGNOSTIC_MODE     APP_DIAG_NONE
#endif

#endif /* APP_CONFIG_H */
