#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "board_config.h"
#include "touch_calibration.h"
#include "weather_service.h"

class Dashboard {
public:
    void begin();
    void poll(uint32_t now, WeatherService &weather);
    void setNetwork(const char *status, const IPAddress &ip, bool connected);
    void updateLdr(uint16_t raw, uint32_t millivolts, const char *resistance, uint16_t color);
    bool rgbEnabled() const { return rgbEnabled_; }
    void setRgbEnabled(bool enabled);
    uint8_t selectedLocation() const { return location_; }

private:
    enum class Page { Weather, Rgb };
    TFT_eSPI tft_;
    SPIClass touchSpi_{VSPI};
    XPT2046_Touchscreen touch_{cyd::TOUCH_PIN_CS, cyd::TOUCH_PIN_IRQ};
    Preferences preferences_;
    bool preferencesReady_ = false;
    Page page_ = Page::Weather;
    uint8_t location_ = 0, group_ = 0;
    bool rgbEnabled_ = true;
    bool connected_ = false;
    char networkStatus_[32] = "WiFi: starting";
    char ipv4_[16] = "--";
    uint16_t raw_ = 0, ledColor_ = TFT_BLUE;
    uint32_t millivolts_ = 0;
    char resistance_[24] = "R: N/A";
    bool layoutDirty_ = true, dataDirty_ = true;
    uint32_t lastFrameAt_ = 0, lastTouchAt_ = 0;
    bool touchHeld_ = false;
    touchcal::Calibration calibration_;
    touchcal::Point calibrationPoints_[4];
    bool calibrating_ = false, waitRelease_ = false;
    uint8_t calibrationStep_ = 0, calibrationSamples_ = 0;
    float calibrationSumX_ = 0, calibrationSumY_ = 0;

    void drawHeader();
    void drawLayout();
    void drawWeather(const weather::Snapshot &data, bool serviceReady);
    void drawRgb();
    void drawLed(int16_t x, const char *name, uint16_t color, bool on);
    void button(int16_t x, int16_t y, int16_t width, int16_t height, const char *label, bool active);
    void label(const char *text, int16_t x, int16_t y, uint16_t color = TFT_WHITE, uint8_t font = 2);
    void weatherLabel(const char *text, int16_t y, uint16_t color = TFT_WHITE);
    void beginCalibration();
    void drawCalibrationTarget();
    void pollTouch(uint32_t now, WeatherService &weather);
    void tap(int16_t x, int16_t y, WeatherService &weather);
};
