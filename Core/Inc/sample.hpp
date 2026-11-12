// sample.hpp -- the message the Sensors process sends: one reading of all three sensors.
#pragma once

#include <cstdint>
#include <type_traits>

struct Sample {
    uint32_t seq;            // 1, 2, 3, ... (a gap at the reader means a sample was overwritten)
    uint32_t time_ms;        // kernel tick count (1 tick = 1 ms) when the sample was taken
    float    temperature_c;  // HTS221, degrees Celsius
    float    humidity_pct;   // HTS221, relative humidity, percent
    float    pressure_hpa;   // LPS22HB, hectopascal (= millibar)
    bool     valid;          // false: a sensor did not initialise or an I2C transfer failed
};

// CSP4CMSIS buffered channels copy elements with memcpy.
static_assert(std::is_trivially_copyable_v<Sample>, "Sample must be trivially copyable");
