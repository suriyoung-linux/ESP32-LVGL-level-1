#pragma once

#include <DHT.h>
#include <cmath>
#include "app_config.h"
#include "board_config.h"

#include "dht_reading.h"

class DhtSensor {
public:
    void begin(uint32_t now);
    bool poll(uint32_t now);
    const dhtsensor::Reading &get() const { return reading_; }

private:
    DHT driver_{cyd::SAFE_SENSOR_PIN, app::DHT_SENSOR_MODEL};
    dhtsensor::Reading reading_;
    uint32_t lastAttemptAt_ = 0;
    bool started_ = false;
};
