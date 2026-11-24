/*
 * csp_trace_device.h -- the device header for CSP4CMSIS in the Debug-Trace configuration
 * (CSP4CMSIS_DEVICE_HEADER="csp_trace_device.h"; Debug and Release use "stm32l4xx.h" directly).
 *
 * The trace ring backend of CSP4CMSIS's dev/tracing branch (lib/csp4cmsis-trace/csp4cmsis/src/
 * csp_trace_ring.c) enables the cycle counter through DCB->DEMCR, the CMSIS 6 name of the Debug
 * Control Block. STM32Cube FW_L4 V1.18.2 ships CMSIS-Core 5.3, where the same register is
 * CoreDebug->DEMCR. The two names below map one to the other; the library stays unmodified.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#ifndef CSP_TRACE_DEVICE_H
#define CSP_TRACE_DEVICE_H

#include "stm32l4xx.h"

#ifndef DCB
#define DCB                   CoreDebug
#define DCB_DEMCR_TRCENA_Msk  CoreDebug_DEMCR_TRCENA_Msk
#endif

#endif /* CSP_TRACE_DEVICE_H */
