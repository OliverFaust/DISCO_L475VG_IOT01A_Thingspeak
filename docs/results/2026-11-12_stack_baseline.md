# Stack baseline before Wi-Fi (2026-11-12)

One stack report per run, printed by the Console 60 s after boot (`printStackReport()` in
`Core/Src/console_process.cpp`). For each process it gives the smallest amount of free stack the
thread has had since it started (FreeRTOS's stack high-water mark, read through CSP4CMSIS's
`CSProcess::stackHighWaterMarkWords()`, i.e. `osThreadGetStackSpace()`). Stack used at the worst moment
so far = size - minimum free. This is the baseline for phase 3, which adds the Wi-Fi Uplink process.

State measured: Sensors + Console, including the one-off HTS221 check (section 2 of
`2026-11-12_sensor_check.md`), 12 samples in the first 60 s.

| Process | Stack (words) | Debug (-O0): min free / max used | Release (-Os): min free / max used |
|---|---|---|---|
| Sensors | 512 (2 KB) | 323 / **189 words** (36 %) | 369 / **143 words** (27 %) |
| Console | 512 (2 KB) | 277 / **235 words** (45 %) | 295 / **217 words** (42 %) |

Runs:

```bash
tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf   75 docs/results/2026-11-12_sensor_check_and_stacks_console.txt
tools/flash_and_capture.sh Release/DISCO_L475VG_IOT01A_Thingspeak.elf 75 docs/results/2026-11-12_stacks_release_console.txt
```

Debug console (excerpt; Release prints the same lines with its own numbers):
```
[   60.000 s] Stack high-water marks (smallest free stack so far):
  Sensors   512 words, min free  323 words, max used  189 words (36 %)
  Console   512 words, min free  277 words, max used  235 words (45 %)
```

In both runs: 15 valid samples in 75 s, no stack overflow, and after the run
`__sbrk_heap_end = 0x00000000` and `xMinimumEverFreeBytesRemaining = 0x00000000` (no dynamic
allocation). Build sizes: Debug text 61112 / data 152 / bss 17008; Release 36964 / 152 / 16920.
This was the first run of the Release build on the board.

Notes:

- A high-water mark is a lower bound on what a thread *can* need: it covers only the paths the thread
  has taken so far. Here that includes both start-up paths (sensor initialisation, the HTS221 check,
  the banner) and the steady state. Not covered: the error path (`sensor read failed` line,
  I2C re-initialisation), which has not occurred.
- The Console's peak comes from `printf()` (newlib-nano's formatter) plus the calls into
  `format_fixed()` (`snprintf()`), not from CSP4CMSIS.
- Both processes keep more than half of their 2 KB free in either build; the sizes stay as they are
  for now. Phase 3 measures again, with the Uplink process and its HTTP buffers.
- Not measured: the MainApp thread (256 words; it exits after starting the network), CubeMX's
  `defaultTask` (exits at once), the FreeRTOS idle and timer threads, and the interrupt (main) stack.
