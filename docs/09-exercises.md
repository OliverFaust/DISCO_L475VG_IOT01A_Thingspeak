# 9. Exercises

Graded extensions of the project. Each exercise states what you should observe, so that you can check
your own result. Level A: explain and calculate; level B: change the code and measure on the board;
level C: design decisions that need an argument, not only a measurement. Further exercises (a
button-triggered send via an ALT, an LED status process, the accelerometer as a fourth field, bulk
updates) are added together with the phases they build on.

---

## 9.1 Relative and absolute timeouts: where does the 5.005 s come from?

### Observation

The Sensors process is meant to take a sample every 5 s. The phase 1 log
(`docs/results/2026-11-12_phase1_console.txt`, firmware of commit `ede9cdc`) shows the kernel time of
each sample:

```
[    0.008 s] #1  T = 13.87 C  RH = 83.56 %  p = 976.17 hPa
[    5.018 s] #2  T = 13.82 C  RH = 83.45 %  p = 976.11 hPa
[   10.023 s] #3  T = 13.85 C  RH = 83.49 %  p = 976.15 hPa
[   15.028 s] #4  ...
   ...
[   55.068 s] #12 ...
```

The first interval is 5.010 s, every further one 5.005 s. The period is not 5.000 s, and the error
accumulates: sample 12 is 60 ms later than 11 × 5 s after sample 1.

### The code

`Core/Src/sensors_process.cpp`, the loop of `SensorsProcess::run()` in phase 1 (commit `ede9cdc`;
`git show ede9cdc:Core/Src/sensors_process.cpp`), reduced to its timing:

```cpp
for (;;) {
    Sample s = read(++seq);          // s.time_ms = osKernelGetTickCount(), then BSP_TSENSOR_ReadTemp(),
                                     // BSP_HSENSOR_ReadHumidity(), BSP_PSENSOR_ReadPressure()
    out_ << s;
    RelTimeoutGuard next(period_);   // period_ = Milliseconds(5000) = 5000 ticks (1 tick = 1 ms)
    Alternative alt(next);
    (void)alt.priSelect();           // returns when the timeout has expired
}
```

A `RelTimeoutGuard` is **relative**: it expires `period_` after the moment the ALT starts waiting,
not `period_` after the previous sample. Everything the loop does before it starts waiting (here:
reading three sensor values over I2C at 100 kHz) is added to every period.

### Level A: explain and calculate

1. Draw a time line of two loop iterations: mark where `time_ms` is taken, where the I2C transfers
   happen, where the timeout starts and where it expires. Explain from it why consecutive timestamps
   differ by 5000 ms + *t*<sub>read</sub>.
2. From the log, estimate *t*<sub>read</sub>. Then estimate it from first principles: at 100 kHz one
   bit takes 10 µs; a register read is a start, the device address, the register address, a repeated
   start, the device address again and the data bytes, each 9 bits including the acknowledge. Count the
   transfers that `BSP_TSENSOR_ReadTemp()`, `BSP_HSENSOR_ReadHumidity()` and `BSP_PSENSOR_ReadPressure()`
   make (`Drivers/BSP/Components/hts221/hts221.c`, `lps22hb/lps22hb.c`). Do the two estimates agree?
   Name two further contributions that the bit count leaves out.
3. Why is the first interval (5.010 s) longer than the others? (Hint: what happens in the first
   iteration only?)
4. At 5.005 s per sample: how many samples does the board take in one hour and in one day, instead
   of 720 and 17 280? By how much has the sampling time drifted after one day?
4b. The current code converts temperature and humidity itself (`hts221_calib.c`, docs/05): one 4-byte
   HTS221 transfer per sample instead of ST's eight, plus the LPS22HB's three single-byte reads.
   Predict the new period before you look; then check `docs/results/2026-11-12_hts221_fix_console.txt`
   (5.001 s). Does your model from question 2 explain both 5.005 s and 5.001 s? (Hint: the timeout
   counts whole ticks from the tick in which the ALT starts waiting.)

### Level B: make the period absolute, and measure

Change the loop so that sample *n* is taken at *t*<sub>0</sub> + *n* × 5 s, independent of how long
a reading takes. Keep the ALT: the timeout guard stays a `RelTimeoutGuard` (CSP4CMSIS 3.1.0 has no
absolute one), but its delay is recomputed on every pass from an absolute deadline. The Console
process already does this for its one stack report (`ConsoleProcess::run()`).

Outline (fill in the details yourself):

```cpp
uint32_t deadline = osKernelGetTickCount();            // ticks
for (;;) {
    // ... read and send a sample ...
    deadline += period_.to_ticks();
    const int32_t left = static_cast<int32_t>(deadline - osKernelGetTickCount());
    RelTimeoutGuard next(Ticks(left > 0 ? static_cast<uint32_t>(left) : 0));
    Alternative alt(next);
    (void)alt.priSelect();
}
```

5. Build, flash, and record 60 s with `tools/flash_and_capture.sh` (chapter 3). You should now see
   intervals of 5.000 s, apart from a constant offset of the first sample: e.g. `0.008, 5.008, 10.008, ...`.
   Put the log next to the phase 1 log and compare.
6. Why `static_cast<int32_t>(deadline - now)` and not `deadline > now ? deadline - now : 0`? The tick
   counter is a `uint32_t` in milliseconds: when does it wrap around, and what does each expression do
   just before and just after the wrap?
7. CMSIS-RTOS2 has `osDelayUntil()`, which waits for an absolute tick count directly. Why does the
   project not use it here? (What would the Sensors process lose if it waited in `osDelayUntil()`
   instead of in an ALT? Think of an exercise where it must also react to a button.)

### Level C: which one is right?

8. **Overrun.** Suppose a reading occasionally takes longer than the period (for example while I2C is
   re-initialised after an error). What does the absolute version do afterwards: catch up with a burst
   of samples, or skip? What does your code from question 5 do, and what *should* it do for a sensor
   that is sampled for a cloud dashboard? Change it if necessary, and justify the policy in two or three
   sentences.
9. **ThingSpeak's rate limit.** From phase 3 on, an Uplink process sends a sample to ThingSpeak, which
   rejects updates less than 15 s apart (the response then contains entry id 0; chapter 2). The Uplink
   period will be 60 s by default, configurable down to 15 s. Which kind of timeout should the
   Uplink use: relative (each send at least one period after the previous one ends) or absolute (sends
   on a fixed grid)? Consider a send that is delayed by a Wi-Fi reconnect of 10 s, with a period of
   15 s, in both cases. Which guarantee does ThingSpeak's rule actually require?
10. The Sensors process and the Uplink process may reasonably use *different* answers to questions 8
    and 9. Explain why, in terms of what each process promises to its readers.

### Checking your answers

- Q2: 11 transfers with 20 data bytes, about 500 bit times at 10 µs: ≈ 5 ms. If your estimate is far
  off, recount the transfers per value: ST's drivers re-read the calibration registers on every reading.
- Q4b: 4 transfers with 7 data bytes ≈ 1.8 ms. A period of 5000 ticks plus the whole ticks that pass
  during the reading gives 5001 ms here and 5005 ms in phase 1 (5 to 6 ms of reading).
- Q4: 719.3 samples per hour, 17 263 per day; the drift is 0.1 %, i.e. 86.4 s per day.
- Q5: the decisive check is the log: constant 5.000 s intervals over at least 60 s.
- Q6: the wrap happens after 2<sup>32</sup> ms ≈ 49.7 days. Test your reasoning with concrete numbers,
  e.g. `deadline = 0x00000010`, `now = 0xFFFFFFF0`.
