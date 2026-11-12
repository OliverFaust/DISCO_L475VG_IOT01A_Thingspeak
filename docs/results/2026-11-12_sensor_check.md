# Sensor check (2026-11-12): I2C access and HTS221 conversion

Question: does `Core/Src/sensor_io.c`, which replaces ST's `stm32l475e_iot01.c` (8.7), give ST's
HTS221 and LPS22HB drivers exactly the I2C transfers they expect? And how are the HTS221's raw values
turned into temperature and humidity? Answered by reading the code (section 1) and by a register dump
on the board (sections 2 and 3). Not covered: the accuracy of the sensor itself (breath test and
reference comparison: done separately by the lecturer).

## 1. Code review: what the drivers pass, what reaches the bus

| Point | ST's drivers | `sensor_io.c` | Verdict |
|---|---|---|---|
| Device address | `HTS221_I2C_ADDRESS` 0xBE, `LPS22HB_I2C_ADDRESS` 0xBA (`stm32l475e_iot01.h`): the 7-bit addresses 0x5F and 0x5D shifted left by one | passed unchanged to `HAL_I2C_Mem_Read/Write`, whose `DevAddress` must be the 7-bit address shifted left (HAL documentation) | correct; same as ST's `I2Cx_ReadMultiple()` |
| Register address | 8 bits | `I2C_MEMADD_SIZE_8BIT`, register passed unchanged | correct; same as ST |
| HTS221 multi-byte reads | the HTS221 increments the register address only if bit 7 of the register address is set; `hts221.c` sets it itself: `HTS221_H0_RH_X2 \| 0x80`, `HTS221_TEMP_OUT_L_REG \| 0x80`, ... (lines 115, 120, 124, 128, 191, 199, 204) | passes the register byte unchanged, so bit 7 reaches the device | correct |
| LPS22HB reads | single-byte reads only (`lps22hb.c` lines 90, 108, 161, 190, 198): no auto-increment involved | — | correct |
| 16-bit values | little-endian, signed: `(buffer[1] << 8) \| buffer[0]` into `int16_t` | — | correct |
| Errors | ST: count nothing, re-initialise I2C2, 1000 ms timeout | counts the failure, re-initialises I2C2, 10 ms timeout | intended difference (lets the Sensors process mark a sample invalid) |

## 2. On the board: registers and one sample

A one-off check in the Sensors process (`readHts221Check()` in `Core/Src/sensors_process.cpp`) reads,
after the BSP's initialisation, the WHO_AM_I registers, HTS221 CTRL_REG1, the HTS221's calibration
registers 0x30..0x3F (once in one 16-byte read with bit 7 set, once byte by byte), one raw sample
(0x28..0x2B in one 4-byte read), and then the BSP's converted values. The Console prints it, with
the conversion done by `hts221_convert()` (`Core/Src/hts221_check.cpp`).

Command: `tools/flash_and_capture.sh Debug/DISCO_L475VG_IOT01A_Thingspeak.elf 75 docs/results/2026-11-12_sensor_check_and_stacks_console.txt`.
Output (full file: `2026-11-12_sensor_check_and_stacks_console.txt`):

```
[    0.023 s] HTS221 check (I2C transfers all OK)
  WHO_AM_I: HTS221 @0xBE: 0xBC (expect 0xBC), LPS22HB @0xBA: 0xB1 (expect 0xB1)
  HTS221 CTRL_REG1: 0x85 (expect 0x85: active, BDU, 1 Hz)
  calibration 0x30..0x3F: 3E 8B A3 10 00 C4 06 00 7A 03 C5 CE FE FF 00 03
  same 16 registers read one at a time: identical
  H0_rH_x2 = 62 -> H0 = 31.0 %RH, H1_rH_x2 = 139 -> H1 = 69.5 %RH
  T0_degC_x8 = 163 -> T0 = 20.375 C, T1_degC_x8 = 272 -> T1 = 34.000 C
  H0_T0_OUT = 6, H1_T0_OUT = -12603, T0_OUT = -2, T1_OUT = 768
  raw sample: H_OUT = -16528, T_OUT = -291
  T : datasheet formula 15.261 C, ST driver formula 14.745 C, BSP_TSENSOR_ReadTemp() 14.745 C
  RH: datasheet formula 81.484 %, ST driver formula 80.829 %, BSP_HSENSOR_ReadHumidity() 80.829 %
```

