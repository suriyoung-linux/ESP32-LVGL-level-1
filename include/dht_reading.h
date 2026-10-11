#pragma once

#include <cstdint>
#include <cmath>

namespace dhtsensor {
enum class Status { Waiting, Ready, Unavailable };

struct Reading {
    Status status = Status::Waiting;
    float temperature = NAN;
    float humidity = NAN;
    uint32_t attemptedAtMs = 0;
};
}
