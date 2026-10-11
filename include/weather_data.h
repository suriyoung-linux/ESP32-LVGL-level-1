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
    uint32_t intervalSeconds = 0;
    uint32_t receivedAtMs = 0;
};

struct AirQuality {
    bool valid = false;
    float pm25 = NAN;
    uint32_t timestamp = 0;
    uint32_t intervalSeconds = 0;
    uint32_t receivedAtMs = 0;
};

struct Snapshot {
    Position position;
    Conditions conditions;
    AirQuality air;
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
    JsonObjectConst c = root["current"];
    for (const char *key : {"temperature_2m", "relative_humidity_2m", "rain", "showers",
                            "cloud_cover", "wind_speed_10m", "wind_direction_10m"}) {
        if (!number(c[key])) return false;
    }
    if (!c["weather_code"].is<int>() || !c["time"].is<uint32_t>() ||
        !c["interval"].is<uint32_t>() || c["interval"].as<uint32_t>() == 0 ||
        !root["utc_offset_seconds"].is<int32_t>() || !root["timezone"].is<const char *>()) return false;
    Conditions data;
    data.temperature = c["temperature_2m"];
    data.humidity = c["relative_humidity_2m"];
    data.rain = c["rain"].as<float>() + c["showers"].as<float>();
    data.clouds = c["cloud_cover"];
    data.windSpeed = c["wind_speed_10m"];
    data.windDirection = c["wind_direction_10m"];
    data.code = c["weather_code"];
    data.timestamp = c["time"];
    data.intervalSeconds = c["interval"];
    if (data.humidity < 0 || data.humidity > 100 || data.clouds < 0 || data.clouds > 100 ||
        data.rain < 0 || data.windSpeed < 0 || data.windDirection < 0 || data.windDirection > 360 ||
        data.timestamp < 1704067200 || !std::isfinite(data.rain) || !std::isfinite(data.temperature) ||
        !std::isfinite(data.windSpeed) || root["utc_offset_seconds"].as<int32_t>() < -14 * 3600 ||
        root["utc_offset_seconds"].as<int32_t>() > 14 * 3600) return false;
    position.utcOffsetSeconds = root["utc_offset_seconds"];
    snprintf(position.timezone, sizeof(position.timezone), "%s", root["timezone"].as<const char *>());
    data.valid = true;
    out = data;
    return true;
}

inline bool parseAir(JsonObjectConst root, AirQuality &out)
{
    JsonObjectConst c = root["current"];
    if (!number(c["pm2_5"]) || !std::isfinite(c["pm2_5"].as<float>()) || c["pm2_5"].as<float>() < 0 || !c["time"].is<uint32_t>() ||
        !c["interval"].is<uint32_t>() || c["interval"].as<uint32_t>() == 0 ||
        c["time"].as<uint32_t>() < 1704067200) return false;
    AirQuality data;
    data.pm25 = c["pm2_5"];
    data.timestamp = c["time"];
    data.intervalSeconds = c["interval"];
    data.valid = true;
    out = data;
    return true;
}

inline const char *conditionName(int code)
{
    switch (code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Fog";
        case 51: return "Light drizzle";
        case 53: return "Moderate drizzle";
        case 55: return "Dense drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Moderate rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: case 73: case 75: case 77: return "Snow";
        case 80: return "Light showers";
        case 81: return "Moderate showers";
        case 82: return "Heavy showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 97: return "Heavy thunderstorm";
        case 96: case 99: return "Thunderstorm with hail";
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