What this shows:

- **Addresses:** both sensors answer at the 8-bit addresses with their identity values.
- **Auto-increment:** the 16-byte read equals 16 single-byte reads, so bit 7 does reach the HTS221
  and the multi-byte path through `sensor_io.c` works.
- **Initialisation:** CTRL_REG1 = 0x85 = power on (bit 7), block data update (bit 2), 1 Hz (bits 1..0 = 01),
  as `HTS221_T_Init()`/`HTS221_H_Init()` write it.
- **Same sample:** the BSP's values equal the "ST driver formula" applied to the raw sample read just
  before, so the BSP converted the same sample (BDU, 1 Hz output rate).

## 3. The conversion, by hand

Decoding of the dump (register: bytes):

| Register | Bytes | Value |
|---|---|---|
| 0x30 H0_rH_x2 | 3E | 62 -> H0 = 62 / 2 = 31.0 %RH |
| 0x31 H1_rH_x2 | 8B | 139 -> H1 = 69.5 %RH |
| 0x32 T0_degC_x8, 0x35 bits 1..0 | A3, C4 & 0x03 = 0 | 0x0A3 = 163 -> T0 = 163 / 8 = 20.375 °C |
| 0x33 T1_degC_x8, 0x35 bits 3..2 | 10, (C4 & 0x0C) << 6 = 0x100 | 0x110 = 272 -> T1 = 34.000 °C |
| 0x36..0x37 H0_T0_OUT | 06 00 | 6 |
| 0x3A..0x3B H1_T0_OUT | C5 CE | 0xCEC5 = -12603 |
| 0x3C..0x3D T0_OUT | FE FF | -2 |
| 0x3E..0x3F T1_OUT | 00 03 | 768 |
| (0x34, 0x38, 0x39: reserved) | 00, 7A 03 | not used |

Datasheet formula (HTS221 datasheet, "Temperature/humidity conversion"; linear interpolation between
the two factory calibration points):

```
T  = T0 + (T_OUT - T0_OUT) * (T1 - T0) / (T1_OUT - T0_OUT)
   = 20.375 + (-291 - (-2)) * 13.625 / 770           = 20.375 - 5.114 = 15.261 °C
RH = H0 + (H_OUT - H0_T0_OUT) * (H1 - H0) / (H1_T0_OUT - H0_T0_OUT)
   = 31.0 + (-16528 - 6) * 38.5 / (-12603 - 6)       = 31.0 + 50.484  = 81.484 %
```

Recomputed independently in Python from the 16 bytes and the two raw values: 15.261 °C, 81.484 %;
ST's formula 14.745 °C, 80.829 %. Identical to the board's output.

## 4. Finding: ST's driver truncates the calibration points

`hts221.c` computes `T0_degC = T0_degC_x8_u16 >> 3` and `H0_rh = buffer[0] >> 1`, i.e. it drops the
fractional part of the calibration points before interpolating (lines 117–118, 196–197). The datasheet
defines them as x/8 °C and x/2 %RH. On this board T0 = 20.375 °C becomes 20 °C (T1 = 34.000 °C is
exact), and H1 = 69.5 % becomes 69 %:

| | Datasheet formula | ST driver (and the values this project prints) | Difference |
|---|---|---|---|
| Temperature | 15.261 °C | 14.745 °C | **-0.52 °C** |
| Humidity | 81.484 % | 80.829 % | **-0.66 %RH** |

The error is systematic, not noise: it shifts and tilts the calibration line, so its size depends on
the reading. For this board's calibration the temperature error is zero at T1 = 34 °C (exact),
-0.375 °C at T0 = 20.375 °C, -0.52 °C at 15 °C, and grows further at lower temperatures.
Other boards have other calibration values and other errors (up to 0.875 °C and 0.5 %RH in the
calibration points). The Mbed version used the same driver, so its ThingSpeak data has the same error.

**Not changed yet.** The project still reports ST's values. `hts221_convert()` already computes the
datasheet values; switching the Sensors process to them is a small change, to be decided after the
reference comparison. Self-heating of the board (the HTS221 sits next to the MCU and the Wi-Fi module)
is a separate effect, not examined here.
