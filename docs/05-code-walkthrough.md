# 5. Code walkthrough

This chapter explains the source, process by process. It grows with the project; this version
covers one topic of the Sensors process: how temperature and humidity are computed from the HTS221's
raw output, and why the project does not use ST's conversion for it.

---

## 5.x Temperature and humidity: the HTS221 conversion

Files: `Core/Inc/hts221_calib.h`, `Core/Src/hts221_calib.c`, used by `Core/Src/sensors_process.cpp`.

### What the sensor delivers

The HTS221 does not output degrees or percent. It outputs two raw 16-bit numbers, `T_OUT` and
`H_OUT`, and stores two **calibration points** per quantity, measured at the factory, in its
registers 0x30..0x3F. A reading is converted by linear interpolation between the two points (HTS221
datasheet, "Temperature/humidity conversion"):

```
T  = T0 + (T_OUT - T0_OUT)    * (T1 - T0) / (T1_OUT - T0_OUT)
RH = H0 + (H_OUT - H0_T0_OUT) * (H1 - H0) / (H1_T0_OUT - H0_T0_OUT)
```

The calibration temperatures are stored in eighths of a degree (`T0_degC_x8`, 10 bits), the
calibration humidities in halves of a percent (`H0_rH_x2`, 8 bits). So T0 = `T0_degC_x8` / 8 and
H0 = `H0_rH_x2` / 2, with fractions.

### What ST's driver does

ST's driver (`Drivers/BSP/Components/hts221/hts221.c`, used through `BSP_TSENSOR_ReadTemp()` and
`BSP_HSENSOR_ReadHumidity()`) applies the same interpolation, but first **truncates** the calibration
points to whole units:

```c
T0_degC = T0_degC_x8_u16 >> 3;   /* 163 >> 3 = 20, but 163 / 8 = 20.375 */
H0_rh   = buffer[0] >> 1;        /* 139 >> 1 = 69, but 139 / 2 = 69.5   */
```

Shifting right by 3 divides by 8 and throws the remainder away. The interpolation line then goes
through the wrong points, and every reading is off by an amount that depends on the board's
calibration and on the reading itself: up to 0.875 °C in a calibration temperature, up to 0.5 %RH
in a calibration humidity.

### On the lab board

Calibration registers of the board used for this project (`docs/results/2026-11-12_sensor_check.md`):

| | Stored | Datasheet | ST's driver |
|---|---|---|---|
| T0 | `T0_degC_x8` = 163 | 20.375 °C | 20 °C |
| T1 | `T1_degC_x8` = 272 | 34.000 °C | 34 °C |
| H0 | `H0_rH_x2` = 62 | 31.0 %RH | 31 %RH |
| H1 | `H1_rH_x2` = 139 | 69.5 %RH | 69 %RH |

For the raw sample H_OUT = -16528, T_OUT = -291 (T0_OUT = -2, T1_OUT = 768, H0_T0_OUT = 6,
H1_T0_OUT = -12603):

```
datasheet:  T  = 20.375 + (-291 + 2) * (34.000 - 20.375) / 770   = 15.261 °C
ST driver:  T  = 20     + (-291 + 2) * (34     - 20    ) / 770   = 14.745 °C    (-0.52 °C)
datasheet:  RH = 31.0 + (-16528 - 6) * (69.5 - 31.0) / -12609    = 81.484 %
ST driver:  RH = 31   + (-16528 - 6) * (69   - 31  ) / -12609    = 80.829 %     (-0.66 %RH)
```

Because T1 happens to be a whole number on this board, the temperature error is zero at 34 °C and
grows towards lower temperatures (-0.375 °C at 20.375 °C, -0.52 °C at 15 °C). Another board has other
calibration values and a different error.

### What this project does

`hts221_calib.c` keeps ST's driver for what it does correctly (identifying and configuring the
sensor in `BSP_TSENSOR_Init()`/`BSP_HSENSOR_Init()`), and replaces only the conversion:

1. `hts221_calib_read()` reads the 16 calibration registers in one I2C transfer and
   `hts221_calib_decode()` turns them into a `Hts221Calib` struct, as fractions:
   `c->t0_degc = (float)t0x8 / 8.0f;`. The Sensors process calls it once, in `initSensors()`; the
   calibration never changes, so there is no need to read it again for every sample (ST's driver
   does, which costs six of its eight I2C transfers per reading).
2. For each sample, `hts221_read_raw()` reads `H_OUT` and `T_OUT` in one 4-byte transfer, and
   `hts221_calib_temperature()` / `hts221_calib_humidity()` apply the datasheet formula.

The module keeps no state of its own: the `Hts221Calib` lives in the Sensors process object, the
only thread that uses I2C2. The decoding and conversion are pure functions, so they can be compiled
and tested on a PC (`docs/results/2026-11-12_hts221_fix.md`, section 1).

At start-up the Sensors process also sends a one-off check (`readHts221Check()`), which the Console
prints: the calibration bytes, one raw sample, and the result of both formulas side by side. On the
lab board:

```
  T : datasheet formula (samples) 15.562 C, ST driver formula 15.055 C, BSP_TSENSOR_ReadTemp() 15.055 C
  RH: datasheet formula (samples) 80.752 %, ST driver formula 80.106 %, BSP_HSENSOR_ReadHumidity() 80.106 %
```

**Try it:** run the project on your own board and compare its calibration bytes and its two
temperature values with the ones above. How large is the truncation error on your board, and at
which temperature is it zero (if anywhere in the sensor's range)?
