# Board support files (copied by hand, not by STM32CubeMX)

Source: STM32CubeL4 V1.18.2 (https://github.com/STMicroelectronics/STM32CubeL4, tag v1.18.2),
`Drivers/BSP/`, unmodified:

| Here | Submodule commit |
|---|---|
| `B-L475E-IOT01/`: `stm32l475e_iot01.h`, `stm32l475e_iot01_{t,h,p}sensor.{c,h}`, `LICENSE.md` | 2717974 (stm32l475e-iot01-bsp) |
| `Components/hts221/` | 389557f (stm32-hts221) |
| `Components/lps22hb/` | 4a428fc (stm32-lps22hb) |
| `Components/Common/`: `hsensor.h`, `psensor.h`, `tsensor.h` | 80596b4 (stm32-bsp-common) |

Licence: BSD-3-Clause (STMicroelectronics), see `B-L475E-IOT01/LICENSE.md` and the file headers.

`stm32l475e_iot01.c` is deliberately **not** included: it initialises I2C2 itself (its own handle,
pins and timing), whereas in this project STM32CubeMX configures I2C2 (`hi2c2` in `Core/Src/main.c`).
The four `SENSOR_IO_*` functions the sensor drivers call are implemented on `hi2c2` in
`Core/Src/sensor_io.c`. See `docs/08-how-it-was-made.md`.

Temperature and humidity are not taken from `BSP_TSENSOR_ReadTemp()`/`BSP_HSENSOR_ReadHumidity()`:
`hts221.c` truncates the HTS221's calibration points, so the project converts with the datasheet
formula in `Core/Src/hts221_calib.c` (docs/05-code-walkthrough.md). The files here stay unmodified.

## Wi-Fi

The ISM43362 Wi-Fi driver is not here: ST's driver (`es_wifi`) is under the SLA0044 licence and
is not used. The project uses Arm's CMSIS-Driver instead, see `Drivers/CMSIS-Driver/README.md`.
