/*
 * console_io.c -- printf() retarget and fatal-error reporting on USART1 (ST-LINK VCP).
 *
 * USART1 itself is configured by STM32CubeMX (MX_USART1_UART_Init() in main.c,
 * 115200 8N1); this file only writes bytes to it.
 */
#include "console_io.h"
#include "main.h"            /* stm32l4xx_hal.h: USART1 register definitions */
#include "cmsis_os2.h"

/* One byte to USART1, by polling the "transmit data register empty" flag. */
static void uart_put(char c)
{
  while ((USART1->ISR & USART_ISR_TXE) == 0U) { }
  USART1->TDR = (uint8_t)c;
}

/* Wait until the last byte has left the shift register (before halting). */
static void uart_flush(void)
{
  while ((USART1->ISR & USART_ISR_TC) == 0U) { }
}

void console_raw_write(const char *s)
{
  while (*s != '\0') {
    uart_put(*s++);
  }
  uart_flush();
}

static void raw_write_uint(unsigned long v)
{
  char buf[11];
  int i = (int)sizeof buf;
  buf[--i] = '\0';
  do { buf[--i] = (char)('0' + (v % 10U)); v /= 10U; } while (v != 0U && i > 0);
  console_raw_write(&buf[i]);
}

/*
 * newlib's write system call; replaces the weak _write() in syscalls.c, which would
 * call __io_putchar() once per byte. stdout is unbuffered (setvbuf() in main.c), so
 * every printf() arrives here directly and nothing is allocated for a stdio buffer.
 * Polling costs about 87 us per byte at 115200 baud; the calling thread busy-waits for
 * that time (lower-priority threads do not run, higher-priority ones do).
 */
int _write(int file, char *ptr, int len)
{
  (void)file;
  for (int i = 0; i < len; ++i) {
    uart_put(ptr[i]);
  }
  return len;
}

/* --- Fatal errors: report on the console, then halt (a debugger can inspect the state). --- */

/* configASSERT() in FreeRTOSConfig.h (USER CODE 1) calls this. */
void vAssertCalled(const char *file, int line)
{
  __disable_irq();
  console_raw_write("\r\n*** configASSERT failed: ");
  console_raw_write(file);
  console_raw_write(":");
  raw_write_uint((unsigned long)line);
  console_raw_write("\r\n");
  for (;;) { }
}

/* Replaces CSP4CMSIS's weak default (lib/csp4cmsis/src/glue.cpp), which halts silently.
   May be called from an interrupt handler: only raw register writes here. */
void csp4cmsis_fatal_error(const char *message)
{
  __disable_irq();
  console_raw_write("\r\n*** CSP4CMSIS fatal error: ");
  console_raw_write(message);
  console_raw_write("\r\n");
  for (;;) { }
}

/* FreeRTOS calls this (from freertos.c, USER CODE 4) when a thread overran its stack. */
void app_stack_overflow(const char *task_name)
{
  __disable_irq();
  console_raw_write("\r\n*** Stack overflow in thread: ");
  console_raw_write(task_name);
  console_raw_write("\r\n");
  for (;;) { }
}
