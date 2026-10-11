#pragma once

#include <ArduinoJson.h>
#include <cstddef>

namespace api {
// Slots for all nested objects plus copied city/timezone/time/error strings.
constexpr size_t SENSORS_JSON_CAPACITY = JSON_OBJECT_SIZE(3 + 7 + 6 + 3);
constexpr size_t WEATHER_JSON_CAPACITY = JSON_OBJECT_SIZE(42) + 256;
constexpr size_t LOCATIONS_JSON_CAPACITY = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(3) +
                                         3 * (JSON_OBJECT_SIZE(12) + 128);
}

inline bool parseRgbCommand(const char *body, size_t length, bool &enabled)
{
    if (length == 0 || length > 128) return false;
    StaticJsonDocument<128> json;
    if (deserializeJson(json, body, length) || !json.is<JsonObject>() || !json["enabled"].is<bool>()) return false;
    enabled = json["enabled"].as<bool>();
    return true;
}
