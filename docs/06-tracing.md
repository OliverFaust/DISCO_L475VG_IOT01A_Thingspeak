# 6. Tracing: watching the process network at work

The build configuration **Debug-Trace** records what the processes do -- every channel operation,
every ALT, every wait and wake-up, every thread switch -- into a ring buffer in RAM. You stop the
board in the debugger, copy the ring to the PC, convert it and look at it as a timeline in Perfetto.
This chapter takes you through it step by step.

> **Note: an unreleased trace format.** Debug-Trace uses the source of CSP4CMSIS's unreleased
> `dev/tracing` branch (version 3.2.0 in `csp_version.h`, commit `ca2610a`, in `lib/csp4cmsis-trace/`;
> see its `VERSION`). The record format, the event numbers and the tools may still change before a
> release. Debug and Release use the released CSP4CMSIS 3.1.0 (`lib/csp4cmsis/`) and contain no trace
> code at all: they build to exactly the same image as without Debug-Trace (checked,
> `docs/results/2026-11-14_tracing.md`).

**Tested with:** the tools of chapter 1, Python 3.13 (Ubuntu 25.04), Google Chrome with
<https://ui.perfetto.dev>, and optionally ProB 1.x (`probcli`) for the conformance check. Windows: the
Python scripts run unchanged; the shell scripts in `tools/` need Git Bash or WSL (not tested).

---

## 6.1 What gets recorded, and where

| Part | File | What it does |
|---|---|---|
| Trace hooks in the library | `lib/csp4cmsis-trace/csp4cmsis/...` (`CSP4CMSIS_TRACE`) | each channel put/get, ALT guard/selection/deadline, wait/resume becomes an event |
| Ring backend | `lib/csp4cmsis-trace/csp4cmsis/src/csp_trace_ring.c` (`CSP4CMSIS_TRACE_BACKEND_RING`) | writes each event as a 16-byte record: cycle-counter time stamp, event id, sequence number, two values |
| The ring | `Core/Src/csp_trace_board.c`, `csp_trace.ld` | 1024 records (16 KB + 32-byte header) in SRAM2 at `0x10000000`, `NOLOAD`: flashing does not touch it, `csp_trace_ring_init()` clears it before `main()` |
| Heartbeat | `vApplicationTickHook()` in `csp_trace_board.c` | one record per second, so that the 32-bit cycle counter can be unwrapped |
| Thread switches | `Core/Inc/FreeRTOSConfig.h`, USER CODE Defines | FreeRTOS's `traceTASK_CREATE`, `traceTASK_SWITCHED_IN/OUT` hooks: which thread runs when |

All of these exist only when `CSP4CMSIS_TRACE` is defined, which only Debug-Trace does. The hooks in
`FreeRTOSConfig.h` sit in a USER CODE section and survive regeneration in STM32CubeMX (checked).

**How long the ring lasts.** Measured on the board (`docs/results/2026-11-14_tracing.md`): between
uploads the program writes about 13 records per second -- a heartbeat per second and about 59 records
per sample every 5 s (Sensors reads, Console prints, Uplink takes the sample). One ThingSpeak upload
writes about 690 records in 0.35 s, up to 4500 per second: the Wi-Fi driver waits for the module by
checking a pin every millisecond, and each check is a thread switch to the idle thread and back. The
ring keeps the **newest** 1024 records (policy *overwrite*): about one upload plus 20 s of normal
work. So:

**Dump the ring within about 20 s after an `Uplink: ... -> entry` line** if you want to see an
upload. Later, the upload has been overwritten by normal work. (SRAM2 cannot hold a larger ring:
2048 records would be 32 KB + 32 bytes. The other policy, *stop when full*, would keep only the
first 1024 records after reset -- the Wi-Fi join fills those within a second.)

---

## 6.2 Build and run Debug-Trace

1. STM32CubeIDE: **Project > Build Configurations > Set Active > Debug-Trace**, then **Project >
   Build Project**. Headless:
   ```bash
   ~/st/stm32cubeide_2.1.0/headless-build.sh -data /tmp/ws -import "$PWD" -build DISCO_L475VG_IOT01A_Thingspeak/Debug-Trace
   ```
   You should now see `Build Finished. 0 errors, 0 warnings.` and
   `Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf`.
2. Flash and run it as usual (chapter 3), with a serial terminal on the virtual COM port. The console
   output is the same as in Debug.
3. Wait for an upload line, e.g.
   `[  67.394 s] Uplink: sample #14 -> entry 235694 (328 ms)`, then go to 6.3 within 20 s.

---

## 6.3 Copy the ring to the PC

The ring must be read while the core is **halted**, so that it does not change during the copy.

**With the script** (Ubuntu; it starts the ST-LINK GDB server in *attach* mode -- no reset --, lets GDB
connect, which halts the core, dumps the ring, and lets the board run on):

