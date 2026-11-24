# 4. Architecture: the process network

This chapter explains how the program is built from processes and channels, and why. Chapter 5
then walks through the code of each process; chapter 6 shows the network at work in a trace.

---

## 4.1 The problem

The program has three jobs with very different timing:

| Job | Timing | Can fail? |
|---|---|---|
| read temperature, humidity and pressure (I2C) | every 5 s, about 2 ms each | rarely |
| send the newest values to ThingSpeak (Wi-Fi, HTTP) | every 60 s; one upload 0.3-0.5 s, joining a network 5 s, a failed join 16 s | often: no network, no internet, wrong key, rate limit |
| report what happens (serial console) | whenever something happens | no |

The Mbed OS version did all three in one loop in `main()`: while it waited for the network, nothing
was measured, and a network error stopped everything (`docs/migrating-from-mbed.md`). Here each job is
a **process** of its own, and the processes talk only through **channels**. A slow or failing network
then delays the uploads -- and nothing else.

---

## 4.2 Processes and channels

```
                 samples (Sample, 1, KeepNewest)
   +---------+ ----------------------------------------------> +---------+
   | Sensors | ----------------------------------------------> | Console |  only printer
   |  I2C2   |   check (Hts221Check, 1, KeepNewest; once)       +---------+
   +---------+                                                       ^
        |   uplinkSamples (Sample, 1, KeepNewest)                    | status (UplinkStatus, 8, KeepNewest)
        v                                                            |
   +---------+ ------------------------------------------------------+
   | Uplink  |  Wi-Fi module (SPI3), ThingSpeak; osPriorityLow
   +---------+
```

(The same diagram heads `Core/Src/app.cpp`, where the network is built.)

| Process | Owns | Does | Waits in an ALT for | Priority | Stack |
|---|---|---|---|---|---|
| **Sensors** (`SensorsProcess`) | I2C2: HTS221 (temperature, humidity), LPS22HB (pressure) | reads a `Sample` every 5 s and writes it to `samples` and `uplinkSamples`; once at start a sensor report to `check` | the sample time (timeout) | `osPriorityBelowNormal` | 512 words |
| **Console** (`ConsoleProcess`) | USART1 = the serial console (`printf`) | prints every sample, the sensor report, every Uplink event, and a stack report | `samples`, `check`, `status`, or the report time | `osPriorityBelowNormal` | 512 words |
| **Uplink** (`UplinkProcess`) | SPI3 and the ISM43362 Wi-Fi module (through Arm's CMSIS-Driver) | starts the module, joins the network, uploads the newest sample every 60 s, rejoins and retries with back-off; reports each event to `status` | `uplinkSamples`, or the next upload time | `osPriorityLow` | 1024 words |

| Channel | Element | Capacity | Policy | Writer -> reader |
|---|---|---|---|---|
| `samples` | `Sample` (time, sequence number, T, RH, p, validity) | 1 | KeepNewest | Sensors -> Console |
| `uplinkSamples` | `Sample` | 1 | KeepNewest | Sensors -> Uplink |
| `check` | `Hts221Check` (sensor report) | 1 | KeepNewest | Sensors -> Console, once at start |
| `status` | `UplinkStatus` (event, times, codes, entry id) | 8 | KeepNewest | Uplink -> Console |

`Run(InParallel(sensors, console, uplink), ...)` in `app.cpp` starts the three processes as FreeRTOS
threads. They run until the board is reset; no process ever ends.

---

## 4.3 Why channels, not shared variables

The obvious alternative is a global `Sample latest` that Sensors writes and the others read. Three
things go wrong with it:

1. **Torn reads.** A `Sample` is several words. If the Uplink is interrupted by Sensors halfway
   through copying it, it sends a temperature from one reading and a humidity from the next. The fix
   is a mutex around every access -- in every process, forever.
2. **No notification.** The reader does not know *when* there is something new. It polls (wasting
   CPU and adding delay) or needs a semaphore or event flag in addition to the mutex.
3. **Waiting for two things.** The Console must react to a sample *or* an Uplink event *or* its report
   time, whichever comes first. With shared variables and semaphores this is hand-written, error-prone
   code.

A channel does all of it in one construct: a put copies the whole element into the channel, a get
copies it out, both atomically; a get waits until something is there; and an **ALT** (CSP4CMSIS's
`Alternative`) waits on several channels and a timeout at once and returns the one that is ready. Each
process is then plain sequential code -- "wait for the next event, handle it, repeat" -- and all
communication is visible in one place, the diagram above. That is the idea of CSP (Communicating
Sequential Processes), which CSP4CMSIS brings to CMSIS-RTOS2.

---

## 4.4 Ownership of the hardware

Each hardware resource has exactly one process that uses it:

| Resource | Owner | Nobody else ... |
|---|---|---|
| I2C2 (HTS221, LPS22HB) | Sensors | calls a `BSP_xSENSOR_...` function or `HAL_I2C_...` on I2C2 |
| SPI3 and the ISM43362 module | Uplink (and the driver's own thread, which the Uplink's calls start) | calls the CMSIS-Driver WiFi API |
| USART1 (console) | Console | calls `printf` (apart from the fatal-error report in `console_io.c`, which stops the program) |

Consequences:

- **No locks.** A resource with one user needs no mutex; there is no priority inversion and no
  deadlock over a resource.
- **Clear failure boundaries.** An I2C error is handled inside Sensors, a network error inside the
  Uplink; the others only see fewer or different messages on a channel.
- **Output is never interleaved.** Every line comes from one process, so two lines can never be mixed
  character by character. Whoever wants something printed sends a message to the Console.

The diagnostic modes of chapter 7 (`APP_DIAGNOSTIC_MODE`) replace the network by a single test process
that owns both the Wi-Fi module and the console for its run.

---

## 4.5 Why KeepNewest, and why these capacities

Every channel is **buffered** with the policy **KeepNewest**: a put never blocks; if the channel is
full, the new element replaces the oldest unread one. So **no writer ever waits for a reader**:

- **Sensors keeps its 5 s rhythm**, whatever the network does. With an unbuffered (rendezvous)
  channel, Sensors would wait at its put to `uplinkSamples` until the Uplink comes back from a 16 s
  failed join -- and the Console would get no samples in that time either.
- **The Uplink sends the newest sample.** It uploads once a minute but receives a sample every 5 s; it
  wants the latest one, not a queue of old ones. Capacity 1 with KeepNewest is exactly "the latest
  value". An overwrite happens only when two samples arrive while the Uplink is busy (more than 5 s:
  only while it joins the network) -- and then the older one is the one to drop. Exercise 9.2 measures
  this.
