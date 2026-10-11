#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "weather_config.h"
#include "weather_data.h"

class WeatherService {
public:
    bool begin();
    bool request(uint8_t location, bool force = false);
    void poll();
    void invalidateHome();
    const weather::Snapshot &get(uint8_t location) const { return cache_[location]; }
    static const char *locationId(uint8_t location);
    static const char *locationName(uint8_t location);
    bool ready() const { return jobs_ != nullptr && results_ != nullptr; }

private:
    struct Job { uint8_t location; weather::Position position; bool resolvePosition; uint32_t generation; };
    struct Result { uint8_t location; weather::Snapshot snapshot; uint32_t generation; bool positionResolved; };
    weather::Snapshot cache_[weather::LOCATION_COUNT];
    QueueHandle_t jobs_ = nullptr;
    QueueHandle_t results_ = nullptr;
    uint32_t homeResolvedAtMs_ = 0;
    uint32_t homeGeneration_ = 0;
    static void worker(void *context);
};
