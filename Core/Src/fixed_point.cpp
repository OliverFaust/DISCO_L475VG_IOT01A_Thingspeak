// fixed_point.cpp -- see fixed_point.hpp.
#include "fixed_point.hpp"

#include <cmath>
#include <cstdio>

std::size_t format_fixed(char* buf, std::size_t size, float value, unsigned decimals)
{
    if (buf == nullptr || size == 0) return 0;
    if (decimals > 3) decimals = 3;

    int n;
    if (!std::isfinite(value)) {
        n = std::snprintf(buf, size, "nan");
    } else if (std::fabs(value) >= 1e6f) {
        n = std::snprintf(buf, size, "ovf");
    } else {
        static constexpr unsigned long kScale[] = {1UL, 10UL, 100UL, 1000UL};
        const unsigned long scale = kScale[decimals];
        // |value| < 1e6 and scale <= 1000: the scaled value fits in 32 bits.
        const unsigned long scaled = static_cast<unsigned long>(std::fabs(static_cast<double>(value)) * scale + 0.5);
        const bool negative = value < 0.0f && scaled != 0;
        const unsigned long whole = scaled / scale;
        const unsigned long frac  = scaled % scale;
        if (decimals == 0) {
            n = std::snprintf(buf, size, "%s%lu", negative ? "-" : "", whole);
        } else {
            n = std::snprintf(buf, size, "%s%lu.%0*lu", negative ? "-" : "", whole, static_cast<int>(decimals), frac);
        }
    }
    if (n < 0 || static_cast<std::size_t>(n) >= size) {
        buf[0] = '\0';
        return 0;
    }
    return static_cast<std::size_t>(n);
}
