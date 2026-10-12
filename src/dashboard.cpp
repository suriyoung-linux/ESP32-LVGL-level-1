#include "dashboard.h"

#include <WiFi.h>
#include <ctime>
#include <cstring>

namespace {
// Share the row positions between rendering and touch hit testing.
constexpr int16_t HEADER_HEIGHT = 20;
constexpr int16_t SETTINGS_X = 240, SETTINGS_WIDTH = 74;
constexpr int16_t SLEEP_OPTION_X = 16, SLEEP_OPTION_Y = 70;
constexpr int16_t SLEEP_OPTION_WIDTH = 140, SLEEP_OPTION_HEIGHT = 27, SLEEP_OPTION_STEP = 31;
constexpr int16_t OPTION_COLUMN_STEP = 148;
constexpr int16_t LOG_OPTION_Y = 168;
constexpr int16_t THEME_MODE_Y = 64, THEME_MODE_WIDTH = 96, THEME_MODE_STEP = 104;
constexpr int16_t THEME_LIGHT_Y = 122, THEME_DARK_Y = 164, THEME_TIME_HEIGHT = 28;
constexpr uint16_t SLEEP_MINUTES[] = {2, 5, 10, 0};
constexpr uint32_t DIM_WARNING_MS = 15000;
// LEDC channel 0 is reserved for the TFT backlight; RGB LEDs use digitalWrite.
constexpr uint8_t BACKLIGHT_CHANNEL = 0, BACKLIGHT_BITS = 8;
constexpr uint32_t BACKLIGHT_FREQUENCY_HZ = 5000;
constexpr uint8_t BACKLIGHT_FULL = 255, BACKLIGHT_DIM = 64;
constexpr int16_t CALIBRATION_MAX_RAW_SPREAD = 120;

bool validSleepMinutes(uint16_t minutes)
{
    return minutes == 0 || minutes == 2 || minutes == 5 || minutes == 10;
}

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
    tft_.setTextColor(palette_.text(color), palette_.background);
    tft_.drawString(text, x, y, font);
}

void Dashboard::weatherLabel(const char *text, int16_t y, uint16_t color)
{
    // Reuse Font 2 at double size instead of storing another full glyph table.
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextSize(2);
    if (tft_.textWidth(text, 2) > 304) tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(palette_.text(color), palette_.background);
    tft_.drawString(text, 8, y, 2);
}

void Dashboard::button(int16_t x, int16_t y, int16_t width, int16_t height, const char *text, bool active)
{
    const uint16_t background = active ? palette_.active : palette_.card;
    tft_.fillRoundRect(x, y, width, height, 4, background);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(active ? TFT_WHITE : palette_.foreground, background);
    tft_.drawString(text, x + width / 2, y + height / 2, height < 22 ? 1 : 2);
}

void Dashboard::begin()
{
    tft_.init();
    tft_.setRotation(cyd::DISPLAY_ROTATION);
    cyd::enableBacklight();
    ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQUENCY_HZ, BACKLIGHT_BITS);
    ledcAttachPin(cyd::TFT_PIN_BACKLIGHT, BACKLIGHT_CHANNEL);
    setBacklight(BACKLIGHT_FULL);
    touchSpi_.begin(cyd::TOUCH_PIN_CLK, cyd::TOUCH_PIN_MISO, cyd::TOUCH_PIN_MOSI, cyd::TOUCH_PIN_CS);
    touch_.begin(touchSpi_);
    touch_.setRotation(1);
    logger_.begin(touchSpi_);
    preferencesReady_ = preferences_.begin("weather-ui", false);
    if (preferencesReady_) {
        rgbEnabled_ = preferences_.getBool("rgb", true);
        const uint16_t savedSleep = preferences_.getUShort("sleep-min", 0);
        sleepMinutes_ = validSleepMinutes(savedSleep) ? savedSleep : 0;
        displaytheme::Config savedTheme;
        savedTheme.mode = static_cast<displaytheme::Mode>(preferences_.getUShort("theme-mode", 0));
        const uint32_t times = preferences_.getUInt("theme-times", (7UL * 60 << 16) | (19UL * 60));
        savedTheme.lightStart = times >> 16;
        savedTheme.darkStart = times & 0xFFFF;
        theme_ = savedTheme.valid() ? savedTheme : displaytheme::Config{};
        if (preferences_.getBytesLength("cal") == sizeof(calibration_)) {
            preferences_.getBytes("cal", &calibration_, sizeof(calibration_));
        }
    }
    palette_ = displaytheme::Palette::make(theme_.mode == displaytheme::Mode::Light);
    lastActivityAt_ = millis();
    if (!calibration_.valid()) beginCalibration();
}

