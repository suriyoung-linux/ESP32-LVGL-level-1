#pragma once
#include <cstdint>
#include <cmath>

namespace ldr {
struct Reading {
    uint16_t raw = 0;
    uint32_t millivolts = 0;
    float resistanceOhms = NAN;
    uint32_t receivedAtMs = 0;
    const char *ledName = "STOPPED";
    bool valid = false;
};
}
