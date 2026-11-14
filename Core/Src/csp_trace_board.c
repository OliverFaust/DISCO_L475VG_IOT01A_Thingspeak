/*
 * csp_trace_board.c -- the board's part of CSP4CMSIS's RAM ring trace backend
 * (lib/csp4cmsis-trace/csp4cmsis/inc/csp/csp_trace_ring.h), Debug-Trace configuration only.
 * Compiled to nothing unless CSP4CMSIS_TRACE is defined (Debug, Release).
 *
 * Adapted from the STM32N6570-DK port's CSP/csp_trace_board.c, without its MPU set-up: the
 * Cortex-M4 of the STM32L475 has no data cache, so the debugger reads what the core wrote.
 *  - The ring, in section .csp_trace: NOLOAD, in SRAM2 (csp_trace.ld). The debugger's load does
 *    not write it; csp_trace_ring_init() initialises it.
 *  - csp_trace_ring_init() from a constructor that runs before every other one (priority 101),
 *    i.e. before main() and long before the first event (the first thread creation).
 *  - csp_trace_ring_tick() from the FreeRTOS tick hook: heartbeat once per second, and a record
 *    when SystemCoreClock changes (it does once: from 4 MHz MSI to 80 MHz in SystemClock_Config()).
 * The thread hooks are in Core/Inc/FreeRTOSConfig.h (USER CODE Defines), likewise only with
 * CSP4CMSIS_TRACE.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#if defined(CSP4CMSIS_TRACE)

#include "FreeRTOS.h"
#include "csp/csp_trace_ring.h"

struct csp_trace_ring csp_trace_ring __attribute__((section(".csp_trace"), aligned(32)));

/* Before every other static constructor, so before the first event. */
__attribute__((constructor(101))) static void csp_trace_board_init(void)
{
  csp_trace_ring_init();
}

void vApplicationTickHook(void)
{
  csp_trace_ring_tick(configTICK_RATE_HZ);
}

#endif /* CSP4CMSIS_TRACE */
