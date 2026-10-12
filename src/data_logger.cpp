#include "data_logger.h"

#include <SD.h>
#include <ctime>
#include "board_config.h"
#include "weather_service.h"

namespace {
// Touch and SD share VSPI but have different wiring. All calls run on loop(),
// with files closed before restoring touch. Never create a second VSPI instance.
class SdBus {
public:
    explicit SdBus(SPIClass &spi) : spi_(spi) {
        digitalWrite(cyd::TOUCH_PIN_CS, HIGH);
        digitalWrite(cyd::SD_PIN_CS, HIGH);
        spi_.end();
        spi_.begin(cyd::SD_PIN_CLK, cyd::SD_PIN_MISO, cyd::SD_PIN_MOSI, cyd::SD_PIN_CS);
    }
    ~SdBus() {
        digitalWrite(cyd::SD_PIN_CS, HIGH);
        spi_.end();
        spi_.begin(cyd::TOUCH_PIN_CLK, cyd::TOUCH_PIN_MISO, cyd::TOUCH_PIN_MOSI, cyd::TOUCH_PIN_CS);
    }
private:
    SPIClass &spi_;
};

constexpr const char *HEADER =
    "logged_at_utc,uptime_ms,location,weather_observed_utc,weather_status,"
    "weather_temp_c,weather_humidity_pct,rain_mm_h,clouds_pct,wind_m_s,"
    "wind_direction_deg,weather_code,dht_model,dht_status,dht_temp_c,"
    "dht_humidity_pct,dht_age_ms\n";

void utcText(char *text, size_t size, time_t epoch)
{
    text[0] = '\0';
    if (epoch < 1704067200) return; // Keep blank until NTP has set the clock.
    tm fields;
    gmtime_r(&epoch, &fields);
    strftime(text, size, "%Y-%m-%dT%H:%M:%SZ", &fields);
}

void numberText(char *text, size_t size, float value, bool valid)
{
    text[0] = '\0';
    if (valid && std::isfinite(value)) snprintf(text, size, "%.2f", value);
}
}

void DataLogger::begin(SPIClass &sharedTouchSpi)
{
    spi_ = &sharedTouchSpi;
    pinMode(cyd::SD_PIN_CS, OUTPUT);
    digitalWrite(cyd::SD_PIN_CS, HIGH);
    checkCard(millis());
}

void DataLogger::fail(Card card)
{
    SD.end();
    mounted_ = false;
    enabled_ = false;
    writeDue_ = false;
    card_ = card;
    ++revision_;
    Serial.println(card == Card::Missing ? "microSD not found / cannot mount" : "microSD read/write error; logging stopped");
}

bool DataLogger::checkCard(uint32_t now)
{
    if (!spi_) return false;
    lastCheckAt_ = now;
    SdBus bus(*spi_);
    if (!mounted_) mounted_ = SD.begin(cyd::SD_PIN_CS, *spi_, 4000000, "/sd", 1, false);
    uint8_t sector[512];
    // cardType() is cached; a real sector read also detects removal.
    if (!mounted_ || SD.cardType() == CARD_NONE || !SD.readRAW(sector, 0)) {
        if (card_ != Card::Missing || enabled_ || mounted_) fail(Card::Missing);
        else SD.end();
        return false;
    }
    if (card_ != Card::Ready) { card_ = Card::Ready; ++revision_; }
    return true;
}

bool DataLogger::setEnabled(bool enabled, uint32_t now)
{
    if (enabled_ == enabled) return true;
    if (enabled && !checkCard(now)) return false;
    enabled_ = enabled;
    writeDue_ = enabled;
    ++revision_;
    return true;
}

bool DataLogger::append(uint32_t now, const WeatherService &weather,
                        const dhtsensor::Reading &dht, bool connected)
{
    SdBus bus(*spi_);
    // The loop's timestamp can predate the latest blocking DHT read/probe.
    now = millis();
    File file = SD.open(PATH, FILE_APPEND);
    if (!file) { fail(Card::Error); return false; }
    size_t expectedSize = file.size();
    bool ok = true;
    if (expectedSize == 0) {
        expectedSize = strlen(HEADER);
        ok = file.print(HEADER) == expectedSize;
    }
    char logged[24];
    utcText(logged, sizeof(logged), time(nullptr));
    const bool dhtValid = dht.status == dhtsensor::Status::Ready;
    const char *dhtStatus = dhtValid ? "ok" : dht.status == dhtsensor::Status::Waiting ? "waiting" : "unavailable";
    char dhtTemp[20], dhtHumidity[20], dhtAge[16] = "";
    numberText(dhtTemp, sizeof(dhtTemp), dht.temperature, dhtValid);
    numberText(dhtHumidity, sizeof(dhtHumidity), dht.humidity, dhtValid);
    if (dhtValid) snprintf(dhtAge, sizeof(dhtAge), "%lu", static_cast<unsigned long>(now - dht.attemptedAtMs));
    for (uint8_t i = 0; ok && i < weather::LOCATION_COUNT; ++i) {
        const auto &snapshot = weather.get(i);
        const auto &c = snapshot.conditions;
        const bool stale = c.valid && (!connected || snapshot.weatherFailed || now - c.receivedAtMs > weather::REFRESH_INTERVAL_MS + 60000);
        char observed[24] = "", values[6][20], code[16] = "", row[512];
        if (c.valid) utcText(observed, sizeof(observed), c.timestamp);
        const float fields[] = {c.temperature, c.humidity, c.rain, c.clouds, c.windSpeed, c.windDirection};
        for (uint8_t f = 0; f < 6; ++f) numberText(values[f], sizeof(values[f]), fields[f], c.valid);
        if (c.valid && c.code >= 0) snprintf(code, sizeof(code), "%d", c.code);
        const int length = snprintf(row, sizeof(row), "%s,%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,DHT%u,%s,%s,%s,%s\n",
            logged, static_cast<unsigned long>(now), WeatherService::locationId(i), observed,
            c.valid ? (stale ? "stale" : "ok") : "unavailable",
            values[0], values[1], values[2], values[3], values[4], values[5], code,
            app::DHT_SENSOR_MODEL, dhtStatus, dhtTemp, dhtHumidity, dhtAge);
        ok = length > 0 && static_cast<size_t>(length) < sizeof(row) &&
             file.print(row) == static_cast<size_t>(length);
        if (ok) expectedSize += static_cast<size_t>(length);
    }
    file.flush();
    ok = ok && file.getWriteError() == 0 && file.size() == expectedSize;
    file.close();
    if (!ok) { fail(Card::Error); return false; }
    return true;
}

void DataLogger::poll(uint32_t now, const WeatherService &weather,
                      const dhtsensor::Reading &dht, bool settingsVisible, bool connected)
{
    if (!spi_) return;
    // Keep write errors visible until the user explicitly checks the card.
    if (card_ != Card::Error && (settingsVisible || enabled_) && now - lastCheckAt_ >= CHECK_MS) checkCard(now);
    if (!enabled_ || (!writeDue_ && now - lastWriteAt_ < INTERVAL_MS)) return;
    if (append(now, weather, dht, connected)) {
        lastWriteAt_ = now;
        writeDue_ = false;
    }
}
