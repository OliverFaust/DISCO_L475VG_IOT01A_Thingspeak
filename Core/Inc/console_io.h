/*
 * console_io.h -- output to the ST-LINK virtual COM port (USART1, PB6/PB7, 115200 8N1).
 *
 * Two paths:
 *  - printf() (newlib) ends in _write() below, which writes to USART1 by polling.
 *    In the running application only the Console process calls printf().
 *  - console_raw_write() writes a string straight to the USART1 registers: no stdio,
 *    no HAL, no RTOS. It works with interrupts disabled and from an interrupt handler,
 *    and is used only for fatal errors (failed configASSERT, CSP4CMSIS fatal error,
 *    stack overflow), after which the program halts.
 */
#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

void console_raw_write(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_IO_H */
