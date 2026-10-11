#pragma once

#include <ArduinoJson.h>

namespace weather {
enum class ResponseKind { Conditions, Air, Position };
constexpr size_t PROVIDER_JSON_CAPACITY = 1024;
constexpr size_t FILTER_JSON_CAPACITY = JSON_OBJECT_SIZE(20) + JSON_ARRAY_SIZE(1);

inline void responseFilter(JsonDocument &filter, ResponseKind kind)
{
    filter.clear();
    filter["error"] = true;
    if (kind == ResponseKind::Conditions) {
        filter["main"]["temp"] = true;
        filter["main"]["humidity"] = true;
        filter["clouds"]["all"] = true;
        filter["wind"]["speed"] = true;
        filter["wind"]["deg"] = true;
        filter["weather"][0]["id"] = true;
        // Keep the whole rain value so null/empty/wrong-type fields stay detectable.
        filter["rain"] = true;
        filter["dt"] = true;
        filter["timezone"] = true;
        filter["cod"] = true;
    } else if (kind == ResponseKind::Air) {
        filter["list"][0]["components"]["pm2_5"] = true;
        filter["list"][0]["dt"] = true;
    } else {
        filter["success"] = true;
        filter["latitude"] = true;
        filter["longitude"] = true;
        filter["city"] = true;
        filter["timezone"]["offset"] = true;
        filter["timezone"]["id"] = true;
    }
}
}
