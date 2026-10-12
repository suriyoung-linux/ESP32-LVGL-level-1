#pragma once

#include <SPI.h>
#include "dht_sensor.h"
#include "weather_data.h"

class WeatherService;

class DataLogger {
public:
    enum class Card { Missing, Ready, Error };
    static constexpr uint32_t INTERVAL_MS = 60000;
    static constexpr uint32_t CHECK_MS = 5000;
    static constexpr const char *PATH = "/weather_log.csv";

    void begin(SPIClass &sharedTouchSpi);
    bool checkCard(uint32_t now);
    bool setEnabled(bool enabled, uint32_t now);
    void poll(uint32_t now, const WeatherService &weather,
              const dhtsensor::Reading &dht, bool settingsVisible, bool connected);
    Card card() const { return card_; }
    bool enabled() const { return enabled_; }
    uint32_t revision() const { return revision_; }

private:
    SPIClass *spi_ = nullptr;
    Card card_ = Card::Missing;
    bool mounted_ = false, enabled_ = false, writeDue_ = false;
    uint32_t lastCheckAt_ = 0, lastWriteAt_ = 0, revision_ = 0;
    void fail(Card card);
    bool append(uint32_t now, const WeatherService &weather,
                const dhtsensor::Reading &dht, bool connected);
};
