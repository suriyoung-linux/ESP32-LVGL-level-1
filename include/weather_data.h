#pragma once

#include <ArduinoJson.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace weather {
struct Position {
    double latitude = NAN;
    double longitude = NAN;
    int32_t utcOffsetSeconds = 0;
    char timezone[48] = "";
    char city[48] = "";
    bool approximate = false;
};

struct Conditions {
    bool valid = false;
    float temperature = NAN;
    float humidity = NAN;
    float rain = NAN;
    float clouds = NAN;
    float windSpeed = NAN;
    float windDirection = NAN;
    int code = -1;
    uint32_t timestamp = 0;
    uint32_t receivedAtMs = 0;
};

struct AirQuality {
    bool valid = false;
    float pm25 = NAN;
    uint32_t timestamp = 0;
    uint32_t receivedAtMs = 0;
};

struct Snapshot {
    Position position;
    Conditions conditions;
    AirQuality air;
    uint32_t revision = 0;
    bool pending = false;
    bool weatherFailed = false;
    bool airFailed = false;
    bool attempted = false;
    uint32_t lastAttemptAtMs = 0;
    char error[96] = "";
};

inline bool validPosition(const Position &p)
{
    return std::isfinite(p.latitude) && std::isfinite(p.longitude) &&
           p.latitude >= -90 && p.latitude <= 90 && p.longitude >= -180 && p.longitude <= 180;
}

inline bool number(JsonVariantConst value)
{
    return value.is<double>() && std::isfinite(value.as<double>());
}

inline bool parsePosition(JsonObjectConst root, Position &out)
{
    if (!root["success"].is<bool>() || !root["success"].as<bool>() ||
        !number(root["latitude"]) || !number(root["longitude"]) ||
        !root["timezone"]["offset"].is<int32_t>() ||
        !root["timezone"]["id"].is<const char *>()) return false;
    Position p;
    p.latitude = root["latitude"].as<double>();
    p.longitude = root["longitude"].as<double>();
    p.utcOffsetSeconds = root["timezone"]["offset"].as<int32_t>();
    snprintf(p.timezone, sizeof(p.timezone), "%s", root["timezone"]["id"].as<const char *>());
    snprintf(p.city, sizeof(p.city), "%s", root["city"] | "WiFi/IP area");
    p.approximate = true;
    if (!validPosition(p) || p.utcOffsetSeconds < -14 * 3600 || p.utcOffsetSeconds > 14 * 3600) return false;
    out = p;
    return true;
}

inline bool parseConditions(JsonObjectConst root, Conditions &out, Position &position)
{
    if (!number(root["main"]["temp"]) || !number(root["main"]["humidity"]) ||
        !number(root["clouds"]["all"]) || !number(root["wind"]["speed"]) ||
        !root["weather"][0]["id"].is<int>() || !root["dt"].is<uint32_t>() ||
        !root["timezone"].is<int32_t>() || !root["cod"].is<int>() || root["cod"].as<int>() != 200)
        return false;
    Conditions data;
    data.temperature = root["main"]["temp"];
    data.humidity = root["main"]["humidity"];
    data.clouds = root["clouds"]["all"];
    data.windSpeed = root["wind"]["speed"].as<float>() * 3.6f;  // metric m/s -> km/h
    // OpenWeather omits rain when the phenomenon is absent; malformed rain is an error.
    data.rain = 0;
    if (root.containsKey("rain")) {
        if (!number(root["rain"]["1h"])) return false;
        data.rain = root["rain"]["1h"];  // mm/h, not daily accumulation
    }
    if (root["wind"].as<JsonObjectConst>().containsKey("deg")) {
        if (!number(root["wind"]["deg"])) return false;
        data.windDirection = root["wind"]["deg"];
        if (!std::isfinite(data.windDirection) || data.windDirection < 0 || data.windDirection > 360)
            return false;
    }  // Missing wind direction stays NAN rather than inventing north.
    data.code = root["weather"][0]["id"];
    data.timestamp = root["dt"];
    const int32_t offset = root["timezone"];
    if (!std::isfinite(data.temperature) || !std::isfinite(data.rain) || !std::isfinite(data.windSpeed) ||
        data.humidity < 0 || data.humidity > 100 || data.clouds < 0 || data.clouds > 100 ||
        data.rain < 0 || data.windSpeed < 0 || data.timestamp < 1704067200 ||
        offset < -14 * 3600 || offset > 14 * 3600) return false;
    position.utcOffsetSeconds = offset;
    data.valid = true;
    out = data;
    return true;
}

inline bool parseAir(JsonObjectConst root, AirQuality &out)
{
    JsonObjectConst c = root["list"][0];
    if (!number(c["components"]["pm2_5"]) || !std::isfinite(c["components"]["pm2_5"].as<float>()) ||
        c["components"]["pm2_5"].as<float>() < 0 || !c["dt"].is<uint32_t>() ||
        c["dt"].as<uint32_t>() < 1704067200) return false;
    AirQuality data;
    data.pm25 = c["components"]["pm2_5"];
    data.timestamp = c["dt"];
    // Current Air Pollution does not report an averaging interval.
    data.valid = true;
    out = data;
    return true;
}

inline const char *conditionName(int code)
{
    if (code >= 200 && code <= 232) return "Thunderstorm";
    if (code >= 300 && code <= 321) return "Drizzle";
    if (code >= 600 && code <= 622) return "Snow";
    switch (code) {
        case 500: return "Light rain";
        case 501: return "Moderate rain";
        case 502: return "Heavy rain";
        case 503: return "Very heavy rain";
        case 504: return "Extreme rain";
        case 511: return "Freezing rain";
        case 520: case 521: case 522: case 531: return "Rain showers";
        case 701: return "Mist";
        case 711: return "Smoke";
        case 721: return "Haze";
        case 731: case 761: return "Dust";
        case 741: return "Fog";
        case 751: return "Sand";
        case 762: return "Volcanic ash";
        case 771: return "Squalls";
        case 781: return "Tornado";
        case 800: return "Clear sky";
        case 801: return "Few clouds";
        case 802: return "Scattered clouds";
        case 803: return "Broken clouds";
        case 804: return "Overcast";
        default: return "Unknown condition";
    }
}

inline const char *windCompass(float degrees)
{
    if (!std::isfinite(degrees) || degrees < 0 || degrees > 360) return "N/A";
    static const char *const names[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                       "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    return names[static_cast<unsigned int>((degrees + 11.25f) / 22.5f) % 16];
}
}
