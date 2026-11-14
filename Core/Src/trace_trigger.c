/*
 * trace_trigger.c -- breakpoint trigger for the Debug-Trace build (Core/Inc/trace_trigger.h).
 * Compiled to nothing unless CSP4CMSIS_TRACE is defined (Debug, Release).
 *
 * B1 is configured here and not in the .ioc: STM32CubeMX has one pin configuration for all build
 * configurations, so a B1 EXTI pin in the .ioc would change MX_GPIO_Init() and the interrupt table
 * of Debug and Release as well. The settings are those STM32CubeMX would generate for PC13 as
 * GPIO_EXTI13 (falling edge, pull-up) with the EXTI line[15:10] interrupt enabled
 * (docs/06-tracing.md, 6.6).
 *
 * One shared word, `state`, written by the arming side with a single 32-bit store:
 *   IDLE (0)     not armed: trace_trigger() and B1 may arm
 *   HOLD (1)     fired; waiting until B1 has been released for TRACE_TRIGGER_DEBOUNCE_MS
 *   other        armed: the HAL tick (HAL_GetTick()) at which the trigger fires
 * trace_trigger() and the B1 handler only store a deadline, and only if `state` is IDLE: a call
 * while armed (or a bounce of the same press) is ignored. Everything else runs in the FreeRTOS tick
 * hook (trace_trigger_tick()): it records TriggerArmed when it first sees a deadline, fires once the
 * deadline has passed (TriggerFired, then __BKPT only if C_DEBUGEN is set), and then holds the
 * trigger off until B1 has been high (released) for 50 ms, so that the bounces of the release cannot
 * arm it again. The tick hook and the B1 handler have the same priority (15) and do not preempt
 * each other.
 *
 * Licence: MIT (this file). Part of DISCO_L475VG_IOT01A_Thingspeak.
 */
#if defined(CSP4CMSIS_TRACE)

#include "trace_trigger.h"
#include "main.h"
#include "csp/csp_trace_ring.h"

#define B1_PIN        GPIO_PIN_13
#define B1_PORT       GPIOC

#define STATE_IDLE    0U
#define STATE_HOLD    1U

#define TRIGGER_ID(msg) EventID(EventLevelOp, TRACE_TRIGGER_COMPONENT, (msg))

static volatile uint32_t state = STATE_IDLE;   /* the only word written outside the tick hook */
static uint32_t seen;                          /* tick hook: the deadline it has recorded */
static uint32_t released_since;                /* tick hook: HAL tick since which B1 is high */

void trace_trigger(uint32_t post_ms)
{
  if (state == STATE_IDLE) {
    uint32_t deadline = HAL_GetTick() + post_ms;
    if (deadline <= STATE_HOLD) {
      deadline = STATE_HOLD + 1U;                /* 0 and 1 are states, not deadlines */
    }
    state = deadline;                            /* one 32-bit store */
  }
}

void trace_trigger_tick(void)
{
  const uint32_t s = state;
  const uint32_t now = HAL_GetTick();
  if (s == STATE_IDLE) {
    return;
  }
  if (s == STATE_HOLD) {
    if (HAL_GPIO_ReadPin(B1_PORT, B1_PIN) == GPIO_PIN_RESET) {
      released_since = now;                      /* still pressed, or bouncing */
    } else if (now - released_since >= TRACE_TRIGGER_DEBOUNCE_MS) {
      state = STATE_IDLE;
    }
    return;
  }
  if (s != seen) {
    seen = s;
    EventRecord2(TRIGGER_ID(TRACE_TRIGGER_EVT_ARMED), s - now, 0U);
  }
  if ((int32_t)(now - s) >= 0) {
    released_since = now;
    state = STATE_HOLD;
    seen = 0U;
    const uint32_t debugger = (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) ? 1U : 0U;
    EventRecord2(TRIGGER_ID(TRACE_TRIGGER_EVT_FIRED), debugger, now - s);
    if (debugger) {
      __BKPT(0);                                 /* the debugger halts here; trace_dump.sh steps past it */
    }
  }
}

void trace_trigger_b1_init(void)
{
  GPIO_InitTypeDef init = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  init.Pin = B1_PIN;
  init.Mode = GPIO_MODE_IT_FALLING;
  init.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B1_PORT, &init);
  /* Priority 15, as the FreeRTOS tick (SysTick): the handler and the tick hook never preempt each other. */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* Replaces the startup file's weak default handler; no other pin uses EXTI lines 10-15. */
void EXTI15_10_IRQHandler(void)
{
  if (__HAL_GPIO_EXTI_GET_IT(B1_PIN) != 0U) {
    __HAL_GPIO_EXTI_CLEAR_IT(B1_PIN);
    trace_trigger(TRACE_TRIGGER_B1_POST_MS);    /* stores the deadline, nothing else */
  }
}

#endif /* CSP4CMSIS_TRACE */