```bash
tools/trace_dump.sh Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf trace_out/ring.bin
```

You should now see the ring header and

```
$1 = {magic = 0x52505343, format = 0x1, record_size = 0x10, capacity = 0x400, policy = 0x0, write_count = 0x54d9, clock_hz = 0x4c4b400, dropped = 0x0, dwt_ctrl = 0x40000001}
== wrote trace_out/ring.bin (16416 bytes); the board is running again
```

`magic` 0x52505343 is "CSPR"; `capacity` 0x400 = 1024 records; `clock_hz` 0x4c4b400 = 80 MHz.

**By hand in STM32CubeIDE** (any OS): start a debug session for Debug-Trace (**Run > Debug As > STM32
C/C++ Application**), let it run, wait for the upload line, press **Suspend** (pause button), and in
the **Debugger Console** tab type:

```
dump binary memory ring.bin &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)
```

`ring.bin` is written to the debugger's working directory (the project's `Debug-Trace` folder). It
must be 16416 bytes.

---

## 6.4 Convert and check

```bash
tools/trace_decode.sh trace_out/ring.bin Debug-Trace/DISCO_L475VG_IOT01A_Thingspeak.elf trace_out
```

runs, from CSP4CMSIS's `tools/trace`:

1. `ring2log.py ring.bin -o events.log --report ring_report.txt` -- the records as an Event Recorder
   log. **Check the report** (example):
   ```
   ring: capacity 1024 records, policy overwrite oldest, write count 21721, clock 80000000 Hz, DWT_CTRL 0x40000001
   records kept: 1024 (write index 20697..21720); overwritten 20697; dropped 0; torn 0; orphan continuation records 0
   events: 885 over 27.000000 s (33 events/s, 38 records/s)
   heartbeats: 28 (beats 52..79); intervals not about 1 s: none
   ```
   `torn 0` (no record half-written when the core was halted), `orphan continuation records 0` (no
   multi-record event cut in half by the ring's wrap), `dropped 0`, and heartbeats 1 s apart: the
   ring is consistent. "Overwritten" is expected with the overwrite policy.
2. `decode.py -a <elf> -I trace/FreeRTOS_ring.scvd events.log -o events.csv --summary summary.txt
   --checks checks.json --stable stable.csv --perfetto trace.json` -- names the events, resolves
   addresses to symbols (`MainApp_Task(void*)::uplinkSamples` for the channel object), checks the
   library's state, and writes the Perfetto file. `decode.py` uses `eventlist` from CMSIS-Toolbox if
   it is installed; otherwise (as here) `--no-eventlist`, which supports FreeRTOS traces. The summary
   ends with checks like:
   ```
   ring slots: PASS; 11 puts, 11 gets (11 read their put's slot), 0 overwrites ...
   ALT offers: PASS; 18 waits, 45 enabled guards, 0 unresolved, 45 guards described
   ```
3. With ProB installed (`export PROBCLI=/path/to/probcli`): `csp_trace.py` checks parts of the trace
   against CSP models of the library (`lib/csp4cmsis-trace/docs/formal/`), with deliberately broken
   copies ("mutations") that must fail. See 6.7.

---

## 6.5 Look at it in Perfetto

1. Open <https://ui.perfetto.dev> in Chrome. (Perfetto runs in the browser; the trace is not uploaded.)
2. **Open trace file** (left), choose `trace_out/trace.json`.
3. Press **W** (zoom in) and **S** (zoom out), **A**/**D** to move, or drag on the time axis.

You should now see, top to bottom:

- **buffered channels**: one counter per channel that was used in the window --
  `MainApp_Task(void*)::samples`, `::uplinkSamples`, `::status` -- showing how many elements are in the
  buffer (0 or 1 for the capacity-1 channels). (`::check` is written once, at start-up.)
- **interrupts**: interrupt handlers that recorded events (none in this program: the group is empty).
- one group per thread, named after the object that holds its control block:
  `MainApp_Task(void*)::sensors`, `::console`, `::uplink`, `Idle_TCB.3` (FreeRTOS's idle thread) and
  `async_thread_cb_mem` (the Wi-Fi driver's own thread). Each group has three tracks:
  - **running**: when the thread has the CPU;
  - **waiting**: from a `Wait...` event (e.g. `WaitAlt`) to the `Resume` that ends it;
  - **events**: the CSP4CMSIS events as instants (`AltGuard`, `AltSelected`, `BufferedPut`, ...).

*Figure (to be added): Perfetto showing an upload with the samples before and after it. Open
`docs/results/2026-11-14_trace_upload/trace.json` (recorded on the board) to see the same view
without a board.*

**How to read an upload.** Zoom to an `Uplink ... -> entry` time. Every 5 s you see the same pattern:
`sensors` runs briefly (I2C), then puts into `samples` and `uplinkSamples` -- slices "put ..." on its
events track, with flow arrows to the gets that took the values --, `console` wakes from its ALT and
prints, `uplink` wakes from its ALT, takes the sample ("get ... <- sensors"), and waits again. At the
upload, `uplink` and the idle thread alternate every millisecond for about 0.35 s -- the driver's
polling -- while `sensors` and `console` carry on undisturbed.

**KeepNewest, seen in the trace.** A sample that arrives *during* an upload is not lost: it stays in
`uplinkSamples` (counter 1) and the Uplink's get follows right after the upload. A KeepNewest channel
overwrites only when a second put comes before the reader's get, i.e. when the Uplink is busy for
longer than one sample period (5 s) -- in this program only while it joins the network (about 5.7 s
at start-up, 16 s for a failed join). Then the second put is recorded as `OverwroteOldest` (shown as a
put slice), and the put it replaced, if it is still in the window, is labelled *(overwritten, never
read)*. Because the join writes about 4000 records per second, the ring holds such a moment for only
a quarter of a second: catch it with a breakpoint (6.6).

**Timeouts.** Each ALT with a timeout records its deadline (`AltDeadline`); a timeout wake-up is
`ResumeTimeout`. The decoder checks that no timeout ends before its deadline ("ALT deadlines" in
`summary.txt`).

---

## 6.6 Catching a rare event with a breakpoint

To stop exactly when the Sensors process overwrites an unread sample in `uplinkSamples`, set a
breakpoint on the line of `offer()` that records the overwrite, let the board start (the join at
start-up keeps the Uplink busy for about 5.7 s), and dump when it hits:

```
(gdb) break buffered_channel.h:200
(gdb) continue
Breakpoint 1.3, csp::internal::BufferedChannel<Sample, 1u, (csp::BufferPolicy)1>::offer (this=0x200005f4 <(anonymous namespace)::MainApp_Task(void*)::uplinkSamples>, ...)
(gdb) next
(gdb) dump binary memory ring.bin &csp_trace_ring ((char *)&csp_trace_ring) + sizeof(csp_trace_ring)
```

`next` executes the trace call, so the record is in the ring. In STM32CubeIDE: double-click left of
line 200 of `lib/csp4cmsis-trace/csp4cmsis/inc/csp/buffered_channel.h`, debug, and use the Debugger
Console for `dump`. On the lab board it stopped at tick 5042 ms; the decoded window (0.26 s) shows
`uplink` and the idle thread alternating 253 times (the join), `sensors` waking on its timeout, a put
into `samples`, and `OverwroteOldest` on `uplinkSamples`.

## 6.7 Conformance check (optional, needs ProB)

`csp_trace.py` cuts the trace into projections -- one per ALT selection and one per buffered channel --
and checks each against the CSP model of the library construct, with ProB:
`MODEL \ H [T= RECORDED`, i.e. the recorded behaviour is one the model allows. For each model it also
checks a deliberately falsified copy, which must fail (otherwise the check proves nothing).

With this program, the KeepNewest channels of capacity 1 are checked; the ALTs are outside the
models' parameter range (their timeouts are thousands of ticks; the models cover 0..15) and are
listed as unmatched, which is not a failure. Stable points and their checks need the `ProcessStart`
events, which are written once at start-up and are therefore no longer in an overwrite ring that holds
an upload. For the same reason the first event of each process in a window (a `Resume` whose
`WaitAlt` lies before the window) is reported by `decode.py` as an "ALT deadlines" error, and an
`OverwroteOldest` whose evicted put lies before the window as a "ring slots" error: check that such
errors concern the start of the window before you look further. Results:
`docs/results/2026-11-14_tracing.md`.

---

## 6.8 What tracing costs

| | Debug | Debug-Trace | Difference |
|---|---|---|---|
| Code (text) | 109 664 B | 113 416 B | +3 752 B |
| RAM (data + bss) | 48 528 B | 64 960 B | +16 432 B (ring in SRAM2, otherwise unused) |
| CPU per record | -- | at most about 370 cycles = 4.6 us at 80 MHz | |
| CPU, during an upload | -- | at most about 2 % | |
| CPU, averaged over a 60 s cycle | -- | about 0.01 % | |

(How the CPU figures were obtained: `docs/results/2026-11-14_tracing.md`.)

---

## 6.9 Troubleshooting

- **`ring2log.py`: bad magic** -- `ring.bin` was dumped from a Debug or Release build (no ring), or
  not from `&csp_trace_ring`. Check the active build configuration.
- **No upload in the trace** -- dumped too late (6.1): dump within 20 s after the upload line.
- **`trace_dump.sh` resets the board** -- an old copy without `-g` (attach) for `ST-LINK_gdbserver`.
- **Torn records > 0** -- the core was not halted during the copy (a debugger reading memory while the
  program runs). Use the script or suspend first.