void Dashboard::setNetwork(const char *, const IPAddress &, bool connected)
{
    if (connected_ == connected) return;
    connected_ = connected;
    headerDirty_ = true;
    if (page_ == Page::Weather) dataDirty_ = true;
}

void Dashboard::setBacklight(uint8_t duty)
{
    if (backlightDuty_ == duty) return;
    ledcWrite(BACKLIGHT_CHANNEL, duty);
    backlightDuty_ = duty;
}

void Dashboard::setSleepMinutes(uint16_t minutes, uint32_t now)
{
    if (!validSleepMinutes(minutes)) return;
    lastActivityAt_ = now;
    screenDimmed_ = false;
    setBacklight(BACKLIGHT_FULL);
    if (sleepMinutes_ == minutes) return;
    sleepMinutes_ = minutes;
    if (preferencesReady_) preferences_.putUShort("sleep-min", minutes);
    layoutDirty_ = true;
}

void Dashboard::setRgbEnabled(bool enabled)
{
    if (rgbEnabled_ == enabled) return;
    rgbEnabled_ = enabled;
    if (preferencesReady_) preferences_.putBool("rgb", enabled);
    layoutDirty_ = true;
}

void Dashboard::setThemeMode(displaytheme::Mode mode)
{
    if (static_cast<uint16_t>(mode) > static_cast<uint16_t>(displaytheme::Mode::Auto) || theme_.mode == mode) return;
    theme_.mode = mode;
    if (preferencesReady_) preferences_.putUShort("theme-mode", static_cast<uint16_t>(mode));
    layoutDirty_ = true;
}

void Dashboard::adjustThemeTime(bool lightStart, int16_t change)
{
    if (!change) return;
    const uint16_t current = lightStart ? theme_.lightStart : theme_.darkStart;
    const uint16_t other = lightStart ? theme_.darkStart : theme_.lightStart;
    uint16_t next = (static_cast<int32_t>(current) + change % 1440 + 1440) % 1440;
    // Keep a non-empty interval when an adjustment reaches the other boundary.
    if (next == other) next = (next + (change < 0 ? -15 : 15) + 1440) % 1440;
    if (next == current) return;
    if (lightStart) theme_.lightStart = next;
    else theme_.darkStart = next;
    if (preferencesReady_) preferences_.putUInt("theme-times", (static_cast<uint32_t>(theme_.lightStart) << 16) | theme_.darkStart);
    layoutDirty_ = true;
}

