# Trace dump control: breakpoint mode, B1 trigger, wait-for-halt (2026-11-14)

What was added (docs/06-tracing.md, 6.6), and the tests on the lab board (B-L475E-IOT01A, ST-LINK
V2J46M31), on top of commit `848b487` of `csp4cmsis-port`:

- `tools/trace_dump.sh`: modes `--break LOC [--if COND] [--next] [--reset]` and `--wait [--press]`,
  `--timeout S`; the plain mode is unchanged. A condition GDB cannot evaluate is reported as an
  error (exit 1) instead of a dump.
- `Core/Src/trace_trigger.c`, `Core/Inc/trace_trigger.h` (Debug-Trace only): `trace_trigger(post_ms)`
  only stores a deadline (one 32-bit store; ignored while armed). The FreeRTOS tick hook records
  `TriggerArmed remaining_ms` at its first tick after the call, and once the deadline has passed
  `TriggerFired debugger late_ms`, then `__BKPT` only if `C_DEBUGEN` is set; afterwards it holds
  the trigger off until B1 has been released for 50 ms. B1 (PC13, EXTI line 13, falling edge,
  priority 15 like the tick) calls `trace_trigger(500)` and nothing else. Component 0x52.
- `Core/Src/main.c`, USER CODE SysInit (Debug-Trace only): `__HAL_DBGMCU_FREEZE_TIM6()`.
- `tools/trace_trigger.scvd` (the two events, for `decode.py`), `tools/trace_marks.py` (a "trigger"
  group in `trace.json`), both used by `tools/trace_decode.sh`.

## Builds: Debug and Release unchanged

Headless builds of all three configurations, `Build Finished. 0 errors, 0 warnings.` each; the log
shows `main.c` and `trace_trigger.c` compiled in every configuration. Flash images
(`arm-none-eabi-objcopy -O binary`, first 16 hex digits of SHA-256):

| Configuration | Before (`848b487`) | After | |
|---|---|---|---|
| Debug | `c23e71dbd09cbd1f` | `c23e71dbd09cbd1f` | identical |
| Release | `2afbffa9b5dff454` | `2afbffa9b5dff454` | identical |
| Debug-Trace | `8697fe9e54851aa4` | `58357c02edd2c064` | text 113 416 -> 113 864 B, data + bss 64 960 -> 64 968 B |

In Debug the symbol `EXTI15_10_IRQHandler` is still the startup file's weak default (`W`), and no
`trace_trigger*` symbol exists. The stubs are macros: with `-O0` (Debug), empty `static inline`
functions would still be called.

Not changed: the `.ioc` (B1 there would change `MX_GPIO_Init()` of every configuration; 6.6 gives the
STM32CubeMX settings for projects where that is wanted), CSP4CMSIS, ST's and Arm's files.

## Tests on the board

All with `tools/trace_dump.sh`, decoded with `tools/trace_decode.sh` (`decode.py --no-eventlist`,
no ProB). Console captures masked (`tools/mask_log.py`). Every test ran on the final Debug-Trace
image `58357c02edd2c064` (flash read back from the board and compared with `cmp`).

