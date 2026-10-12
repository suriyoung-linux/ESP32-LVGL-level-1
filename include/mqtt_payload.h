#pragma once

#include <ArduinoJson.h>
#include <cmath>
#include "app_config.h"
#include "dht_reading.h"

namespace mqttdata {
inline bool dhtValid(const dhtsensor::Reading &reading, uint32_t now)
{
    return reading.status == dhtsensor::Status::Ready &&
           now - reading.attemptedAtMs <= app::DHT_READ_INTERVAL_MS + 1000 &&
           std::isfinite(reading.temperature) && std::isfinite(reading.humidity);
}

// Only the explicitly requested temperature/humidity and reading quality leave
// the device. Connection diagnostics remain local to the TFT/Serial interface.
inline void payload(JsonDocument &json, uint32_t now, const dhtsensor::Reading &dht)
{
    json.clear();
    const bool valid = dhtValid(dht, now);
    json["valid"] = valid;
    json["status"] = valid ? "ready" : dht.status == dhtsensor::Status::Waiting ? "waiting" :
                     dht.status == dhtsensor::Status::Ready ? "stale" : "unavailable";
    if (valid) {
        json["temperature_c"] = dht.temperature;
        json["humidity_percent"] = dht.humidity;
    } else {
        json["temperature_c"] = nullptr;
        json["humidity_percent"] = nullptr;
    }
}
}