void Dashboard::updateTheme(const weather::Snapshot &data)
{
    const time_t epoch = time(nullptr);
    const bool clockReady = epoch >= 1704067200 && weather::validPosition(data.position);
    if (clockReady != themeClockReady_) {
        themeClockReady_ = clockReady;
        if (page_ == Page::Appearance) layoutDirty_ = true;
    }
    bool light = palette_.light;
    if (theme_.mode != displaytheme::Mode::Auto) light = theme_.mode == displaytheme::Mode::Light;
    else if (clockReady) {
        const int64_t localSeconds = static_cast<int64_t>(epoch) + data.position.utcOffsetSeconds;
        const uint16_t minute = ((localSeconds % 86400 + 86400) % 86400) / 60;
        light = displaytheme::lightAt(theme_, minute);
    }
    if (light == palette_.light) return;
    palette_ = displaytheme::Palette::make(light);
    layoutDirty_ = headerDirty_ = dataDirty_ = true;
    if (calibrating_) drawCalibrationTarget();
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

void Dashboard::drawHeader(const weather::Snapshot &data)
{
    tft_.fillRect(0, 0, 320, HEADER_HEIGHT, palette_.background);
    char text[16];
    if (weather::validPosition(data.position))
        localTimeText(text, sizeof(text), time(nullptr), data.position.utcOffsetSeconds, "%H:%M:%S");
    else snprintf(text, sizeof(text), "--:--:--");
    label(text, 5, 10, TFT_WHITE, 2);
    const bool serialReady = static_cast<bool>(Serial);
    if (connected_) {
        const int32_t strength = WiFi.RSSI();
        for (uint8_t i = 0; i < 3; ++i) {
            const int16_t height = 4 + i * 4;
            tft_.fillRect(97 + i * 6, 17 - height, 4, height,
                          strength >= -85 + i * 15 ? palette_.success : palette_.inactive);
        }
    }
    label(connected_ ? "WiFi connected" : serialReady ? "Serial ready" : "Offline",
          119, 10, connected_ ? TFT_GREEN : TFT_YELLOW, 1);
    const bool mqttConnected = mqttStatus_.state == mqttdata::State::Connected;
    const bool mqttError = mqttStatus_.state == mqttdata::State::Unavailable || mqttStatus_.queueFailed;
    const uint16_t mqttColor = mqttError ? TFT_RED : mqttConnected ? TFT_GREEN : TFT_YELLOW;
    label("MQTT", 208, 5, mqttColor, 1);
    label(mqttError ? "ERR" : mqttConnected ? "OK" : mqttStatus_.state == mqttdata::State::WaitingWifi ? "OFF" : "WAIT",
          208, 14, mqttColor, 1);
    button(SETTINGS_X, 0, SETTINGS_WIDTH, HEADER_HEIGHT, "SET",
           page_ == Page::Settings || page_ == Page::Appearance || page_ == Page::Mqtt);
    headerDirty_ = false;
}

void Dashboard::drawSettings()
{
    button(8, 28, 108, 26, "LIGHT / DARK", false);
    button(120, 28, 108, 26, "MQTT", false);
    button(SETTINGS_X, 28, SETTINGS_WIDTH, 26, "BACK", false);
    label("Sleep after no touch", 16, 60, TFT_LIGHTGREY, 1);
    const char *options[] = {"2 minutes", "5 minutes", "10 minutes", "Never sleep"};
    for (uint8_t i = 0; i < 4; ++i) {
        button(SLEEP_OPTION_X + (i % 2) * OPTION_COLUMN_STEP, SLEEP_OPTION_Y + (i / 2) * SLEEP_OPTION_STEP,
               SLEEP_OPTION_WIDTH, SLEEP_OPTION_HEIGHT, options[i], sleepMinutes_ == SLEEP_MINUTES[i]);
    }
    label("Dims 15s before sleep | Touch to wake", 16, 138, TFT_LIGHTGREY, 1);
    const bool cardReady = logger_.card() == DataLogger::Card::Ready;
    label(cardReady ? "microSD: Found" : logger_.card() == DataLogger::Card::Error ?
          "microSD: Read/write error" : "microSD: Not found / cannot mount",
          16, 156, cardReady ? TFT_GREEN : TFT_YELLOW, 1);
    button(16, LOG_OPTION_Y, 140, 27, cardReady ? "LOG ON" : "CHECK SD", logger_.enabled());
    button(164, LOG_OPTION_Y, 140, 27, "LOG OFF", !logger_.enabled());
    label(logger_.enabled() ? "Recording CSV every 1 min" : "CSV logging stopped", 16, 204, TFT_LIGHTGREY, 1);
    button(16, 215, 130, 22, "CAL TOUCH", false);
    button(164, 215, 140, 22, "TEST TOUCH", false);
}

void Dashboard::updateDht(const dhtsensor::Reading &reading)
{
    const bool changed = reading.status != dhtReading_.status ||
        (reading.status == dhtsensor::Status::Ready &&
         (reading.temperature != dhtReading_.temperature || reading.humidity != dhtReading_.humidity));
    dhtReading_ = reading;
    if (page_ == Page::Dht && changed) dataDirty_ = true;
}

void Dashboard::updateMqtt(const mqttdata::Status &status, const char *topicRoot)
{
    const bool connectionChanged = mqttStatus_.state != status.state || mqttStatus_.queueFailed != status.queueFailed;
    const bool changed = connectionChanged || mqttStatus_.acknowledgements != status.acknowledgements ||
        mqttStatus_.lastAckAtMs != status.lastAckAtMs || mqttStatus_.queuedBatches != status.queuedBatches ||
        strcmp(mqttTopicRoot_, topicRoot) != 0;
    mqttStatus_ = status;
    snprintf(mqttTopicRoot_, sizeof(mqttTopicRoot_), "%s", topicRoot);
    if (connectionChanged) headerDirty_ = true;
    if (changed && page_ == Page::Mqtt) dataDirty_ = true;
}

void Dashboard::drawMqtt()
{
    tft_.fillRect(0, 54, 320, 186, palette_.background);
    const bool connected = mqttStatus_.state == mqttdata::State::Connected;
    char text[80];
    snprintf(text, sizeof(text), "MQTT: %s", mqttdata::stateName(mqttStatus_.state));
    label(text, 8, 69, connected ? TFT_GREEN : TFT_YELLOW, 2);
    label("broker.hivemq.com:1883 | No login", 8, 91, TFT_LIGHTGREY, 1);
    snprintf(text, sizeof(text), "Topics: %s/", mqttTopicRoot_[0] ? mqttTopicRoot_ : "not initialized");
    label(text, 8, 114, TFT_CYAN, 1);
    label("temperature (C) | humidity (%)", 8, 133, TFT_WHITE, 1);
    label("telemetry (JSON) | status (online/offline)", 8, 150, TFT_WHITE, 1);
    label("Every 10 s | QoS 1 | Retained", 8, 171, TFT_LIGHTGREY, 1);
    if (mqttStatus_.acknowledgements) {
        snprintf(text, sizeof(text), "Last ACK %lu s ago | ACKs %lu",
                 static_cast<unsigned long>((millis() - mqttStatus_.lastAckAtMs) / 1000),
                 static_cast<unsigned long>(mqttStatus_.acknowledgements));
    } else snprintf(text, sizeof(text), "Waiting for broker acknowledgement");
    label(text, 8, 190, TFT_LIGHTGREY, 1);
    snprintf(text, sizeof(text), "Batches %lu | %s", static_cast<unsigned long>(mqttStatus_.queuedBatches),
             mqttStatus_.queueFailed ? "Queue failed; retrying" : "Auto reconnect");
    label(text, 8, 208, mqttStatus_.queueFailed ? TFT_YELLOW : TFT_LIGHTGREY, 1);
    label("Public broker | DHT temperature/humidity", 8, 231, TFT_LIGHTGREY, 1);
}

void Dashboard::drawAppearance()
{
    label("Display theme", 8, 40, TFT_CYAN, 2);
    button(SETTINGS_X, 28, SETTINGS_WIDTH, 26, "BACK", false);
    const char *names[] = {"LIGHT", "DARK", "AUTO"};
    const displaytheme::Mode modes[] = {displaytheme::Mode::Light, displaytheme::Mode::Dark, displaytheme::Mode::Auto};
    for (uint8_t i = 0; i < 3; ++i)
        button(8 + i * THEME_MODE_STEP, THEME_MODE_Y, THEME_MODE_WIDTH, 28, names[i], theme_.mode == modes[i]);
    char text[48];
    snprintf(text, sizeof(text), "Now: %s | %s", palette_.light ? "Light" : "Dark",
             theme_.mode == displaytheme::Mode::Auto ? "Scheduled" : "Fixed mode");
    label(text, 8, 104, TFT_LIGHTGREY, 1);
    for (uint8_t i = 0; i < 2; ++i) {
        const int16_t y = i == 0 ? THEME_LIGHT_Y : THEME_DARK_Y;
        const uint16_t minute = i == 0 ? theme_.lightStart : theme_.darkStart;
        label(i == 0 ? "Light from" : "Dark from", 16, y + 14, TFT_WHITE, 2);
        button(160, y, 36, THEME_TIME_HEIGHT, "-", false);
        snprintf(text, sizeof(text), "%02u:%02u", minute / 60, minute % 60);
        label(text, 204, y + 14, TFT_WHITE, 2);
        button(268, y, 36, THEME_TIME_HEIGHT, "+", false);
    }
    label("+ / - adjusts 15 min | AUTO uses top clock", 8, 205, TFT_LIGHTGREY, 1);
    if (theme_.mode == displaytheme::Mode::Auto && !themeClockReady_)
        label("Waiting for clock; keeping current theme", 8, 229, TFT_YELLOW, 1);
    else label(preferencesReady_ ? "Saved | Schedule runs only in AUTO" : "Not saved | Schedule runs only in AUTO",
               8, 229, preferencesReady_ ? TFT_LIGHTGREY : TFT_YELLOW, 1);
}

void Dashboard::drawLayout()
{
    tft_.fillScreen(palette_.background);
    if (page_ == Page::Mqtt) {
        label("MQTT / HiveMQ", 8, 40, TFT_CYAN, 2);
        button(SETTINGS_X, 28, SETTINGS_WIDTH, 26, "BACK", false);
        drawMqtt();
        layoutDirty_ = dataDirty_ = false;
        return;
    }
    if (page_ == Page::Appearance) {
        drawAppearance();
        layoutDirty_ = false;
        dataDirty_ = false;
        return;
    }
    if (page_ == Page::TouchTest) {
        drawTouchTest();
        layoutDirty_ = false;
        dataDirty_ = false;
        return;
    }
    if (page_ == Page::Settings) {
        drawSettings();
        layoutDirty_ = false;
        dataDirty_ = false;
        return;
    }
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
    tft_.fillRect(0, WEATHER_CONTENT_Y, 320, 240 - WEATHER_CONTENT_Y, palette_.background);
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
    button(REFRESH_X, REFRESH_Y, REFRESH_WIDTH, REFRESH_HEIGHT, "REFRESH", data.pending);
}

void Dashboard::drawLed(int16_t x, const char *name, uint16_t color, bool on)
{
    tft_.fillRoundRect(x - 45, 167, 90, 68, 6, palette_.card);
    if (on) tft_.drawRoundRect(x - 45, 167, 90, 68, 6, color);
    const uint16_t body = on ? color : palette_.inactive;
    tft_.fillCircle(x, 185, 10, body);
    tft_.fillRect(x - 10, 185, 21, 14, body);
    tft_.fillRect(x - 13, 199, 27, 3, TFT_DARKGREY);
    tft_.drawFastVLine(x - 6, 202, 8, palette_.muted);
    tft_.drawFastVLine(x + 6, 202, 10, palette_.muted);
    char text[16];
    snprintf(text, sizeof(text), "%s %s", name, on ? "ON" : "OFF");
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextSize(1);
    tft_.setTextPadding(0);
    tft_.setTextColor(on ? palette_.foreground : palette_.muted, palette_.card);
    tft_.drawString(text, x, 222, 2);
}

void Dashboard::drawRgb()
{
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(palette_.text(rgbEnabled_ ? ledColor_ : TFT_LIGHTGREY), palette_.background);
    tft_.setTextSize(3);
    tft_.setTextPadding(280);
    tft_.drawNumber(raw_, 160, 113, 2);
    tft_.setTextPadding(0);
    tft_.setTextSize(1);
    tft_.fillRect(0, 140, 320, 24, palette_.background);
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
    tft_.fillRect(0, WEATHER_CONTENT_Y, 320, 240 - WEATHER_CONTENT_Y, palette_.background);
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
    tft_.fillRect(0, 180, 320, 20, palette_.background);
    char text[48];
    snprintf(text, sizeof(text), "Sensor OK | Read %lu s ago",
             static_cast<unsigned long>((millis() - dhtReading_.attemptedAtMs) / 1000));
    label(text, 8, 190, TFT_GREEN, 2);
}

void Dashboard::swipe(touchgesture::Kind direction)
{
    if (page_ == Page::Settings || page_ == Page::TouchTest || page_ == Page::Appearance || page_ == Page::Mqtt) return;
    const uint8_t current = static_cast<uint8_t>(page_);
    // Swiping left reveals the next page; right returns to the previous page.
    page_ = static_cast<Page>((current + (direction == touchgesture::Kind::SwipeLeft ? 1 : 2)) % 3);
    layoutDirty_ = true;
}

void Dashboard::beginCalibration()
{
    gesture_.cancel();
    lastActivityAt_ = millis();
    screenDimmed_ = false;
    screenSleeping_ = false;
    setBacklight(BACKLIGHT_FULL);
    calibrating_ = true;
    waitRelease_ = true;
    calibrationStep_ = calibrationSamples_ = 0;
    calibrationSumX_ = calibrationSumY_ = 0;
    drawCalibrationTarget();
}

void Dashboard::drawCalibrationTarget()
{
    tft_.fillScreen(palette_.background);
    label("Touch calibration", 80, 95);
    label("Hold the cross, then release", 45, 119, TFT_LIGHTGREY, 2);
    char text[24];
    snprintf(text, sizeof(text), "Point %u / 4", calibrationStep_ + 1);
    label(text, 113, 142, TFT_YELLOW);
    const auto target = targets[calibrationStep_];
    tft_.drawCircle(target.x, target.y, 10, palette_.warning);
    tft_.drawFastHLine(target.x - 15, target.y, 31, palette_.warning);
    tft_.drawFastVLine(target.x, target.y - 15, 31, palette_.warning);
}

void Dashboard::beginTouchTest()
{
    gesture_.cancel();
    touchTest_.reset();
    testHasReading_ = false;
    page_ = Page::TouchTest;
    touchHeld_ = true; // Consume the press/release used to enter or finish calibration.
    lastActivityAt_ = millis();
    screenSleeping_ = screenDimmed_ = false;
    setBacklight(BACKLIGHT_FULL);
    layoutDirty_ = true;
    headerDirty_ = true;
}

void Dashboard::drawTouchReadout()
{
    if (touchTest_.complete()) return;
    tft_.fillRect(0, 181, 320, 28, palette_.background);
    char text[64];
    if (!testHasReading_) {
        label("Hold cross steadily, then release", 8, 187, TFT_LIGHTGREY, 1);
        label("Yellow: target | Red: measured touch", 8, 200, TFT_LIGHTGREY, 1);
        return;
    }
    snprintf(text, sizeof(text), "Raw X %.0f Y %.0f Z %d", testRaw_.x, testRaw_.y, testPressure_);
    label(text, 8, 187, TFT_LIGHTGREY, 1);
    if (touchTest_.count()) {
        snprintf(text, sizeof(text), "XY %.1f,%.1f | Last error %.1f px", testProjected_.x, testProjected_.y,
                 touchTest_.error(touchTest_.count() - 1));
    } else snprintf(text, sizeof(text), "Screen X %.1f Y %.1f", testProjected_.x, testProjected_.y);
    label(text, 8, 200, TFT_WHITE, 1);
}

void Dashboard::drawTouchTest()
{
    label("Touch accuracy test", 8, 40, TFT_CYAN, 2);
    button(SETTINGS_X, 28, SETTINGS_WIDTH, 26, "BACK", false);
    button(16, 215, 140, 22, "RETEST", false);
    button(164, 215, 140, 22, "CAL TOUCH", false);
    char text[64];
    if (touchTest_.complete()) {
        label(touchTest_.passed() ? "PASS: all points <= 10 px" : "CHECK: recalibrate and retest", 8, 68,
              touchTest_.passed() ? TFT_GREEN : TFT_YELLOW, 2);
        snprintf(text, sizeof(text), "Avg %.1f px | Max %.1f px", touchTest_.averageError(), touchTest_.maxError());
        label(text, 8, 91, TFT_WHITE, 2);
        for (uint8_t i = 0; i < touchcheck::POINT_COUNT; ++i) {
            const auto measured = touchTest_.measured(i);
            snprintf(text, sizeof(text), "P%u XY %.1f,%.1f  error %.1f px", i + 1, measured.x, measured.y, touchTest_.error(i));
            label(text, 8, 113 + i * 18, touchTest_.error(i) <= touchcheck::MAX_ERROR_PX ? TFT_GREEN : TFT_YELLOW, 1);
        }
        label("Test does not change saved calibration", 8, 203, TFT_LIGHTGREY, 1);
        return;
    }
    snprintf(text, sizeof(text), "Point %u / %u | %s", touchTest_.count() + 1, touchcheck::POINT_COUNT,
             touchTest_.readyToRelease() ? "Release to measure" : "Hold steady: max 10 px");
    label(text, 8, 57, TFT_LIGHTGREY, 1);
    for (uint8_t i = 0; i < touchTest_.count(); ++i) {
        const auto point = touchcheck::TARGETS[i];
        tft_.drawCircle(point.x, point.y, 4, touchTest_.error(i) <= touchcheck::MAX_ERROR_PX ? palette_.success : palette_.warning);
    }
    const auto target = touchTest_.target();
    tft_.drawCircle(target.x, target.y, 10, palette_.warning);
    tft_.drawFastHLine(target.x - 12, target.y, 25, palette_.warning);
    tft_.drawFastVLine(target.x, target.y - 12, 25, palette_.warning);
    const auto held = touchTest_.heldPoint();
    if (touchTest_.readyToRelease() && held.x >= 4 && held.x < 316 && held.y >= 65 && held.y < 177) {
        tft_.drawCircle(held.x, held.y, 4, palette_.error);
    }
    drawTouchReadout();
}

void Dashboard::pollTouch(uint32_t now, WeatherService &weather)
{
    if (now - lastTouchAt_ < 20) return;
    lastTouchAt_ = now;
    const bool down = touch_.touched();
    if (calibrating_) {
        lastActivityAt_ = now;
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
        if (point.z < 300) {
            calibrationSamples_ = 0;
            calibrationSumX_ = calibrationSumY_ = 0;
            return;
        }
        const touchcal::Point raw = {static_cast<float>(point.x), static_cast<float>(point.y)};
        if (!calibrationSamples_) calibrationMin_ = calibrationMax_ = raw;
        const touchcal::Point minimum = {std::min(calibrationMin_.x, raw.x), std::min(calibrationMin_.y, raw.y)};
        const touchcal::Point maximum = {std::max(calibrationMax_.x, raw.x), std::max(calibrationMax_.y, raw.y)};
        if (maximum.x - minimum.x > CALIBRATION_MAX_RAW_SPREAD || maximum.y - minimum.y > CALIBRATION_MAX_RAW_SPREAD) {
            calibrationSamples_ = 0;
            calibrationSumX_ = calibrationSumY_ = 0;
            calibrationMin_ = calibrationMax_ = raw;
        } else { calibrationMin_ = minimum; calibrationMax_ = maximum; }
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
                beginTouchTest();
                return;
            }
        }
        Serial.println("Touch calibration failed validation; repeat the four points.");
        beginCalibration();
        return;
    }
    if (!down) {
        if (touchHeld_) {
            touchHeld_ = false;  // Ignore release after calibration, brightening or waking the screen.
            lastActivityAt_ = now;
            gesture_.cancel();
            return;
        }
        if (page_ == Page::TouchTest && touchTest_.release()) {
            gesture_.cancel();
            layoutDirty_ = true;
            return;
        }
        const auto event = gesture_.release(now);
        if (event.kind == touchgesture::Kind::Tap) tap(event.position.x, event.position.y, weather);
        else if (event.kind == touchgesture::Kind::SwipeLeft || event.kind == touchgesture::Kind::SwipeRight) {
            swipe(event.kind);
        }
        return;
    }
    if (touchHeld_) { lastActivityAt_ = now; return; }
    const auto raw = touch_.getPoint();
    if (raw.z < 300) {
        if (page_ == Page::TouchTest) {
            if (touchTest_.readyToRelease()) layoutDirty_ = true;
            touchTest_.clearHold();
        }
        return;
    }
    lastActivityAt_ = now;
    if (screenSleeping_ || screenDimmed_) {
        setBacklight(BACKLIGHT_FULL);
        screenSleeping_ = false;
        screenDimmed_ = false;
        touchHeld_ = true;
        gesture_.cancel();
        layoutDirty_ = true;
        headerDirty_ = true;
        return;
    }
    const auto position = calibration_.project({static_cast<float>(raw.x), static_cast<float>(raw.y)});
    if (page_ == Page::TouchTest) {
        testRaw_ = {static_cast<float>(raw.x), static_cast<float>(raw.y)};
        testProjected_ = position;
        testPressure_ = raw.z;
        testHasReading_ = true;
        dataDirty_ = true;
        // Keep the diagnostic area separate from navigation and action buttons.
        const bool wasReady = touchTest_.readyToRelease();
        if (position.y >= 54 && position.y < 190 && !touchTest_.complete()) {
            touchTest_.sample(position);
        } else touchTest_.clearHold();
        if (wasReady != touchTest_.readyToRelease()) layoutDirty_ = true;
    }
    if (position.x >= 0 && position.x < 320 && position.y >= 0 && position.y < 240) {
        gesture_.track(position.x, position.y, now);
    }
}

