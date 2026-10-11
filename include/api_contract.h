#pragma once

#include <ArduinoJson.h>
#include <cstddef>

inline bool parseRgbCommand(const char *body, size_t length, bool &enabled)
{
    if (length == 0 || length > 128) return false;
    StaticJsonDocument<128> json;
    if (deserializeJson(json, body, length) || !json.is<JsonObject>() || !json["enabled"].is<bool>()) return false;
    enabled = json["enabled"].as<bool>();
    return true;
}
