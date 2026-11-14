# First upload no earlier than 15 s after boot (2026-11-14)

**Why.** ThingSpeak accepts one update per channel every 15 s, and the limit holds across resets of
the board. The Uplink uploaded as soon as it had joined and had a sample, about 7 s after the start.
A reset a few seconds after an upload therefore made the first upload of the new run come less than
15 s after the previous one. Seen on the lab board during the trace tests (Debug-Trace, reset by
`trace_dump.sh --reset` a few seconds after the upload at 6.387 s of the run before):

```
Reset cause: software reset; consecutive restarts after a failed Wi-Fi start: 0 of 3
[    5.728 s] Uplink: joined (4794 ms), IP 192.168.1.28  [ok 0, rejected 0, failed 0, joins 1]
[    7.262 s] Uplink: sample #2 REJECTED by ThingSpeak (HTTP 200, entry 0)  [ok 0, rejected 1, failed 0, joins 1]
```

(`2026-11-14_trace_trigger/break_counter/console_rejected_run.txt`.)

**Change.** `Core/Src/uplink_process.cpp`, `UplinkProcess::run()`:
`uint32_t nextUploadAt = kMinPeriodMs;` (was `0`). `now()` is the kernel tick count, which starts
after the reset, so the first upload of a run comes at least 15 s after the last upload of the run
before. One line changed, so the line numbers used in docs/06 and exercise 9.3 (line 222) stay.

## Builds

`Build Finished. 0 errors, 0 warnings.` for all three; sizes unchanged (Debug text 109 664, data 152,
bss 48 376; Release text 63 984). Flash images (first 16 hex digits of SHA-256 of
`objcopy -O binary`): Debug `62205605b1f7a49f`, Release `2ed0a033a5a14dda`, Debug-Trace
`c212334bd4a12e7e`.

## Test on the board

**Debug** (`debug_console.txt`): flashed with `tools/flash_and_capture.sh` (150 s capture); after the
first upload, reset with `STM32_Programmer_CLI -c port=SWD -hardRst` 4 s after it; after the next
first upload, reset again 3 s after it.

| Run | Joined | First upload | Gap to the previous upload | Result |
|---|---|---|---|---|
| 1 (after flashing) | 9.579 s | 15.335 s, sample #3 | -- | `-> entry 236391` |
| 2 (reset 4 s after run 1's upload) | 5.726 s | 15.329 s, sample #3 | about 19.4 s | `-> entry 236392`, accepted |
| 3 (reset 3 s after run 2's upload) | 5.726 s | 15.350 s, sample #3 | about 18.4 s | `-> entry 236393`, accepted |

Run 3 went on normally: `75.681 s sample #16 -> entry 236394` (60 s after the end of the first
upload, as before). No `REJECTED` line in the capture.

**Debug-Trace** (`debug_trace_console.txt`, `debug_trace_break_counter_dump.txt`): exercise 9.3's
counter condition, started 3 s after the first upload (15.337 s, entry 236396):
`tools/trace_dump.sh --break uplink_process.cpp:222 --if "uploadsOk_ == 2" --reset --timeout 180`.
After the reset: `15.347 s -> entry 236397`, `75.683 s -> entry 236398`, the breakpoint hit
(`Breakpoint 1, UplinkProcess::run ...`, `halted at pc = 0x8007cee`), then `136.024 s -> entry
236399`. All accepted. The instruction "start at least 15 s after the last upload" was removed from
exercise 9.3.

Side effect: the first sample on the channel arrives about 8 s later after a start (15.3 s instead of
about 7 s); the docs (02, 03, 05) show the new console output.

## Not covered

- HTTP 200 with entry 0 because no field of the update is enabled in the channel (docs/07, 7.2,
  marked as not measured). Measured earlier: with some fields disabled, the update is accepted
  (`2026-11-12_one_upload.md`, attempts 2 and 3).
- A second device writing to the same channel (docs/07, 7.2): not provoked.
