/*
 * RTE_Components.h -- minimal stand-in for the file that CMSIS pack builds generate.
 *
 * Arm's ISM43362 Wi-Fi driver (Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c) includes
 * RTE_Components.h and checks:
 *   RTE_CMSIS_RTOS2_FreeRTOS  -> the CMSIS-RTOS2 implementation is FreeRTOS: include FreeRTOS.h and
 *                                give the driver's thread a static control block (StaticTask_t),
 *                                so that creating it takes nothing from the FreeRTOS heap.
 *   RTE_Compiler_EventRecorder / RTE_CMSIS_View_EventRecorder -> not defined: no Event Recorder.
 *
 * This folder (Core/Config) is on the include path of the C compiler only. CSP4CMSIS (compiled by
 * G++) also looks for an RTE_Components.h; it does not see this one and keeps detecting FreeRTOS
 * and the device header as before (CSP4CMSIS_DEVICE_HEADER).
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H

#define RTE_CMSIS_RTOS2_FreeRTOS

#endif /* RTE_COMPONENTS_H */
