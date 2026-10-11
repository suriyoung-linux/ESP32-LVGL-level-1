#include "weather_service.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctime>
#include "api_root_certificates.h"

namespace {
bool fetchJson(const String &url, const char *ca, DynamicJsonDocument &doc, char *error, size_t errorSize)
{
    WiFiClientSecure client;
    client.setCACert(ca);
    client.setHandshakeTimeout(8);
    HTTPClient http;
    http.setConnectTimeout(weather::HTTP_TIMEOUT_MS);
    http.setTimeout(weather::HTTP_TIMEOUT_MS);
    http.setReuse(false);
    http.useHTTP10(true);  // ไม่รับ chunked encoding เพื่ออ่าน JSON จาก stream โดยตรง
    if (!http.begin(client, url)) {
        snprintf(error, errorSize, "Unable to start HTTPS");
        return false;
    }
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        snprintf(error, errorSize, "HTTP %d", status);
        http.end();
        return false;
    }
    const DeserializationError result = deserializeJson(doc, http.getStream());
    http.end();
    if (result || doc["error"].as<bool>()) {
        snprintf(error, errorSize, "JSON: %s", result ? result.c_str() : "provider error");
        return false;
    }
    return true;
}

String coordinates(const weather::Position &p)
{
    return "latitude=" + String(p.latitude, 6) + "&longitude=" + String(p.longitude, 6);
}
}

const char *WeatherService::locationId(uint8_t location)
{
    static const char *const ids[] = {"chiang_mai", "bangkok", "home"};
    return location < weather::LOCATION_COUNT ? ids[location] : "unknown";
}

const char *WeatherService::locationName(uint8_t location)
{
    static const char *const names[] = {"Chiang Mai", "Bangkok", "Home (WiFi/IP approx.)"};
    return location < weather::LOCATION_COUNT ? names[location] : "Unknown";
}

bool WeatherService::begin()
{
    cache_[0].position.latitude = 18.7883;
    cache_[0].position.longitude = 98.9853;
    cache_[1].position.latitude = 13.7563;
    cache_[1].position.longitude = 100.5018;
    for (uint8_t i = 0; i < 2; ++i) {
        cache_[i].position.utcOffsetSeconds = 25200;
        snprintf(cache_[i].position.timezone, sizeof(cache_[i].position.timezone), "Asia/Bangkok");
    }
    cache_[2].position.approximate = true;
    jobs_ = xQueueCreate(weather::LOCATION_COUNT, sizeof(Job));
    results_ = xQueueCreate(weather::LOCATION_COUNT, sizeof(Result));
    if (!jobs_ || !results_ || xTaskCreate(worker, "weather-api", 12288, this, 1, nullptr) != pdPASS) {
        if (jobs_) vQueueDelete(jobs_);
        if (results_) vQueueDelete(results_);
        jobs_ = results_ = nullptr;
        return false;
    }
    return true;
}

bool WeatherService::request(uint8_t location, bool force)
{
    if (!ready() || location >= weather::LOCATION_COUNT || WiFi.status() != WL_CONNECTED ||
        time(nullptr) < 1704067200) return false;  // ต้องมีเวลาจริงเพื่อยืนยัน certificate
    auto &data = cache_[location];
    if (data.pending) return false;
    const uint32_t now = millis();
    const uint32_t interval = force ? weather::MIN_REQUEST_INTERVAL_MS
                            : (!data.conditions.valid || !data.air.valid || data.weatherFailed || data.airFailed)
                              ? weather::RETRY_INTERVAL_MS : weather::REFRESH_INTERVAL_MS;
    if (data.attempted && now - data.lastAttemptAtMs < interval) return false;
    const bool resolve = location == 2 && (!weather::validPosition(data.position) ||
                           now - homeResolvedAtMs_ >= weather::GEO_REFRESH_INTERVAL_MS);
    const Job job{location, data.position, resolve, homeGeneration_};
    if (xQueueSend(jobs_, &job, 0) != pdPASS) return false;
    data.pending = true;
    data.attempted = true;
    data.lastAttemptAtMs = now;
    return true;
}

void WeatherService::invalidateHome()
{
    ++homeGeneration_;  // ไม่รับผลของตำแหน่งจากการเชื่อมต่อเครือข่ายเดิม
    cache_[2] = weather::Snapshot{};
    cache_[2].position.approximate = true;
    homeResolvedAtMs_ = 0;
}

