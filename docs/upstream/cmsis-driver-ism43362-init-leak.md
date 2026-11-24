# Issue for ARM-software/CMSIS-Driver: ISM43362 initialisation defects

**Filed on 2026-11-13: <https://github.com/ARM-software/CMSIS-Driver/issues/60>** (date as GitHub
shows it). On 2026-11-15 it was open, with no comments and no labels.

The defects were found on 2026-11-12 in CMSIS-Driver 2.11.0 (commit `8d01700`, `main` at the time).
On that day no open or closed issue or pull request mentioned them. Below are the title and text as
filed, copied from GitHub on 2026-11-15. The draft before filing had a reproduction for each defect,
which the filed issue leaves out ("Steps To Reproduce": no response). The draft is in this file's git
history.

---

**Title:** [Bug]: WiFi/ISM43362: failed Initialize() leaks its RTOS objects; Initialize() after Uninitialize() does not reset the module

### Is there an existing issue for this?

- [x] I have searched the existing issues

### Problem statement

Two defects in the initialisation lifecycle of the ISM43362 WiFi driver.

**Version:** CMSIS-Driver 2.11.0 (commit 8d01700), `WiFi/ISM43362/WiFi_ISM43362.c` V1.16.
**Found on:** B-L475E-IOT01A, module firmware `C3.5.2.7.STM`, FreeRTOS 10.3.1 with ST's CMSIS-RTOS2 wrapper, GCC 14.3. The code paths are RTOS-independent.

### 1. A failed `WiFi_Initialize()` does not release the RTOS objects it created

`WiFi_Initialize()` creates two mutexes and `1 + WIFI_ISM43362_SOCKETS_NUM` event flag objects before it talks to the module:
https://github.com/ARM-software/CMSIS-Driver/blob/8d0170006e3064aa57fdd02eeb9cf13ed69fa0f0/WiFi/ISM43362/WiFi_ISM43362.c#L1391-L1402

If initialisation fails after that, it cleans up by calling `WiFi_Uninitialize()`:
https://github.com/ARM-software/CMSIS-Driver/blob/8d0170006e3064aa57fdd02eeb9cf13ed69fa0f0/WiFi/ISM43362/WiFi_ISM43362.c#L1543-L1547

but `WiFi_Uninitialize()` returns at once, because `driver_initialized` is only set on success:
https://github.com/ARM-software/CMSIS-Driver/blob/8d0170006e3064aa57fdd02eeb9cf13ed69fa0f0/WiFi/ISM43362/WiFi_ISM43362.c#L1563-L1565

So the objects are never deleted, and the next `Initialize()` overwrites their IDs. With FreeRTOS this costs 384 bytes of RTOS heap per failed call (4 sockets; measured with `xPortGetMinimumEverFreeHeapSize()`). An application that retries a failed `Initialize()` eventually exhausts the heap and can then never initialise the driver without a reset.

**Expected:** a failed `Initialize()` releases everything it created; the RTOS heap is the same before and after.

### 2. `Initialize()` after `Uninitialize()` does not reset the module

The hardware reset of the module (and the initial command sequence) is guarded by `module_initialized`:
https://github.com/ARM-software/CMSIS-Driver/blob/8d0170006e3064aa57fdd02eeb9cf13ed69fa0f0/WiFi/ISM43362/WiFi_ISM43362.c#L1430-L1459

It is set once and never cleared: not in `WiFi_Uninitialize()` and not in `ResetVariables()`. An application that uninitialises and re-initialises the driver to recover a module that stopped responding gets no module reset on the second `Initialize()`. On our board, `Initialize()` right after `Uninitialize()` (module previously failing to join) returned an error; a further call 60 s later succeeded.

**Expected:** every `Initialize()` after `Uninitialize()` resets the module, as the first one does.

### Suggested fix

1. Release the RTOS objects on the failure path of `WiFi_Initialize()`, e.g. with a helper that deletes the mutexes, the event flags and the thread (if created), used by both `WiFi_Initialize()` (on failure) and `WiFi_Uninitialize()`. Setting `driver_initialized = 1U` before the existing `WiFi_Uninitialize()` call would also work, but would then try to close sockets on a module that is not responding.
2. Clear `module_initialized` in `WiFi_Uninitialize()` on success, so the next `Initialize()` resets the module again.

Happy to test a fix on the B-L475E-IOT01A.

### Steps To Reproduce

_No response_

---

*For the lecturer:* how this project works around both defects without modifying the driver (no
retry of a failed `Initialize()`; a board restart instead, counted across resets):
`docs/05-code-walkthrough.md` ("When the Wi-Fi module does not start"), measurements in
`docs/results/2026-11-12_uplink_recovery.md` and `docs/results/2026-11-12_wifi_init_fail_restart_console.txt`.
