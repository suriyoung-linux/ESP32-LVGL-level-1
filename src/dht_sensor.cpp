#include "dht_sensor.h"

static_assert(app::DHT_SENSOR_MODEL == DHT11 || app::DHT_SENSOR_MODEL == DHT22,
              "Select DHT11 (11) or DHT22 (22)");
static_assert(app::DHT_READ_INTERVAL_MS >= 2000, "DHT reads must be at least 2 seconds apart");

void DhtSensor::begin(uint32_t now)
{
    driver_.begin();
    reading_ = dhtsensor::Reading{};
    lastAttemptAt_ = now;  // Allow the sensor to settle after power-on.
    started_ = true;
}

bool DhtSensor::poll(uint32_t now)
{
    if (!started_ || now - lastAttemptAt_ < app::DHT_READ_INTERVAL_MS) return false;
    // Force one physical read; temperature reuses that same humidity transaction.
    const float humidity = driver_.readHumidity(true);
    const float temperature = driver_.readTemperature();
    lastAttemptAt_ = millis();
    reading_.attemptedAtMs = lastAttemptAt_;
    const bool valid = std::isfinite(temperature) && std::isfinite(humidity) &&
                       temperature >= app::DHT_MIN_TEMPERATURE && temperature <= app::DHT_MAX_TEMPERATURE &&
                       humidity >= app::DHT_MIN_HUMIDITY && humidity <= app::DHT_MAX_HUMIDITY;
    reading_.status = valid ? dhtsensor::Status::Ready : dhtsensor::Status::Unavailable;
    reading_.temperature = valid ? temperature : NAN;
    reading_.humidity = valid ? humidity : NAN;
    if (valid) {
        Serial.printf("DHT%u GPIO%u: temperature=%.1f C, humidity=%.1f %%\n",
                      app::DHT_SENSOR_MODEL, cyd::SAFE_SENSOR_PIN, temperature, humidity);
    } else {
        Serial.printf("DHT%u GPIO%u: no sensor connected or sensor read error; retry in %lu ms\n",
                      app::DHT_SENSOR_MODEL, cyd::SAFE_SENSOR_PIN,
                      static_cast<unsigned long>(app::DHT_READ_INTERVAL_MS));
    }
    return true;
}
