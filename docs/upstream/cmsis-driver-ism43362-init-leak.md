# Draft issue for ARM-software/CMSIS-Driver (not filed)

Checked 2026-11-12: `main` of github.com/ARM-software/CMSIS-Driver is at commit `8d01700`
(= tag 2.11.0); `WiFi/ISM43362/WiFi_ISM43362.c` there is identical to the copy in this project, and
both defects below are present. No open or closed issue or pull request mentions them (15 issues, 44
pull requests at the time; the ISM43362 ones are driver updates).

---

**Title:** WiFi/ISM43362: failed `WiFi_Initialize()` leaks its RTOS objects; `Initialize()` after
`Uninitialize()` does not reset the module

**Version:** CMSIS-Driver 2.11.0 (commit 8d0170006e3064aa57fdd02eeb9cf13ed69fa0f0),
`WiFi/ISM43362/WiFi_ISM43362.c` `$Revision: V1.16`. Found on a B-L475E-IOT01A (module firmware
`C3.5.2.7.STM`) with FreeRTOS 10.3.1 and ST's CMSIS-RTOS2 wrapper; the code paths are RTOS-independent.

### 1. A failed `WiFi_Initialize()` does not release what it created

`WiFi_Initialize()` creates two mutexes and `1 + WIFI_ISM43362_SOCKETS_NUM` event flag objects
(lines 1392-1402) before it talks to the module. If anything after that fails, it cleans up with:

```c
// WiFi_ISM43362.c, lines 1545-1546
  } else {                              // Else if initialization failed -> cleanup
    WiFi_Uninitialize();
```

but `WiFi_Uninitialize()` returns immediately, because `driver_initialized` has not been set yet:

```c
// lines 1563-1565
  if (driver_initialized == 0U) {       // If driver is already uninitialized
    return ARM_DRIVER_OK;
  }
```

The mutexes and event flags are therefore never deleted, and the next `WiFi_Initialize()` overwrites
their IDs. Every failed initialisation leaks these objects; with FreeRTOS they come from the FreeRTOS
heap (384 bytes per failure with 4 sockets, measured with `xPortGetMinimumEverFreeHeapSize()`). An
application that retries `Initialize()` eventually cannot create the objects any more and can then
never initialise the driver again without a reset.

**Reproduction:** make `WiFi_Initialize()` fail after the objects are created, e.g. keep the module in
reset by implementing `WiFi_ISM43362_Pin_RSTN()` to always drive RSTN active (low), so that DATARDY
never becomes active. Call `Initialize(NULL)` three times and print the free RTOS heap (FreeRTOS:
`xPortGetFreeHeapSize()`) after each call: it falls by the size of 2 mutexes + 5 event flag objects
per call, and does not recover.

### 2. `WiFi_Initialize()` after `WiFi_Uninitialize()` does not reset the module

`module_initialized` (line 309) guards the module reset and the initial "fetch cursor" sequence:

```c
// lines 1430-1459
      if (module_initialized == 0U) {
        // Reset WiFi Module
        WiFi_ISM43362_Pin_RSTN(true);
        ...
        module_initialized = 1U;
      }
```

It is set once and never cleared, neither by `WiFi_Uninitialize()` nor by `ResetVariables()`. An
application that uninitialises and re-initialises the driver to recover a module that stopped
responding (a natural recovery step) gets no hardware reset of the module on the second
`Initialize()`. On our board, `Initialize()` right after `Uninitialize()` (module previously failing
to join with a wrong password) returned an error; a further call 60 s later succeeded.

**Reproduction:** `Initialize()`, `Uninitialize()`, `Initialize()`, with a trace of the calls of
`WiFi_ISM43362_Pin_RSTN()`: it is called only during the first `Initialize()`.

### Suggested fix

```c
  } else {                              // Else if initialization failed -> cleanup
    driver_initialized = 1U;            // so that WiFi_Uninitialize() releases what was created
    WiFi_Uninitialize();
  }
```

(or move the release of the RTOS objects into a helper used by both functions), and in
`WiFi_Uninitialize()`, on success:

```c
    driver_initialized = 0U;
    module_initialized = 0U;            // next Initialize() resets the module again
```

`WiFi_Uninitialize()` with `driver_initialized` forced to 1 also closes the sockets and calls
`WiFi_ISM43362_Pin_Uninitialize()`; both are harmless after a failed initialisation, but a helper that
only deletes the RTOS objects would be the cleaner fix.

---

*For the lecturer:* how this project works around both defects without modifying the driver (no
retry of a failed `Initialize()`; a board restart instead, counted across resets):
`docs/05-code-walkthrough.md` ("When the Wi-Fi module does not start"), measurements in
`docs/results/2026-11-12_uplink_recovery.md` and `docs/results/2026-11-12_wifi_init_fail_restart_console.txt`.
