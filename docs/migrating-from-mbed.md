# Migrating from the Mbed OS version

For lecturers who used the Mbed OS version of this project (branch `mbed-os-legacy`, tag
`mbed-os-final`). The construct-by-construct comparison (Mbed OS -> STM32CubeIDE, FreeRTOS and
CSP4CMSIS) follows once the Wi-Fi part is ported.

## Differences in the data

### Temperature and humidity in old ThingSpeak channels read low

The Mbed version read the HTS221 through ST's BSP (`BSP_TSENSOR_ReadTemp()`,
`BSP_HSENSOR_ReadHumidity()`). ST's driver truncates the sensor's factory calibration points to whole
degrees and whole percent before converting, and therefore reads too low. On the lab board used for
this port the error is **about -0.5 °C and -0.7 %RH** at room temperature (-0.52 °C and -0.66 %RH at
15 °C; the size depends on each board's calibration and on the reading). ThingSpeak data recorded
with the Mbed version contains this error.

How this was established: the truncation (`T0_degC = T0_degC_x8_u16 >> 3`, `H0_rh = buffer[0] >> 1`) is
in ST's current driver (STM32Cube FW_L4 V1.18.2) and in the Mbed library `BSP_B-L475E-IOT01` as
vendored by another Mbed project (github.com/edgeimpulse/example-dataforwarder-mbed,
`BSP_B-L475E-IOT01/Drivers/BSP/Components/hts221/hts221.c`, lines 135–136 and 214–215). The exact
revision the legacy project pins (`BSP_B-L475E-IOT01.lib`: `bfe8272ced90`) could not be checked:
os.mbed.com, which hosted it, now redirects to GitHub and no longer serves the code.

The current version converts with the datasheet formula (`Core/Src/hts221_calib.c`), so the same board
reads about 0.5 °C and 0.7 %RH higher than before. A step of that size in a channel that continues
from Mbed-era data is this correction, not a change in the room. Details: `docs/05-code-walkthrough.md`
(section on the HTS221 conversion) and `docs/results/2026-11-12_sensor_check.md`.

Pressure (LPS22HB) is converted as before.
