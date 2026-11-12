// fixed_point.hpp -- decimal text for a float without printf("%f").
//
// The C library used by STM32CubeIDE (newlib-nano) leaves floating-point printf out unless
// the program is linked with -u _printf_float (about 10 KB more). The Mbed version printed
// with an integer split that went wrong for negative values: -0.5 came out as "0.-50".
// format_fixed() rounds |value| to `decimals` places as an integer and adds the sign
// separately, so negative values are correct: -0.5 -> "-0.50", -12.345 -> "-12.35" (2 places).
#pragma once

#include <cstddef>

// Writes `value` with `decimals` (0..3) digits after the point into buf (always terminated).
// Rounds half away from zero. Values that round to zero print without a sign ("0.00").
// Returns the number of characters written (as snprintf), or 0 if buf is too small.
// NaN and infinity print as "nan"; |value| >= 1e6 prints as "ovf".
std::size_t format_fixed(char* buf, std::size_t size, float value, unsigned decimals);
