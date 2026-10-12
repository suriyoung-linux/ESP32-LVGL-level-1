#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "board_config.h"
#include "touch_calibration.h"
#include "touch_accuracy.h"
#include "weather_service.h"
#include "dht_sensor.h"
#include "touch_gesture.h"
#include "data_logger.h"
#include "display_theme.h"
#include "mqtt_state.h"

class Dashboard {
public:
    void begin();
    void poll(uint32_t now, WeatherService &weather);
    void setNetwork(const char *status, const IPAddress &ip, bool connected);
    void updateLdr(uint16_t raw, uint32_t millivolts, const char *resistance, uint16_t color);
    void updateDht(const dhtsensor::Reading &reading);
    void updateMqtt(const mqttdata::Status &status, const char *topicRoot);
    bool rgbEnabled() const { return rgbEnabled_; }
    void setRgbEnabled(bool enabled);
    uint8_t selectedLocation() const { return location_; }

private:
    enum class Page { Weather, Rgb, Dht, Settings, TouchTest, Appearance, Mqtt };
    TFT_eSPI tft_;
    SPIClass touchSpi_{VSPI};
    XPT2046_Touchscreen touch_{cyd::TOUCH_PIN_CS, cyd::TOUCH_PIN_IRQ};
    DataLogger logger_;
    uint32_t lastLogRevision_ = UINT32_MAX;
    Preferences preferences_;
    bool preferencesReady_ = false;
    Page page_ = Page::Weather;
    Page pageBeforeSettings_ = Page::Weather;
    uint16_t sleepMinutes_ = 0;
    uint32_t lastActivityAt_ = 0;
    bool screenSleeping_ = false, screenDimmed_ = false, headerDirty_ = true;
    uint8_t backlightDuty_ = 0;
    uint8_t location_ = 0, group_ = 0;
    bool rgbEnabled_ = true;
    bool connected_ = false;
    uint16_t raw_ = 0, ledColor_ = TFT_BLUE;
    uint32_t millivolts_ = 0;
    char resistance_[24] = "R: N/A";
    dhtsensor::Reading dhtReading_;
    bool layoutDirty_ = true, dataDirty_ = true;
    uint32_t lastClockAt_ = 0, lastTouchAt_ = 0;
    uint32_t lastWeatherRevision_ = UINT32_MAX;
    bool lastWeatherStale_ = false;
    bool touchHeld_ = false;
    touchgesture::Tracker gesture_;
    touchcal::Calibration calibration_;
    touchcal::Point calibrationPoints_[4];
    touchcal::Point calibrationMin_{0, 0}, calibrationMax_{0, 0};
    bool calibrating_ = false, waitRelease_ = false;
    uint8_t calibrationStep_ = 0, calibrationSamples_ = 0;
    float calibrationSumX_ = 0, calibrationSumY_ = 0;
    touchcheck::Session touchTest_;
    touchcal::Point testRaw_{0, 0}, testProjected_{0, 0};
    int16_t testPressure_ = 0;
    bool testHasReading_ = false;
    displaytheme::Config theme_;
    displaytheme::Palette palette_;
    bool themeClockReady_ = false;
    mqttdata::Status mqttStatus_;
    char mqttTopicRoot_[48] = "";

    void drawHeader(const weather::Snapshot &data);
    void drawSettings();
    void drawAppearance();
    void drawMqtt();
    void setThemeMode(displaytheme::Mode mode);
    void adjustThemeTime(bool lightStart, int16_t change);
    void updateTheme(const weather::Snapshot &data);
    void setSleepMinutes(uint16_t minutes, uint32_t now);
    void setBacklight(uint8_t duty);
    void drawLayout();
    void drawWeather(const weather::Snapshot &data, bool serviceReady);
    void drawRgb();
    void drawDht();
    void drawDhtAge();
    void swipe(touchgesture::Kind direction);
    void drawLed(int16_t x, const char *name, uint16_t color, bool on);
    void button(int16_t x, int16_t y, int16_t width, int16_t height, const char *label, bool active);
    void label(const char *text, int16_t x, int16_t y, uint16_t color = TFT_WHITE, uint8_t font = 2);
    void weatherLabel(const char *text, int16_t y, uint16_t color = TFT_WHITE);
    void beginCalibration();
    void drawCalibrationTarget();
    void beginTouchTest();
    void drawTouchTest();
    void drawTouchReadout();
    void pollTouch(uint32_t now, WeatherService &weather);
    void tap(int16_t x, int16_t y, WeatherService &weather);
};
