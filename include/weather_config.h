#pragma once

#include <cstdint>

namespace weather {
constexpr uint8_t LOCATION_COUNT = 3;
constexpr uint32_t REFRESH_INTERVAL_MS = 10 * 60 * 1000;
constexpr uint32_t RETRY_INTERVAL_MS = 5 * 60 * 1000;
constexpr uint32_t MIN_REQUEST_INTERVAL_MS = 30 * 1000;
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;
constexpr uint32_t GEO_REFRESH_INTERVAL_MS = 60 * 60 * 1000;
constexpr uint16_t API_PORT = 80;
constexpr uint32_t SCHEDULE_INTERVAL_MS = 250;
constexpr uint32_t WORKER_STACK_BYTES = 12288;
}
