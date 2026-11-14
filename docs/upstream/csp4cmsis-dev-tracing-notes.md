# Notes for CSP4CMSIS `dev/tracing` (not filed; the CSP4CMSIS repository was not changed)

Findings from using the unreleased trace support of CSP4CMSIS in this project's Debug-Trace build
(docs/06-tracing.md; results in docs/results/2026-11-14_tracing.md).

**Version:** branch `dev/tracing`, commit `ca2610a` (2026-11-11), `csp_version.h` 3.2.0; copied unmodified into `lib/csp4cmsis-trace/` (see its `VERSION`).
**Setup:** B-L475E-IOT01A (STM32L475VG, Cortex-M4), STM32Cube FW_L4 V1.18.2 (CMSIS-Core 5.3),
FreeRTOS V10.3.1 with ST's CMSIS-RTOS2 wrapper, GNU Tools for STM32 14.3.rel1, RAM ring backend
(`CSP4CMSIS_TRACE_BACKEND_RING`), 1024 records, policy overwrite; Python 3.13, ProB 1.16.1.

Each note: what happens, where, how this project works around it, and a suggestion.

---

## 1. `csp_trace_ring.c` uses `DCB`, which ST's CMSIS-Core 5.x does not define

**What happens.** The Debug-Trace build fails to compile `csp_trace_ring.c` with ST's device headers:
`DCB` and `DCB_DEMCR_TRCENA_Msk` are undeclared.

**Where.** `csp4cmsis/src/csp_trace_ring.c`, `dwt_init()`:

```c
DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
```

`DCB` is the CMSIS 6 name of the Debug Control Block. CMSIS-Core 5.x -- which every current STM32Cube
firmware package ships (FW_L4 V1.18.2: CMSIS-Core 5.3, `cmsis_version.h`) -- calls the same register
`CoreDebug->DEMCR` with `CoreDebug_DEMCR_TRCENA_Msk`. (The STM32N6570-DK port, the template for this
backend, uses CMSIS 6, where it compiles.)

**Workaround here.** Debug-Trace sets `CSP4CMSIS_DEVICE_HEADER="csp_trace_device.h"`, a project header
that includes `stm32l4xx.h` and maps the names:

```c
#ifndef DCB
#define DCB                   CoreDebug
#define DCB_DEMCR_TRCENA_Msk  CoreDebug_DEMCR_TRCENA_Msk
#endif
```

**Suggestion.** Do the same in the library, in `csp_trace_ring.c` (or `csp_trace_ring.h`) after the
device header: `#if !defined(DCB) && defined(CoreDebug)` -> the two defines above. Then the backend
builds unchanged with CMSIS 5 and 6, and `Documentation/CSP4CMSIS_STM32CubeIDE.md` needs no extra step
for the STM32Cube packages that still ship CMSIS 5.

---

## 2. Events from before the window are reported as errors by the checker

**What happens.** With the overwrite policy the ring holds the newest records only; the decoded
trace starts in the middle of the program. `decode.py --checks` (via `conformance.py`) then reports,
in every dump of this project:

```
ALT deadlines: FAIL 3; ... 6 ResumeTimeout (at most 0 ticks after the deadline), 10 Resume (10 before the deadline, 0 at it, 0 after)
```

with `deadline_errors` such as

```
ResumeTimeout at 0.000 us without a deadline wait
Resume at 1996.862 us with ticks after a wait without a deadline
Resume at 7578.850 us with ticks after a wait without a deadline
```

one per process: the `WaitAlt`/`AltDeadline` that began each wait lie before the window. In the same
way a get whose put lies before the window gives `ring slots: FAIL 1` (`<channel>: get at 42.962 us
from an empty replay`), and an `OverwroteOldest` whose evicted put lies before the window (dump c,
caught with a breakpoint) gives `ring slots: FAIL 1` as well.