| Test | Command | Result | Files |
|---|---|---|---|
| plain mode | `trace_dump.sh` | as before: halted, 16416 bytes, 5 names, board runs on (1.7 s in all) | -- |
| timeout | `--break Error_Handler --timeout 5` | `Program received signal SIGINT` after 5 s, `== not stopped by the breakpoint or trigger (timeout?): no dump`, breakpoint deleted, detached, exit 1; console went on | -- |
| example 1: upload finished | `--break uplink_process.cpp:222` | `Breakpoint 1, UplinkProcess::run ... uplink_process.cpp:222`, `halted at pc = 0x8007cec, write_count = 84528`; window 30.5 s, ends 325 ms after the Uplink's upload timeout (629 records of the upload); ring slots and ALT offers PASS, ALT deadlines FAIL 2 (window start, 6.4); `orphan continuation records 1` (the window starts inside a two-record event). The board's `-> entry` line and the next upload followed | `break_upload/` |
| example 2: `OverwroteOldest` | `--break buffered_channel.h:200 --if "this == (void *)&'(anonymous namespace)::MainApp_Task(void*)::uplinkSamples'" --next --reset` | 3 locations; `Breakpoint 1.3, ...offer (this=0x200005f4 <...::uplinkSamples>, ...)`; `next` to line 206; `halted at pc = 0x800332e, write_count = 16665`; window 0.256 s ending at tick 5041 with Sensors `ResumeTimeout`, `BufferedPut` on `samples`, `OverwroteOldest` on `uplinkSamples` | `break_overwrite/` |
| condition on a counter, exercise 9.3 Q5 | `--break uplink_process.cpp:222 --if "uploadsOk_ == 2" --reset --timeout 240`, started 20 s after an `-> entry` line | `Breakpoint 1, UplinkProcess::run ...`; console: `-> entry` at 11.118 s (`ok 1`) and 71.465 s (`ok 2`), the third (`ok 3`, 131.803 s) printed only after the dump. A first attempt, started a few seconds after an upload (`--timeout 180`), timed out: the first upload after the reset came within 15 s of the previous one, `[    7.262 s] Uplink: sample #2 REJECTED by ThingSpeak (HTTP 200, entry 0)` (rate limit), so the third success came after 180 s. Fixed afterwards in the Uplink: the first upload now waits until 15 s after boot (`2026-11-14_first_upload_after_boot.md`) | `break_counter/` |
| condition on the thread | `--break csp::Alternative::priSelect --if "pxCurrentTCB->pcTaskName[0] == 'U'"` | `Breakpoint 1, csp::Alternative::priSelect (this=0x200029ac <...::uplink+3604>)`: the Uplink's ALT, although Sensors and Console run the same function; the ring ends with the Uplink's `BufferedGet` on `uplinkSamples`, `AltSelected guard=0` | `break_thread/` |
| condition GDB cannot evaluate | same, `--if '$_streq(pxCurrentTCB->pcTaskName, "Uplink")'` | before the fix: `Error in testing condition for breakpoint 1: Invalid data type for function to be called.`, then a stop in the **Sensors** ALT, dumped as a hit (this GDB has no Python, `$_streq` is a Python function). After the fix: `== the condition could not be evaluated ...`, exit 1 | -- |
| cost of a condition | `halt_timing/cond.gdb`: `break tasks.c:3432 if ($n = $n + 1) < 0` (idle loop, always false), interrupted after about 10.5 s | `passes 178, kernel ticks advanced 0`: about 60 ms per pass, the program did not advance | `halt_timing/cond_output.txt` |
| time while halted | `halt_timing/tick.gdb`: halt 3 s, with `DBG_TIM6_STOP` as built and cleared | `APB1FZR1 = 0x00000010`; frozen: `uwTick +0, xTickCount +0, DWT_CYCCNT +0, TIM6_CNT 489 -> 489`; not frozen: the same +0, `TIM6_CNT 353 -> 995` | `halt_timing/tick_output.txt` |
| B1, press inside an upload | `--wait --press`, started 59.25 s after an `-> entry` line | see below | `b1_press/` |
| re-arm while armed | `rearm_ignored/rearm.gdb`: `call trace_trigger(2000)`, software press, continue | `state - uwTick = 2000`; `SIGTRAP` in `trace_trigger_tick () at ../Core/Src/trace_trigger.c:80`; ring: one `TriggerArmed remaining_ms=2000`, `TriggerFired debugger=1, late_ms=0` 2000.0 ms later (the press's 500 ms were ignored) | `rearm_ignored/` |
| no debugger | `STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -w32 0x40010410 0x2000` (software press; the programmer disconnects at once), dump 16 s later | `TriggerArmed remaining_ms=500`, 500.0 ms later `TriggerFired debugger=0, late_ms=0`; console without a gap. The programmer reports `Error: Failed to download data!` because `EXTI_SWIER1` reads back 0; the write takes effect | `no_debugger/` |

After all of these (eight halts and dumps, including the 10.5 s stall), the program kept uploading
every 60 s without a failure: entries 236329 to 236337 (`b1_press/console.txt`).

### B1 pressed inside an upload (`b1_press/`)

Host times relative to the `-> entry` line before (`dump.txt`): `== B1 pressed in software` at
59.767 s, `SIGTRAP` at 60.325 s (`trace_trigger_tick () at ../Core/Src/trace_trigger.c:80`), dump
written at 61.508 s. The upload completed: `[  127.885 s] Uplink: sample #26 -> entry 236331 (262 ms)`.

Decoded (`events.csv`), relative to `TriggerArmed`:

| Time | Thread | Event |
|---|---|---|
| -127.976 ms | Uplink | `ResumeTimeout` (tick 127623 = deadline), `AltSelected guard=1`: the upload starts |
| 0 | (tick hook) | `TriggerArmed remaining_ms=500` |
| +134.651 ms | Uplink | `BufferedPut` on `status` |
| +134.692 / +134.734 ms | Console | `Resume` (tick 127885), `BufferedGet` from `status` |
| +143.217 / +143.375 ms | Console, Uplink | `WaitAlt` |
| +499.997 ms | (tick hook) | `TriggerFired debugger=1, late_ms=0` |

Thread switch-ins from the press to the status put (135 ms): Wi-Fi driver thread 51, idle 53,
Uplink 12; from there to the halt (365 ms): 9. The window is 48.2 s (780 events, the ring's 1024
records). `trace.json` has the group "trigger" (`tools/trace_marks.py`): instants *armed (B1 /
trace_trigger())* and *halt (BKPT)*, slice *post-trigger 500 ms* with `measured_ms` 500.0, and the
`status` flow from the Uplink's put to the Console's get inside it. Checks: ring slots PASS, ALT
offers PASS, ALT deadlines FAIL 3 (window start, docs/06, 6.4); the trigger records add none.

The GDB connection halts the core for a moment before the software press (the press is written
while halted and takes effect when GDB continues); between press and halt the core was not halted.

## Not covered

- **A press of the real button B1.** Every test used the software press (`EXTI_SWIER1`), which runs
  the same interrupt handler but not the pin, its pull-up, or the bounces of a real press and
  release (the hold-off after firing reads the pin; with no press, the pin was high). To do by hand:
  `tools/trace_dump.sh --wait`, press B1 shortly before an expected upload, decode, look at the
  trigger track in Perfetto; then press and hold B1 for a second and check that one press gives one
  halt.
- A Perfetto screenshot of the trigger track (no browser automation here).
- STM32CubeIDE's own debug session with the trigger (6.6, *by hand*): not run; whether STM32CubeIDE
  clears `C_DEBUGEN` when its session ends: not checked.
- A torn record from an external halt: possible (6.6), not produced in any dump so far.
