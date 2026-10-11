#include "dashboard.h"

#include <ctime>
#include <cstring>

namespace {
constexpr uint16_t BACKGROUND = TFT_BLACK;
constexpr uint16_t CARD = 0x18C3;
constexpr uint16_t INACTIVE = 0x4208;
// Share the row positions between rendering and touch hit testing.
constexpr int16_t HEADER_HEIGHT = 20;
constexpr int16_t PAGE_TAB_Y = 22, PAGE_TAB_HEIGHT = 20;
constexpr int16_t PAGE_TAB_X = 6, PAGE_TAB_WIDTH = 100, PAGE_TAB_STEP = 105;
constexpr int16_t LOCATION_TAB_Y = 46, LOCATION_TAB_HEIGHT = 24;
constexpr int16_t GROUP_TAB_Y = 74, GROUP_TAB_HEIGHT = 22;
constexpr int16_t WEATHER_CONTENT_Y = 100;
constexpr int16_t REFRESH_X = 267, REFRESH_Y = 220;
constexpr int16_t REFRESH_WIDTH = 50, REFRESH_HEIGHT = 19;
const touchcal::Point targets[] = {{20, 20}, {300, 20}, {20, 220}, {300, 220}};

void localTimeText(char *text, size_t size, time_t epoch, int32_t offset, const char *format)
{
    if (epoch < 1704067200) {
        snprintf(text, size, "--");
        return;
    }
    const time_t local = epoch + offset;
    tm fields;
    gmtime_r(&local, &fields);
    strftime(text, size, format, &fields);
}
}

void Dashboard::label(const char *text, int16_t x, int16_t y, uint16_t color, uint8_t font)
{
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(color, BACKGROUND);
    tft_.drawString(text, x, y, font);
}

void Dashboard::weatherLabel(const char *text, int16_t y, uint16_t color)
{
    // Reuse Font 2 at double size instead of storing another full glyph table.
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextSize(2);
    if (tft_.textWidth(text, 2) > 304) tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(color, BACKGROUND);
    tft_.drawString(text, 8, y, 2);
}

void Dashboard::button(int16_t x, int16_t y, int16_t width, int16_t height, const char *text, bool active)
{
    const uint16_t background = active ? TFT_DARKCYAN : CARD;
    tft_.fillRoundRect(x, y, width, height, 4, background);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(TFT_WHITE, background);
    tft_.drawString(text, x + width / 2, y + height / 2, height < 22 ? 1 : 2);
}

void Dashboard::begin()
{
    tft_.init();
    tft_.setRotation(cyd::DISPLAY_ROTATION);
    cyd::enableBacklight();
    touchSpi_.begin(cyd::TOUCH_PIN_CLK, cyd::TOUCH_PIN_MISO, cyd::TOUCH_PIN_MOSI, cyd::TOUCH_PIN_CS);
    touch_.begin(touchSpi_);
    touch_.setRotation(1);
    preferencesReady_ = preferences_.begin("weather-ui", false);
    if (preferencesReady_) {
        rgbEnabled_ = preferences_.getBool("rgb", true);
        if (preferences_.getBytesLength("cal") == sizeof(calibration_)) {
            preferences_.getBytes("cal", &calibration_, sizeof(calibration_));
        }
    }
    if (!calibration_.valid()) beginCalibration();
}