**Where.** `tools/trace/conformance.py`, `resume_deadline()` (`dl_error(... without a deadline
wait)`), and the `BUFFERED_GET` branch of the ring-slot replay (`ring_error(... from an empty
replay)`).

**Workaround here.** None in the tools; the student guide (docs/06, 6.4) explains how to recognise the
artefacts (each report lies before the first own `WaitAlt` of its process, or concerns the first get
of a channel). The results note checks each one by hand.

**Suggestion.** `ring2log.py` already knows that the window is partial: it writes the note
`ring: N records overwritten, 0 dropped before this point` at its start. When that note is present
(or `write_count > capacity` with policy overwrite), treat the first resume of each thread without a
recorded wait, and the first get / overwrite of each channel without a recorded put, as *window start*:
count them separately (e.g. `ALT deadlines: PASS; ... 3 waits begun before the window`) instead of as
failures. A FAIL would then always mean a finding.

---

## 3. No stable points with an overwrite ring

**What happens.** `stable points: 0 {}` in every dump of this project; the stable-point checks and
the conformance checks that need them do not run.

**Where.** `tools/trace/conformance.py` builds the set of live processes only from `ProcessStart`
events (the `PROCESS_START` branch); `ProcessStart` is written once per process, at start-up. An
overwrite ring that holds an upload (here: about 690 records in 0.35 s, after 2.2 x 10^5 records since
reset) lost those records long ago.

**Workaround here.** None; documented as a limitation (docs/06, 6.7). A stop-when-full ring would keep
the start-up, but the Wi-Fi join of this project fills 1024 records within a second, so it would not
reach the steady state.

**Suggestion.** Let the decoder learn live threads from what the window does show: any thread that
records a CSP4CMSIS event or a `TaskSwitchedIn` is live, and a process is known by its control block
address. Alternatively, the backend could repeat a compact "process table" record (thread address and
process id for each process) from `csp_trace_ring_tick()`, e.g. with every heartbeat, so that every
window of more than a second contains it.

---

## 4. Thread names are lost with the start-up records

**What happens.** `decode.py` names a thread after `CSProcess::name()` from the `ProcessName` record,
which follows `ProcessStart` at start-up; in an overwrite window it falls back to the symbol of the
control block (`MainApp_Task(void*)::sensors`, `Idle_TCB.3`, `async_thread_cb_mem`). The `TaskName`
data record of the FreeRTOS thread hook (`csp_trace_task_create()`, component 0x51, message 0x03) is
written at the same time and lost in the same way.

**Where.** `tools/trace/decode.py`, class `Names` and the `PROCESS_NAME` branch (`pending_start`).

**Workaround here.** `tools/trace_names.py`: while the core is halted for the dump, GDB reads
`pcTaskName` of every control block that appears in the ring's thread records (CSP4CMSIS's `Run`
passes `name()` to `osThreadNew`), and the names are put into `trace.json` afterwards (docs/06, 6.3).

**Suggestion.** Accept a names file in `decode.py` (`--names FILE`: address and name per line), or
take the names from the periodic process table suggested in note 3.

---

## 5. `csp_trace.py` writes a get without its put as `rd.UNKNOWN`, which ProB rejects

**What happens.** For the window-start get of note 2, the channel projection contains the event
`rd.UNKNOWN`. ProB stops with a parse error, and the channel's trace check is UNKNOWN (dump d):

```
! CSP-M Parse error message:  Unbound Identifier :UNKNOWN
```

The mutation checks of the same channel still run and are caught; the other channel PASSes.

**Where.** `tools/trace/csp_trace.py`, the projection of buffered channels:

```python
t = (ident % k) if ident is not None else 'UNKNOWN'
```

**Workaround here.** None; reported as UNKNOWN (not PASS) in the results, as the tool intends.

**Suggestion.** Start a channel's projection after the window-start gets (or after its first recorded
put), together with note 2; the remaining trace is then checkable.