void WeatherService::poll()
{
    if (!ready()) return;
    Result result;
    while (xQueueReceive(results_, &result, 0) == pdPASS) {
        if (result.location == 2 && result.generation != homeGeneration_) continue;
        auto &previous = cache_[result.location];
        const auto &fresh = result.snapshot;
        if (weather::validPosition(fresh.position)) {
            if (result.location == 2 && weather::validPosition(previous.position) &&
                (std::abs(previous.position.latitude - fresh.position.latitude) > 0.00001 ||
                 std::abs(previous.position.longitude - fresh.position.longitude) > 0.00001)) {
                previous.conditions = weather::Conditions{};
                previous.air = weather::AirQuality{};
            }
            previous.position = fresh.position;
        }
        if (result.location == 2 && result.positionResolved) {
            homeResolvedAtMs_ = millis();
        }
        if (fresh.conditions.valid) previous.conditions = fresh.conditions;
        if (fresh.air.valid) previous.air = fresh.air;
        previous.pending = false;
        previous.weatherFailed = fresh.weatherFailed;
        previous.airFailed = fresh.airFailed;
        snprintf(previous.error, sizeof(previous.error), "%s", fresh.error);
        Serial.printf("Weather API [%s]: weather=%s, PM2.5=%s%s%s\n", locationId(result.location),
                      fresh.conditions.valid ? "OK" : "unavailable", fresh.air.valid ? "OK" : "unavailable",
                      fresh.error[0] ? ", " : "", fresh.error);
    }
}

void WeatherService::worker(void *context)
{
    auto *self = static_cast<WeatherService *>(context);
    Job job;
    for (;;) {
        if (xQueueReceive(self->jobs_, &job, portMAX_DELAY) != pdPASS) continue;
        Result result{};
        result.location = job.location;
        result.generation = job.generation;
        auto &data = result.snapshot;
        data.position = job.position;
        DynamicJsonDocument json(4096);
        if (WiFi.status() != WL_CONNECTED) {
            snprintf(data.error, sizeof(data.error), "WiFi unavailable");
        } else if (job.resolvePosition &&
                   (!fetchJson("https://ipwho.is/?fields=success,latitude,longitude,city,timezone", GEO_ROOT_CA,
                               json, data.error, sizeof(data.error)) ||
                    !weather::parsePosition(json.as<JsonObjectConst>(), data.position))) {
            if (!data.error[0]) snprintf(data.error, sizeof(data.error), "WiFi/IP position unavailable");
        } else if (weather::validPosition(data.position)) {
            result.positionResolved = job.resolvePosition;
            json.clear();
            const String params = coordinates(data.position);
            const String url = "https://api.open-meteo.com/v1/forecast?" + params +
                "&current=temperature_2m,relative_humidity_2m,weather_code,rain,showers,cloud_cover,wind_speed_10m,wind_direction_10m"
                "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm&timezone=auto&timeformat=unixtime&forecast_days=1";
            if (fetchJson(url, WEATHER_ROOT_CA, json, data.error, sizeof(data.error))) {
                if (!weather::parseConditions(json.as<JsonObjectConst>(), data.conditions, data.position)) {
                    snprintf(data.error, sizeof(data.error), "Weather fields unavailable");
                } else {
                    data.conditions.receivedAtMs = millis();
                }
            }
            json.clear();
            char airError[64] = "";
            if (fetchJson("https://air-quality-api.open-meteo.com/v1/air-quality?" + params +
                          "&current=pm2_5&timezone=auto&timeformat=unixtime&forecast_days=1",
                          WEATHER_ROOT_CA, json, airError, sizeof(airError))) {
                if (!weather::parseAir(json.as<JsonObjectConst>(), data.air)) {
                    snprintf(airError, sizeof(airError), "PM2.5 fields unavailable");
                } else {
                    data.air.receivedAtMs = millis();
                }
            }
            if (airError[0]) {
                const size_t used = strlen(data.error);
                snprintf(data.error + used, sizeof(data.error) - used, "%s%s", used ? "; " : "", airError);
            }
        } else {
            snprintf(data.error, sizeof(data.error), "Position unavailable");
        }
        data.weatherFailed = !data.conditions.valid;
        data.airFailed = !data.air.valid;
        xQueueSend(self->results_, &result, portMAX_DELAY);
    }
}
