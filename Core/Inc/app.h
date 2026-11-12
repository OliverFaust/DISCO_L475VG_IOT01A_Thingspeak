/*
 * app.h -- entry point of the CSP application, called from main.c (C).
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the static MainApp thread, which builds and starts the process network.
   Call after osKernelInitialize() and before osKernelStart() (main.c, USER CODE RTOS_THREADS). */
void csp_app_main_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