void Dashboard::tap(int16_t x, int16_t y, WeatherService &weather)
{
    if (page_ == Page::Mqtt) {
        if (x >= SETTINGS_X && x < SETTINGS_X + SETTINGS_WIDTH &&
            ((y >= 0 && y < HEADER_HEIGHT) || (y >= 28 && y < 54))) {
            page_ = Page::Settings;
            layoutDirty_ = true;
        }
        return;
    }
    if (page_ == Page::Appearance) {
        if (x >= SETTINGS_X && x < SETTINGS_X + SETTINGS_WIDTH &&
            ((y >= 0 && y < HEADER_HEIGHT) || (y >= 28 && y < 54))) {
            page_ = Page::Settings;
            layoutDirty_ = true;
        } else if (y >= THEME_MODE_Y && y < THEME_MODE_Y + 28) {
            for (uint8_t i = 0; i < 3; ++i) {
                const int16_t left = 8 + i * THEME_MODE_STEP;
                if (x >= left && x < left + THEME_MODE_WIDTH) {
                    const displaytheme::Mode modes[] = {displaytheme::Mode::Light, displaytheme::Mode::Dark, displaytheme::Mode::Auto};
                    setThemeMode(modes[i]);
                    return;
                }
            }
        } else {
            const bool lightRow = y >= THEME_LIGHT_Y && y < THEME_LIGHT_Y + THEME_TIME_HEIGHT;
            const bool darkRow = y >= THEME_DARK_Y && y < THEME_DARK_Y + THEME_TIME_HEIGHT;
            if (lightRow || darkRow) {
                if (x >= 160 && x < 196) adjustThemeTime(lightRow, -15);
                else if (x >= 268 && x < 304) adjustThemeTime(lightRow, 15);
            }
        }
        return;
    }
    if (page_ == Page::TouchTest) {
        if ((x >= SETTINGS_X && x < SETTINGS_X + SETTINGS_WIDTH && y >= 0 && y < 54)) {
            page_ = Page::Settings;
            gesture_.cancel();
            layoutDirty_ = true;
        } else if (y >= 215 && y < 237) {
            if (x >= 16 && x < 156) beginTouchTest();
            else if (x >= 164 && x < 304) beginCalibration();
        }
        return;
    }
    if (x >= SETTINGS_X && x < SETTINGS_X + SETTINGS_WIDTH && y >= 0 && y < HEADER_HEIGHT) {
        if (page_ == Page::Settings) page_ = pageBeforeSettings_;
        else { pageBeforeSettings_ = page_; page_ = Page::Settings; }
        layoutDirty_ = true;
        return;
    }
    if (page_ == Page::Settings) {
        if (x >= SETTINGS_X && x < SETTINGS_X + SETTINGS_WIDTH && y >= 28 && y < 54) {
            page_ = pageBeforeSettings_;
            layoutDirty_ = true;
        } else if (x >= 8 && x < 116 && y >= 28 && y < 54) {
            page_ = Page::Appearance;
            layoutDirty_ = true;
        } else if (x >= 120 && x < 228 && y >= 28 && y < 54) {
            page_ = Page::Mqtt;
            layoutDirty_ = true;
        } else if (x >= SLEEP_OPTION_X && x < 304) {
            for (uint8_t i = 0; i < 4; ++i) {
                const int16_t left = SLEEP_OPTION_X + (i % 2) * OPTION_COLUMN_STEP;
                const int16_t top = SLEEP_OPTION_Y + (i / 2) * SLEEP_OPTION_STEP;
                if (x >= left && x < left + SLEEP_OPTION_WIDTH && y >= top && y < top + SLEEP_OPTION_HEIGHT) {
                    setSleepMinutes(SLEEP_MINUTES[i], millis());
                    return;
                }
            }
            if (y >= LOG_OPTION_Y && y < LOG_OPTION_Y + 27) {
                if (x < 156) {
                    if (logger_.card() == DataLogger::Card::Ready) logger_.setEnabled(true, millis());
                    else logger_.checkCard(millis());
                } else if (x >= 164) logger_.setEnabled(false, millis());
                layoutDirty_ = true;
                return;
            }
            if (x < 146 && y >= 215 && y < 237) beginCalibration();
            else if (x >= 164 && y >= 215 && y < 237) beginTouchTest();
        }
        return;
    }
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
    updateTheme(weather.get(location_));
    // Recording continues while dimmed, asleep, or calibrating; it is not activity.
    logger_.poll(now, weather, dhtReading_, page_ == Page::Settings, connected_);
    if (lastLogRevision_ != logger_.revision()) {
        lastLogRevision_ = logger_.revision();
        if (page_ == Page::Settings) layoutDirty_ = true;
    }
    if (calibrating_) return;
    if (page_ == Page::TouchTest) lastActivityAt_ = now;
    if (!screenSleeping_) {
        const uint32_t idleMs = now - lastActivityAt_;
        const uint32_t timeoutMs = static_cast<uint32_t>(sleepMinutes_) * 60000UL;
        if (sleepMinutes_ != 0 && idleMs >= timeoutMs) {
            setBacklight(0);
            screenSleeping_ = true;
            screenDimmed_ = false;
            gesture_.cancel();
        } else {
            const bool dim = sleepMinutes_ != 0 && idleMs >= timeoutMs - DIM_WARNING_MS;
            if (screenDimmed_ != dim) {
                screenDimmed_ = dim;
                setBacklight(dim ? BACKLIGHT_DIM : BACKLIGHT_FULL);
            }
        }
    }
    if (screenSleeping_) return;
    const bool redrawLayout = layoutDirty_;
    if (redrawLayout) drawLayout();
    const bool clockTick = now - lastClockAt_ >= 1000;
    if (redrawLayout || headerDirty_ || clockTick) {
        drawHeader(weather.get(location_));
        lastClockAt_ = now;
    }
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
        }
    } else if (page_ == Page::Rgb) {
        if (dataDirty_) drawRgb();
    } else if (page_ == Page::Dht) {
        if (dataDirty_) drawDht();
        else if (clockTick && dhtReading_.status == dhtsensor::Status::Ready) drawDhtAge();
    } else if (page_ == Page::TouchTest) {
        if (dataDirty_) drawTouchReadout();
    } else if (page_ == Page::Mqtt) {
        if (dataDirty_ || clockTick) drawMqtt();
    }
    dataDirty_ = false;
}