- **The Console** is fast (it takes a sample about 0.1 ms after the put and prints it in a few
  milliseconds), so `samples` does not overwrite in practice; in the traces of chapter 6 it never did.
- **`status` has capacity 8**, because Uplink events can come in quick succession (e.g. *rejoining*,
  *joined*, *uploaded*) and every one should be printed, even if the Console is busy printing a sample
  or the stack report at that moment. Should the Console ever fall behind by more than 8 events, the
  oldest are dropped rather than the Uplink being stopped.
- **`check`** carries one report, once.

The cost of KeepNewest is that a reader may miss elements. That is acceptable here because every
reader wants the current state, not every value. For a reader that needs every value (exercise 9.2,
question 9: a bulk upload), the policy would have to change.

---

## 4.6 Time and priorities

**Every process waits in an ALT with a timeout** -- never in `osDelay()` or a busy loop:

- Sensors: a timeout guard alone (the next sample time).
- Console: three input guards and a timeout (the next stack report).
- Uplink: an input guard on `uplinkSamples` and a timeout (the next upload time). While it waits, it
  takes every new sample; when the timeout fires, it uploads the newest one it has.

A waiting process uses no CPU; in a trace the processor is idle about 96 % of the time (chapter 6).

**Priorities** (FreeRTOS, higher runs first):

| Thread | Priority | Why |
|---|---|---|
| Wi-Fi driver thread (`WiFi_ISM43362_Async_Thread`) | `osPriorityAboveNormal` | Arm's driver's setting: it polls the module every few milliseconds |
| MainApp (starts the network, then ends) | `osPriorityNormal` | -- |
| Sensors, Console | `osPriorityBelowNormal` (the network priority given to `Run`) | short work, must not be delayed by the network |
| Uplink | `osPriorityLow` | while the driver waits for the module it polls a pin every 1 ms; at low priority this never delays Sensors or Console |
| FreeRTOS idle | lowest | runs when everybody waits |

---

## 4.7 Memory

Everything is allocated statically: the channels and processes are `static` objects in `app.cpp`,
each `CSProcessStatic<N>` holds its own stack (N words) and thread control block, and MainApp has a
1 KB static stack. The application never calls `malloc()` or allocates from the FreeRTOS heap. The only
dynamic allocation is inside Arm's Wi-Fi driver: 384 bytes from the FreeRTOS heap for its mutexes and
event flags (the stack report shows `FreeRTOS heap: free 640 of 1024 bytes`). The stack report every
10 minutes shows how much of each stack has ever been used (chapter 3.7).

---

## 4.8 When the Wi-Fi module does not start

If the module fails to start, the Uplink restarts the whole board (the driver cannot be started a
second time; chapter 5, 5.2). After 3 failed starts in a row the board stops restarting and runs Sensors and
Console alone, printing once a minute that Wi-Fi is unavailable (chapter 7, 7.1). The KeepNewest
channel `uplinkSamples` makes this simple: Sensors writes into it whether or not anyone reads.
