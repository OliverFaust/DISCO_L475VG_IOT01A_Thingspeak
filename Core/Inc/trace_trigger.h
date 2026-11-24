/*
 * trace_trigger.h -- stop the Debug-Trace build at a chosen moment, so that the trace ring holds
 * what happened before and after it (docs/06-tracing.md, 6.6).
 *
 * trace_trigger(post_ms): stores a deadline post_ms milliseconds ahead (HAL tick); once it has
 * passed, the FreeRTOS tick hook executes a breakpoint instruction (BKPT) -- but only if a debugger
 * is attached (CoreDebug->DHCSR C_DEBUGEN); without one it does nothing, so the board runs on.
 * tools/trace_dump.sh --wait waits for that halt and dumps the ring. The user button B1 (PC13,
 * EXTI line 13) calls trace_trigger(TRACE_TRIGGER_B1_POST_MS). A call while armed is ignored.
 *
 * Trace records (component 0x52, tools/trace_trigger.scvd), both written by the tick hook:
 *   0x5200 TriggerArmed  a ms until the deadline (at the first tick after the call), b 0
 *   0x5201 TriggerFired  a 1 if a debugger was attached (BKPT follows), 0 if not (nothing
 *                        happens); b ms after the deadline
 *
 * Debug-Trace configuration only: without CSP4CMSIS_TRACE (Debug, Release) the three calls are
 * macros that generate no code.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#ifndef TRACE_TRIGGER_H
#define TRACE_TRIGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_TRIGGER_B1_POST_MS   500U   /* B1: halt 500 ms after the press */
#define TRACE_TRIGGER_DEBOUNCE_MS  50U    /* after firing: B1 must be released this long before re-arming */

#define TRACE_TRIGGER_COMPONENT    0x52U
#define TRACE_TRIGGER_EVT_ARMED    0x00U
#define TRACE_TRIGGER_EVT_FIRED    0x01U

#if defined(CSP4CMSIS_TRACE)
/* Arm the trigger: BKPT at the first tick after post_ms. Any context (thread or interrupt): a
   single 32-bit store. A call while armed, or within 50 ms of B1's release after firing, is ignored. */
void trace_trigger(uint32_t post_ms);
/* B1 (PC13) as falling-edge EXTI input with its interrupt; called from MX_GPIO_Init. */
void trace_trigger_b1_init(void);
/* From the FreeRTOS tick hook: records, fires (BKPT if C_DEBUGEN) and re-arms the trigger. */
void trace_trigger_tick(void);
#else
/* Macros, not empty inline functions: Debug is built with -O0, where an inline function is still
   called. These generate no code, and the argument is not evaluated. */
#define trace_trigger(post_ms)    ((void)0)
#define trace_trigger_b1_init()   ((void)0)
#define trace_trigger_tick()      ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* TRACE_TRIGGER_H */