void Dashboard::setNetwork(const char *status, const IPAddress &ip, bool connected)
{
    snprintf(networkStatus_, sizeof(networkStatus_), "%s", status);
    connected_ = connected;
    if (connected) {
        snprintf(ipv4_, sizeof(ipv4_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    } else {
        snprintf(ipv4_, sizeof(ipv4_), "--");
    }
    layoutDirty_ = true;
}

void Dashboard::setRgbEnabled(bool enabled)
{
    if (rgbEnabled_ == enabled) return;
    rgbEnabled_ = enabled;
    if (preferencesReady_) preferences_.putBool("rgb", enabled);
    layoutDirty_ = true;
}

void Dashboard::updateLdr(uint16_t raw, uint32_t millivolts, const char *resistance, uint16_t color)
{
    if (raw_ == raw && millivolts_ == millivolts && ledColor_ == color &&
        strcmp(resistance_, resistance) == 0) return;
    raw_ = raw;
    millivolts_ = millivolts;
    snprintf(resistance_, sizeof(resistance_), "%s", resistance);
    ledColor_ = color;
    if (page_ == Page::Rgb) dataDirty_ = true;
}

void Dashboard::drawHeader()
{
    tft_.fillRect(0, 0, 320, HEADER_HEIGHT, BACKGROUND);
    label(networkStatus_, 5, 10, connected_ ? TFT_GREEN : TFT_YELLOW, 1);
    label(ipv4_, 166, 10, TFT_WHITE, 1);
    button(283, 0, 34, HEADER_HEIGHT, "CAL", false);
}

void Dashboard::updateDht(const dhtsensor::Reading &reading)
{
    const bool changed = reading.status != dhtReading_.status ||
        (reading.status == dhtsensor::Status::Ready &&
         (reading.temperature != dhtReading_.temperature || reading.humidity != dhtReading_.humidity));
    dhtReading_ = reading;
    if (page_ == Page::Dht && changed) dataDirty_ = true;
}

void Dashboard::drawLayout()
{
    tft_.fillScreen(BACKGROUND);
    drawHeader();
    const char *pages[] = {"WEATHER", "LDR / RGB", "DHT"};
    for (uint8_t i = 0; i < 3; ++i) {
        button(PAGE_TAB_X + i * PAGE_TAB_STEP, PAGE_TAB_Y, PAGE_TAB_WIDTH, PAGE_TAB_HEIGHT,
               pages[i], static_cast<uint8_t>(page_) == i);
    }
    if (page_ == Page::Weather) {
        for (uint8_t i = 0; i < weather::LOCATION_COUNT; ++i) {
            const char *name = i == 0 ? "Chiang Mai" : i == 1 ? "Bangkok" : "WiFi area";
            button(6 + i * 105, LOCATION_TAB_Y, 100, LOCATION_TAB_HEIGHT, name, i == location_);
        }
        const char *groups[] = {"Weather", "Rain", "Wind", "PM2.5"};
        for (uint8_t i = 0; i < 4; ++i) {
            button(6 + i * 78, GROUP_TAB_Y, 73, GROUP_TAB_HEIGHT, groups[i], i == group_);
        }
    } else if (page_ == Page::Rgb) {
        label("LDR raw ADC (0-4095)", 8, 77, TFT_LIGHTGREY, 1);
        button(200, 64, 114, 22, rgbEnabled_ ? "STOP RGB" : "START RGB", rgbEnabled_);
    } else {
        char text[48];
        snprintf(text, sizeof(text), "DHT%u sensor | DATA GPIO%u", app::DHT_SENSOR_MODEL, cyd::SAFE_SENSOR_PIN);
        label(text, 8, 62, TFT_CYAN, 2);
        label("Local temperature and humidity", 8, 84, TFT_LIGHTGREY, 2);
    }
    layoutDirty_ = false;
    dataDirty_ = true;
}

void Dashboard::drawWeather(const weather::Snapshot &data, bool serviceReady)
{
    tft_.fillRect(0, WEATHER_CONTENT_Y, 320, 240 - WEATHER_CONTENT_Y, BACKGROUND);
    char text[80];
    if (location_ == 2) {
        snprintf(text, sizeof(text), "IP approx: %.32s", data.position.city[0] ? data.position.city : "locating...");
        label(text, 6, 104, TFT_YELLOW, 1);
    } else label("City centre weather | OpenWeather", 6, 104, TFT_LIGHTGREY, 1);

    const bool airPage = group_ == 3;
    const bool valid = airPage ? data.air.valid : data.conditions.valid;
    const bool failed = airPage ? data.airFailed : data.weatherFailed;
    const uint32_t received = airPage ? data.air.receivedAtMs : data.conditions.receivedAtMs;
    const bool stale = valid && (failed || !connected_ || millis() - received > weather::REFRESH_INTERVAL_MS + 60000);
    if (!valid) {
        weatherLabel(data.pending ? "Loading data..." : "Data unavailable", 133, TFT_YELLOW);
        label("Choose a location / tap REFRESH", 8, 169, TFT_LIGHTGREY, 2);
        label("OpenWeather", 8, 205, TFT_LIGHTGREY, 1);
    } else {
        const auto &c = data.conditions;
        if (group_ == 0) {
            snprintf(text, sizeof(text), "Temp %.1f C", c.temperature);
            weatherLabel(text, 123);
            snprintf(text, sizeof(text), "Humidity %.0f %%", c.humidity);
            weatherLabel(text, 155);
            weatherLabel(weather::conditionName(c.code), 187);
        } else if (group_ == 1) {
            snprintf(text, sizeof(text), "Rain %.2f mm/h", c.rain);
            weatherLabel(text, 129);
            snprintf(text, sizeof(text), "Cloud cover %.0f %%", c.clouds);
            weatherLabel(text, 165);
            label("Rain rate from OpenWeather", 8, 190, TFT_LIGHTGREY, 2);
        } else if (group_ == 2) {
            snprintf(text, sizeof(text), "Wind %.1f km/h", c.windSpeed);
            weatherLabel(text, 129);
            if (std::isfinite(c.windDirection))
                snprintf(text, sizeof(text), "From %s (%.0f deg)", weather::windCompass(c.windDirection), c.windDirection);
            else snprintf(text, sizeof(text), "Direction unavailable");
            weatherLabel(text, 165);
            label("Direction the wind blows from", 8, 190, TFT_LIGHTGREY, 2);
        } else {
            snprintf(text, sizeof(text), "PM2.5: %.1f ug/m3", data.air.pm25);
            weatherLabel(text, 133);
            label("OpenWeather Air Pollution", 8, 166, TFT_LIGHTGREY, 2);
            label("Current air pollution data", 8, 190, TFT_LIGHTGREY, 2);
        }
        char stamp[32];
        localTimeText(stamp, sizeof(stamp), airPage ? data.air.timestamp : c.timestamp,
                      data.position.utcOffsetSeconds, "%d/%m %H:%M");
        snprintf(text, sizeof(text), "Data %s%s | OpenWeather", stamp, stale ? " (STALE)" : "");
        label(text, 8, 205, stale ? TFT_YELLOW : TFT_LIGHTGREY, 1);
    }
    const char *state = !serviceReady ? "Weather service unavailable"
                      : !connected_ ? "Connect WiFi to get weather"
                      : time(nullptr) < 1704067200 ? "Synchronizing clock..."
                      : data.pending ? "Updating..."
                      : failed ? (data.error[0] ? data.error : "Update failed; retry scheduled") : "Updates every 10 minutes";
    label(state, 8, 216, failed ? TFT_YELLOW : TFT_LIGHTGREY, 1);
    drawWeatherClock(data);
    button(REFRESH_X, REFRESH_Y, REFRESH_WIDTH, REFRESH_HEIGHT, "REFRESH", data.pending);
}

void Dashboard::drawWeatherClock(const weather::Snapshot &data)
{
    // Update only the clock; do not clear the adjacent REFRESH button.
    tft_.fillRect(0, 222, REFRESH_X - 1, 18, BACKGROUND);
    char text[32];
    if (weather::validPosition(data.position)) {
        localTimeText(text, sizeof(text), time(nullptr), data.position.utcOffsetSeconds, "%d/%m/%Y %H:%M:%S");
    } else snprintf(text, sizeof(text), "Local time: --");
    label(text, 8, 230);
}

void Dashboard::drawLed(int16_t x, const char *name, uint16_t color, bool on)
{
    tft_.fillRoundRect(x - 45, 167, 90, 68, 6, CARD);
    if (on) tft_.drawRoundRect(x - 45, 167, 90, 68, 6, color);
    const uint16_t body = on ? color : INACTIVE;
    tft_.fillCircle(x, 185, 10, body);
    tft_.fillRect(x - 10, 185, 21, 14, body);
    tft_.fillRect(x - 13, 199, 27, 3, TFT_DARKGREY);
    tft_.drawFastVLine(x - 6, 202, 8, TFT_LIGHTGREY);
    tft_.drawFastVLine(x + 6, 202, 10, TFT_LIGHTGREY);
    char text[16];
    snprintf(text, sizeof(text), "%s %s", name, on ? "ON" : "OFF");
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(on ? TFT_WHITE : TFT_LIGHTGREY, CARD);
    tft_.drawString(text, x, 222, 2);
}

void Dashboard::drawRgb()
{
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(rgbEnabled_ ? ledColor_ : TFT_LIGHTGREY, BACKGROUND);
    tft_.setTextSize(3);
    tft_.setTextPadding(280);
    tft_.drawNumber(raw_, 160, 113, 2);
    tft_.setTextPadding(0);
    tft_.setTextSize(1);
    tft_.fillRect(0, 140, 320, 24, BACKGROUND);
    char text[32];
    snprintf(text, sizeof(text), "V = %.3f V", millivolts_ / 1000.0f);
    label(text, 8, 150);
    label(resistance_, 166, 150);
    drawLed(54, "RED", TFT_RED, rgbEnabled_ && ledColor_ == TFT_RED);
    drawLed(160, "GREEN", TFT_GREEN, rgbEnabled_ && ledColor_ == TFT_GREEN);
    drawLed(266, "BLUE", TFT_BLUE, rgbEnabled_ && ledColor_ == TFT_BLUE);
}

void Dashboard::drawDht()
{
    tft_.fillRect(0, WEATHER_CONTENT_Y, 320, 240 - WEATHER_CONTENT_Y, BACKGROUND);
    char text[48];
    const bool ready = dhtReading_.status == dhtsensor::Status::Ready;
    if (ready) snprintf(text, sizeof(text), "Temp %.1f C", dhtReading_.temperature);
    else snprintf(text, sizeof(text), "Temp --.- C");
    weatherLabel(text, 122, ready ? TFT_WHITE : TFT_LIGHTGREY);
    if (ready) snprintf(text, sizeof(text), "Humidity %.1f %%", dhtReading_.humidity);
    else snprintf(text, sizeof(text), "Humidity --.- %%");
    weatherLabel(text, 158, ready ? TFT_WHITE : TFT_LIGHTGREY);

    if (dhtReading_.status == dhtsensor::Status::Waiting) {
        label("Waiting for first sensor reading...", 8, 190, TFT_YELLOW, 2);
    } else if (!ready) {
        label("No sensor / sensor error", 8, 190, TFT_RED, 2);
        label("Check DATA, VCC, GND and DHT type", 8, 211, TFT_YELLOW, 2);
    } else {
        drawDhtAge();
        label("Measures here | Updates every 2 s", 8, 211, TFT_LIGHTGREY, 2);
    }
    label("< Swipe left / right to change page >", 8, 233, TFT_LIGHTGREY, 1);
}

void Dashboard::drawDhtAge()
{
    tft_.fillRect(0, 180, 320, 20, BACKGROUND);
    char text[48];
    snprintf(text, sizeof(text), "Sensor OK | Read %lu s ago",
             static_cast<unsigned long>((millis() - dhtReading_.attemptedAtMs) / 1000));
    label(text, 8, 190, TFT_GREEN, 2);
}

void Dashboard::swipe(touchgesture::Kind direction)
{
    const uint8_t current = static_cast<uint8_t>(page_);
    // Swiping left reveals the next page; right returns to the previous page.
    page_ = static_cast<Page>((current + (direction == touchgesture::Kind::SwipeLeft ? 1 : 2)) % 3);
    layoutDirty_ = true;
}

void Dashboard::beginCalibration()
{
    gesture_.cancel();
    calibrating_ = true;
    waitRelease_ = true;
    calibrationStep_ = calibrationSamples_ = 0;
    calibrationSumX_ = calibrationSumY_ = 0;
    drawCalibrationTarget();
}

void Dashboard::drawCalibrationTarget()
{
    tft_.fillScreen(BACKGROUND);
    label("Touch calibration", 80, 95);
    label("Hold the cross, then release", 45, 119, TFT_LIGHTGREY, 2);
    char text[24];
    snprintf(text, sizeof(text), "Point %u / 4", calibrationStep_ + 1);
    label(text, 113, 142, TFT_YELLOW);
    const auto target = targets[calibrationStep_];
    tft_.drawCircle(target.x, target.y, 10, TFT_YELLOW);
    tft_.drawFastHLine(target.x - 15, target.y, 31, TFT_YELLOW);
    tft_.drawFastVLine(target.x, target.y - 15, 31, TFT_YELLOW);
}

void Dashboard::pollTouch(uint32_t now, WeatherService &weather)
{
    if (now - lastTouchAt_ < 20) return;
    lastTouchAt_ = now;
    const bool down = touch_.touched();
    if (calibrating_) {
        if (waitRelease_) {
            if (!down) waitRelease_ = false;
            return;
        }
        if (!down) {
            calibrationSamples_ = 0;
            calibrationSumX_ = calibrationSumY_ = 0;
            return;
        }
        const auto point = touch_.getPoint();
        if (point.z < 300) return;
        calibrationSumX_ += point.x;
        calibrationSumY_ += point.y;
        if (++calibrationSamples_ < 12) return;
        calibrationPoints_[calibrationStep_] = {calibrationSumX_ / 12, calibrationSumY_ / 12};
        calibrationSamples_ = 0;
        calibrationSumX_ = calibrationSumY_ = 0;
        waitRelease_ = true;
        if (++calibrationStep_ < 4) {
            drawCalibrationTarget();
            return;
        }
        touchcal::Calibration candidate;
        if (touchcal::fit(calibrationPoints_, candidate)) {
            const auto validation = candidate.project(calibrationPoints_[3]);
            if (std::abs(validation.x - 300) < 20 && std::abs(validation.y - 220) < 20) {
                calibration_ = candidate;
                if (preferencesReady_) preferences_.putBytes("cal", &calibration_, sizeof(calibration_));
                calibrating_ = false;
                touchHeld_ = true;
                layoutDirty_ = true;
                return;
            }
        }
        Serial.println("Touch calibration failed validation; repeat the four points.");
        beginCalibration();
        return;
    }
    if (!down) {
        if (touchHeld_) {
            touchHeld_ = false;  // Ignore release of the last calibration point.
            gesture_.cancel();
            return;
        }
        const auto event = gesture_.release(now);
        if (event.kind == touchgesture::Kind::Tap) tap(event.position.x, event.position.y, weather);
        else if (event.kind == touchgesture::Kind::SwipeLeft || event.kind == touchgesture::Kind::SwipeRight) {
            swipe(event.kind);
        }
        return;
    }
    if (touchHeld_) return;
    const auto raw = touch_.getPoint();
    if (raw.z < 300) return;
    const auto position = calibration_.project({static_cast<float>(raw.x), static_cast<float>(raw.y)});
    if (position.x >= 0 && position.x < 320 && position.y >= 0 && position.y < 240) {
        gesture_.track(position.x, position.y, now);
    }
}

void Dashboard::tap(int16_t x, int16_t y, WeatherService &weather)
{
    if (x >= 283 && x < 317 && y < HEADER_HEIGHT) { beginCalibration(); return; }
    if (y >= PAGE_TAB_Y && y < PAGE_TAB_Y + PAGE_TAB_HEIGHT) {
        for (uint8_t i = 0; i < 3; ++i) {
            const int16_t left = PAGE_TAB_X + i * PAGE_TAB_STEP;
            if (x >= left && x < left + PAGE_TAB_WIDTH) {
                page_ = static_cast<Page>(i);
                layoutDirty_ = true;
                break;
            }
        }
    } else if (page_ == Page::Rgb && x >= 200 && x < 314 && y >= 64 && y < 86) {
        setRgbEnabled(!rgbEnabled_);
    } else if (page_ == Page::Weather) {
        if (y >= LOCATION_TAB_Y && y < LOCATION_TAB_Y + LOCATION_TAB_HEIGHT) {
            for (uint8_t i = 0; i < 3; ++i) {
                if (x >= 6 + 105 * i && x < 106 + 105 * i) { location_ = i; layoutDirty_ = true; }
            }
        } else if (y >= GROUP_TAB_Y && y < GROUP_TAB_Y + GROUP_TAB_HEIGHT) {
            for (uint8_t i = 0; i < 4; ++i) {
                if (x >= 6 + 78 * i && x < 79 + 78 * i) { group_ = i; layoutDirty_ = true; }
            }
        } else if (x >= REFRESH_X && x < REFRESH_X + REFRESH_WIDTH &&
                   y >= REFRESH_Y && y < REFRESH_Y + REFRESH_HEIGHT) {
            weather.request(location_, true);
            dataDirty_ = true;
        }
    }
}

void Dashboard::poll(uint32_t now, WeatherService &weather)
{
    pollTouch(now, weather);
    if (calibrating_) return;
    if (layoutDirty_) drawLayout();
    const bool clockTick = now - lastFrameAt_ >= 1000;
    if (!dataDirty_ && !clockTick) return;
    if (page_ == Page::Weather) {
        const auto &data = weather.get(location_);
        const bool airPage = group_ == 3;
        const bool valid = airPage ? data.air.valid : data.conditions.valid;
        const bool failed = airPage ? data.airFailed : data.weatherFailed;
        const uint32_t received = airPage ? data.air.receivedAtMs : data.conditions.receivedAtMs;
        const bool stale = valid && (failed || !connected_ || now - received > weather::REFRESH_INTERVAL_MS + 60000);
        if (dataDirty_ || lastWeatherRevision_ != data.revision || lastWeatherStale_ != stale) {
            drawWeather(data, weather.ready());
            lastWeatherRevision_ = data.revision;
            lastWeatherStale_ = stale;
        } else if (clockTick) drawWeatherClock(data);
    } else if (page_ == Page::Rgb) {
        if (dataDirty_) drawRgb();
    } else if (dataDirty_) drawDht();
    else if (clockTick && dhtReading_.status == dhtsensor::Status::Ready) drawDhtAge();
    dataDirty_ = false;
    lastFrameAt_ = now;
}
