> **Frozen — Mbed OS legacy version.**
> This branch (`mbed-os-legacy`, tag `mbed-os-final`) keeps the original Mbed OS project unchanged.
> Mbed OS reached end of life in July 2026, so this version is no longer maintained.
> The current version (STM32CubeIDE + FreeRTOS + CSP4CMSIS) is on the
> [`main`](https://github.com/OliverFaust/DISCO_L475VG_IOT01A_Thingspeak/tree/main) branch.

DISCO_L475VG_IOT01A_Thingspeak

Creator: Dr Oliver Faust

Year: 2025

Functionality:
Connect DISCO_L475VG_IOT01A to Thingspeak via WiFi.
Transfer: Temperature, Humidity, and Pressure to a Thingspeak channel. 

Setup:
- Create a Thingspeak channel with 3 fields.
- Clone DISCO_L475VG_IOT01A_Thingspeak into MBED studio
- Provide the information required in mbed_app,josn
- Select DISCO-L475VG-IOT01A (B-L475E-IOT01A) as build target. 
- Build and run.
