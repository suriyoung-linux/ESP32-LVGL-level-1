// ESP32 CYD: อ่าน LDR และเลือกสี RGB LED ตามค่าดิบ ADC
//   มากกว่า 1000 -> แดง, 500 ถึง 1000 -> เขียว, น้อยกว่า 500 -> น้ำเงิน
// อ่านและแสดงผลบน Serial และจอ TFT ทุก 0.5 วินาที โดยจับเวลาด้วย millis()
// แสดงแรงดันที่ GPIO34 เทียบกับ GND และค่าประมาณความต้านทาน LDR
// เชื่อม WiFi โดยไม่วนรอ แสดง IPv4 และลองเชื่อมต่อใหม่เมื่อหลุด

#include <Arduino.h>
#include <WebServer.h>
#include <ctime>
#include "dashboard.h"
#include "weather_service.h"
#include "dht_sensor.h"
#include <WiFi.h>
#include <esp_adc_cal.h>

#include "app_config.h"
#include "board_config.h"
#include "ldr_measurement.h"
#include "api_contract.h"
#include "ldr_reading.h"
#include "web_dashboard.h"

// ค่าจริงอยู่ในไฟล์ที่ไม่เก็บใน Git; ถ้ายังไม่มีให้ใช้แม่แบบว่างเพื่อ Build ได้
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.h.example"
#endif

namespace {

Dashboard dashboard;
WeatherService weatherService;
DhtSensor dhtSensor;
WebServer apiServer(weather::API_PORT);
bool apiStarted = false;
bool wasNetworkConnected = false;
bool lastRgbEnabled = true;
esp_adc_cal_characteristics_t adcCalibration = {};
esp_adc_cal_value_t adcCalibrationSource = ESP_ADC_CAL_VAL_DEFAULT_VREF;
ldr::Reading latestLdrReading;
uint32_t lastWeatherScheduleAt = 0;
uint32_t lastLdrReadAt = 0;  // เวลาอ่าน LDR และอัปเดต LED ครั้งล่าสุด
uint32_t lastWiFiCheckAt = 0;
uint32_t lastWiFiAttemptAt = 0;
bool wifiScanning = false;
int displayedWiFiStatus = -1;
IPAddress displayedIPv4;
bool wifiConfigured = false;

void formatResistance(float resistanceOhms, char *text, size_t textSize)
{
    if (!std::isfinite(resistanceOhms)) {
        snprintf(text, textSize, "R: N/A");
    } else if (resistanceOhms < 1000.0f) {
        snprintf(text, textSize, "R~ %.0f Ohm", resistanceOhms);
    } else if (resistanceOhms < 1000000.0f) {
        snprintf(text, textSize, "R~ %.1f kOhm", resistanceOhms / 1000.0f);
    } else {
        snprintf(text, textSize, "R~ %.2f MOhm", resistanceOhms / 1000000.0f);
    }
}

void initDisplay()
{
    dashboard.begin();
}

void showWiFiStatus(const char *statusText, const IPAddress &ipv4, bool connected)
{
    dashboard.setNetwork(statusText, ipv4, connected);
    if (connected) {
        Serial.printf("%s  IPv4: %u.%u.%u.%u\n", statusText, ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
    } else {
        Serial.printf("%s  IPv4: --\n", statusText);
    }
}

void startWiFiAttempt(uint32_t now)
{
    lastWiFiAttemptAt = now;
    displayedWiFiStatus = -1;
    showWiFiStatus("WiFi: CONNECTING", IPAddress(), false);
    // เริ่มเชื่อมต่อแล้วคืนการทำงานให้ loop() ไม่รอด้วย while/delay
    WiFi.disconnect(false, false);
    WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
}

const char *wifiSecurityName(wifi_auth_mode_t security)
{
    switch (security) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "OTHER";
    }
}

void updateWiFiScan(uint32_t now)
{
    const int16_t count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) {
        return;  // scanComplete() มี timeout ของ framework; อ่าน LDR ต่อระหว่างสแกน
    }

    if (count >= 0) {
        unsigned int matchingNetworks = 0;
        // API ของ framework ใช้ index ขนาด uint8_t
        for (int16_t index = 0; index < count && index <= UINT8_MAX; ++index) {
            if (WiFi.SSID(index) == DEFAULT_WIFI_SSID) {
                ++matchingNetworks;
                Serial.printf("WiFi scan: target FOUND, RSSI = %ld dBm, channel = %ld, security = %s\n",
                              static_cast<long>(WiFi.RSSI(index)), static_cast<long>(WiFi.channel(index)),
                              wifiSecurityName(WiFi.encryptionType(index)));
            }
        }
        Serial.printf("WiFi scan: %d networks visible, %u match configured SSID\n", count, matchingNetworks);
        if (matchingNetworks == 0) {
            Serial.println("WiFi scan: target NOT FOUND; check exact SSID, 2.4 GHz, signal and hidden SSID.");
        }
    } else {
        Serial.println("WiFi scan: failed; proceeding with connection attempt.");
    }
    WiFi.scanDelete();  // คืนหน่วยความจำผลสแกนก่อนเริ่มเชื่อมต่อ
    wifiScanning = false;
    startWiFiAttempt(now);
}

void initWiFi()
{
    wifiConfigured = DEFAULT_WIFI_SSID[0] != '\0';
    if (!wifiConfigured) {
        showWiFiStatus("WiFi: SET SSID", IPAddress(), false);
        Serial.println("Set DEFAULT_WIFI_SSID and DEFAULT_WIFI_PASSWORD in include/secrets.h, then upload again.");
        return;
    }

    WiFi.persistent(false);  // ใช้ค่าจาก secrets.h โดยไม่เขียนซ้ำลง flash
    WiFi.setAutoReconnect(false);  // จัดการเวลาลองใหม่ใน updateWiFi() เพียงที่เดียว
    // Callback อยู่คนละ task: พิมพ์ Serial เท่านั้น ไม่วาด TFT หรือแก้สถานะของ loop()
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const uint8_t reason = info.wifi_sta_disconnected.reason;
            Serial.printf("WiFi event: disconnected, reason = %u (%s)\n", reason,
                          WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
        } else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) {
            Serial.printf("WiFi event: scan done, status = %lu, count = %u\n",
                          static_cast<unsigned long>(info.wifi_scan_done.status),
                          info.wifi_scan_done.number);
        }
    });
    WiFi.mode(WIFI_STA);  // เชื่อมกับเราเตอร์ และรับ IPv4 ผ่าน DHCP
    Serial.printf("WiFi: SSID = %s (2.4 GHz), retry interval = %lu ms\n",
                  DEFAULT_WIFI_SSID, static_cast<unsigned long>(app::WIFI_RETRY_INTERVAL_MS));
    // สแกนครั้งเดียวตอนเปิดเครื่องเพื่อช่วยหาสาเหตุ SSID NOT FOUND
    showWiFiStatus("WiFi: SCANNING", IPAddress(), false);
    // เพิ่มเวลาเผื่อสแกนทุก channel; timeout ของ framework = 20 x ค่านี้
    const int16_t scanResult = WiFi.scanNetworks(true, true, false, 500);
    wifiScanning = scanResult != WIFI_SCAN_FAILED;
    if (!wifiScanning) {
        Serial.println("WiFi scan: could not start; proceeding with connection attempt.");
        startWiFiAttempt(millis());
    }
}

void updateWiFi(uint32_t now)
{
    if (!wifiConfigured || now - lastWiFiCheckAt < app::WIFI_CHECK_INTERVAL_MS) {
        return;
    }
    lastWiFiCheckAt = now;

    if (wifiScanning) {
        updateWiFiScan(now);
        return;
    }

    const wl_status_t status = WiFi.status();
    const IPAddress ipv4 = status == WL_CONNECTED ? WiFi.localIP() : IPAddress();
    const bool connected = status == WL_CONNECTED && ipv4 != IPAddress();
    if (status != displayedWiFiStatus || ipv4 != displayedIPv4) {
        const char *statusText;
        switch (status) {
            case WL_CONNECTED: statusText = connected ? "WiFi: CONNECTED" : "WiFi: WAIT IPv4"; break;
            case WL_NO_SSID_AVAIL: statusText = "WiFi: SSID NOT FOUND"; break;
            case WL_CONNECT_FAILED: statusText = "WiFi: CONNECT FAILED"; break;
            case WL_CONNECTION_LOST: statusText = "WiFi: CONNECTION LOST"; break;
            default: statusText = "WiFi: CONNECTING"; break;
        }
        showWiFiStatus(statusText, ipv4, connected);
        displayedWiFiStatus = status;
        displayedIPv4 = ipv4;
    }

    if (connected) {
        lastWiFiAttemptAt = now;  // เริ่มนับเวลาลองใหม่หลังการเชื่อมต่อหลุด
    } else if (now - lastWiFiAttemptAt >= app::WIFI_RETRY_INTERVAL_MS) {
        Serial.println("WiFi: retrying connection...");
        startWiFiAttempt(now);
    }
}

void updateDisplay(uint16_t ldrValue, uint32_t millivolts,
                   const char *resistanceText, uint16_t ledColor)
{
    dashboard.updateLdr(ldrValue, millivolts, resistanceText, ledColor);
}

void printBanner()
{
    Serial.println();
    Serial.println("==========================================");
    Serial.printf("%s from ESP32-2432S028 (CYD)!\n", app::HELLO_TEXT);
    Serial.printf("Firmware: %s\n", app::FIRMWARE_VERSION);
    Serial.println("Mode : Touch WEATHER / LDR-RGB / DHT + swipe navigation + WiFi IPv4");
    Serial.printf("DHT%u: DATA GPIO%u, temperature C / humidity %%, interval %lu ms\n",
                  app::DHT_SENSOR_MODEL, cyd::SAFE_SENSOR_PIN,
                  static_cast<unsigned long>(app::DHT_READ_INTERVAL_MS));
    const char *calibrationName = adcCalibrationSource == ESP_ADC_CAL_VAL_EFUSE_TP ? "eFuse Two Point"
                                : adcCalibrationSource == ESP_ADC_CAL_VAL_EFUSE_VREF ? "eFuse Vref"
                                : "Default Vref (estimated)";
    Serial.printf("ADC calibration: %s\n", calibrationName);
    Serial.printf("LDR circuit: Vs = %.3f V, R15 = %.0f Ohm, R19 = %.0f Ohm\n",
                  app::LDR_SUPPLY_VOLTAGE, cyd::LDR_PULLUP_OHMS, cyd::LDR_PULLDOWN_OHMS);
    Serial.println("V = GPIO34 to GND; R~ = estimated LDR resistance; N/A = outside usable range");
    Serial.printf("LDR  : GPIO%u, 12-bit ADC (0-4095), interval %lu ms\n",
                  static_cast<unsigned int>(cyd::LDR_PIN),
                  static_cast<unsigned long>(app::LDR_READ_INTERVAL_MS));
    Serial.printf("LED  : RED > %u, GREEN %u-%u (inclusive), BLUE < %u\n",
                  static_cast<unsigned int>(app::LDR_HIGH_THRESHOLD),
                  static_cast<unsigned int>(app::LDR_LOW_THRESHOLD),
                  static_cast<unsigned int>(app::LDR_HIGH_THRESHOLD),
                  static_cast<unsigned int>(app::LDR_LOW_THRESHOLD));
    Serial.printf("Chip : %s rev %d, CPU %lu MHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                  static_cast<unsigned long>(ESP.getCpuFreqMHz()));
    Serial.printf("Flash: %lu KB, free heap: %lu bytes\n", static_cast<unsigned long>(ESP.getFlashChipSize() / 1024),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.println("==========================================");
}

void updateLightLed()
{
    const uint16_t ldrValue = analogRead(cyd::LDR_PIN);
    // แปลงค่าดิบครั้งเดียวกันเป็น mV ด้วยข้อมูล calibration ของ ADC
    const uint32_t millivolts = esp_adc_cal_raw_to_voltage(ldrValue, &adcCalibration);
    const float voltage = millivolts / 1000.0f;
    const float resistanceOhms = ldr::estimateResistanceOhms(voltage, app::LDR_SUPPLY_VOLTAGE,
                                                          cyd::LDR_PULLUP_OHMS, cyd::LDR_PULLDOWN_OHMS,
                                                          app::LDR_MIN_VALID_VOLTAGE);
    char resistanceText[24];
    formatResistance(resistanceOhms, resistanceText, sizeof(resistanceText));
    const char *ledName;
    uint16_t ledColor;

    if (ldrValue > app::LDR_HIGH_THRESHOLD) {
        if (dashboard.rgbEnabled()) cyd::ledSet(true, false, false);  // ติดเฉพาะสีแดง
        ledName = "RED";
        ledColor = TFT_RED;
    } else if (ldrValue >= app::LDR_LOW_THRESHOLD) {
        // เงื่อนไขแรกไม่ผ่าน จึงรู้แล้วว่าค่าไม่เกิน 1000
        // รวมค่าเท่ากับ 500 และ 1000 ไว้ในช่วงสีเขียว
        if (dashboard.rgbEnabled()) cyd::ledSet(false, true, false);
        ledName = "GREEN";
        ledColor = TFT_GREEN;
    } else {
        if (dashboard.rgbEnabled()) cyd::ledSet(false, false, true);  // น้อยกว่า 500: ติดเฉพาะสีน้ำเงิน
        ledName = "BLUE";
        ledColor = TFT_BLUE;
    }

    if (!dashboard.rgbEnabled()) {
        cyd::ledSet(false, false, false);
        ledName = "STOPPED";
        ledColor = TFT_LIGHTGREY;
    }
    latestLdrReading = {ldrValue, millivolts, resistanceOhms, millis(), ledName, true};
    updateDisplay(ldrValue, millivolts, resistanceText, ledColor);

    Serial.printf("LDR (GPIO%u) = %u (raw ADC)  V = %.3f V  %s  LED = %s\n",
                  static_cast<unsigned int>(cyd::LDR_PIN), static_cast<unsigned int>(ldrValue),
                  voltage, resistanceText, ledName);
}

void sendJson(int status, const JsonDocument &json)
{
    if (json.overflowed()) {
        apiServer.send(500, "application/json; charset=utf-8", "{\"error\":\"JSON capacity exceeded\"}");
        return;
    }
    String body;
    const size_t length = measureJson(json);
    body.reserve(length + 1);
    if (serializeJson(json, body) != length) {
        apiServer.send(503, "application/json; charset=utf-8", "{\"error\":\"Insufficient memory\"}");
        return;
    }
    apiServer.sendHeader("Cache-Control", "no-store");
    apiServer.send(status, "application/json; charset=utf-8", body);
}

int apiLocation()
{
    const String id = apiServer.hasArg("location") ? apiServer.arg("location") :
                      WeatherService::locationId(dashboard.selectedLocation());
    for (uint8_t i = 0; i < weather::LOCATION_COUNT; ++i) {
        if (id == WeatherService::locationId(i)) return i;
    }
    return -1;
}

void writePosition(JsonObject json, uint8_t location, const weather::Position &p)
{
    json["id"] = WeatherService::locationId(location);
    json["name"] = WeatherService::locationName(location);
    json["latitude"] = p.latitude;
    json["longitude"] = p.longitude;
    json["resolved"] = weather::validPosition(p);
    json["approximate"] = p.approximate;
    json["position_source"] = location == 2 ? "wifi_public_ip" : "city_centre";
    json["city"] = p.city;
    json["timezone"] = p.timezone;
    json["utc_offset_seconds"] = p.utcOffsetSeconds;
    if (weather::validPosition(p) && time(nullptr) >= 1704067200) {
        const time_t local = time(nullptr) + p.utcOffsetSeconds;
        tm fields;
        gmtime_r(&local, &fields);
        char text[32];
        strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &fields);
        json["local_time"] = text;
    } else json["local_time"] = nullptr;
}

void apiWeather()
{
    const int location = apiLocation();
    if (location < 0) {
        StaticJsonDocument<128> error;
        error["error"] = "Unknown location; use chiang_mai, bangkok or home";
        sendJson(400, error);
        return;
    }
    weatherService.request(location);
    const auto &data = weatherService.get(location);
    const auto &c = data.conditions;
    DynamicJsonDocument json(api::WEATHER_JSON_CAPACITY);
    writePosition(json.createNestedObject("location"), location, data.position);
    json["pending"] = data.pending;
    json["error"] = data.error;
    json["source"]["weather"] = "OpenWeather";
    json["source"]["air_quality"] = "OpenWeather Air Pollution";
    json["source"]["type"] = "current API data";
    auto status = json.createNestedObject("status");
    status["weather_available"] = c.valid;
    status["air_available"] = data.air.valid;
    status["weather_stale"] = c.valid && (data.weatherFailed || WiFi.status() != WL_CONNECTED ||
        millis() - c.receivedAtMs > weather::REFRESH_INTERVAL_MS + 60000);
    status["air_stale"] = data.air.valid && (data.airFailed || WiFi.status() != WL_CONNECTED ||
        millis() - data.air.receivedAtMs > weather::REFRESH_INTERVAL_MS + 60000);
    status["weather_data_time_utc"] = c.valid ? c.timestamp : 0;
    status["air_data_time_utc"] = data.air.valid ? data.air.timestamp : 0;
    auto conditions = json.createNestedObject("weather");
    conditions["temperature_c"] = c.temperature;
    conditions["relative_humidity_percent"] = c.humidity;
    if (c.valid) {
        conditions["code"] = c.code;
        conditions["description"] = weather::conditionName(c.code);
    } else {
        conditions["code"] = nullptr;
        conditions["description"] = nullptr;
    }
    auto rain = json.createNestedObject("rain_cloud");
    rain["rain_rate_mm_per_hour"] = c.rain;
    rain["rain_definition"] = "OpenWeather rain.1h (mm/h); absent rain means zero";
    rain["cloud_cover_percent"] = c.clouds;
    auto wind = json.createNestedObject("wind");
    wind["speed_kmh"] = c.windSpeed;
    wind["from_degrees"] = c.windDirection;
    wind["from_compass"] = weather::windCompass(c.windDirection);
    auto air = json.createNestedObject("air_quality");
    air["pm2_5_ug_m3"] = data.air.pm25;
    air["interval_seconds"] = nullptr;  // Provider does not supply an averaging interval.
    const int httpStatus = (c.valid || data.air.valid) ? 200 : data.pending ? 202 : 503;
    sendJson(httpStatus, json);
}

void apiRgb()
{
    if (apiServer.method() == HTTP_POST) {
        const String body = apiServer.arg("plain");
        bool enabled;
        if (!parseRgbCommand(body.c_str(), body.length(), enabled)) {
            StaticJsonDocument<128> error;
            error["error"] = "Expected JSON: {\"enabled\":true} or {\"enabled\":false}";
            sendJson(400, error);
            return;
        }
        dashboard.setRgbEnabled(enabled);
        updateLightLed();
        lastRgbEnabled = enabled;
        lastLdrReadAt = millis();
    }
    StaticJsonDocument<192> json;
    json["enabled"] = dashboard.rgbEnabled();
    json["mode"] = dashboard.rgbEnabled() ? "ldr_automatic" : "stopped";
    json["low_threshold"] = app::LDR_LOW_THRESHOLD;
    json["high_threshold"] = app::LDR_HIGH_THRESHOLD;
    sendJson(200, json);
}

void apiSensors()
{
    StaticJsonDocument<api::SENSORS_JSON_CAPACITY> json;
    const uint32_t now = millis();
    const auto &reading = dhtSensor.get();
    const bool attempted = reading.status != dhtsensor::Status::Waiting;
    const uint32_t age = now - reading.attemptedAtMs;
    const bool stale = reading.status == dhtsensor::Status::Ready && age > app::DHT_READ_INTERVAL_MS + 1000;
    const bool valid = reading.status == dhtsensor::Status::Ready && !stale;
    auto dht = json.createNestedObject("dht");
    dht["model"] = app::DHT_SENSOR_MODEL;
    dht["pin"] = cyd::SAFE_SENSOR_PIN;
    dht["status"] = stale ? "stale" : valid ? "ready" : attempted ? "unavailable" : "waiting";
    dht["valid"] = valid;
    if (valid) {
        dht["temperature_c"] = reading.temperature;
        dht["relative_humidity_percent"] = reading.humidity;
    } else {
        dht["temperature_c"] = nullptr;
        dht["relative_humidity_percent"] = nullptr;
    }
    if (attempted) dht["age_ms"] = age;
    else dht["age_ms"] = nullptr;
    auto light = json.createNestedObject("ldr");
    const auto &ldrReading = latestLdrReading;
    const bool lightValid = ldrReading.valid && now - ldrReading.receivedAtMs <= app::LDR_READ_INTERVAL_MS + 1000;
    light["pin"] = cyd::LDR_PIN;
    light["valid"] = lightValid;
    if (lightValid) {
        light["raw_adc"] = ldrReading.raw;
        light["millivolts"] = ldrReading.millivolts;
        light["resistance_ohms"] = ldrReading.resistanceOhms;
    } else {
        light["raw_adc"] = nullptr;
        light["millivolts"] = nullptr;
        light["resistance_ohms"] = nullptr;
    }
    if (ldrReading.valid) light["age_ms"] = now - ldrReading.receivedAtMs;
    else light["age_ms"] = nullptr;
    auto rgb = json.createNestedObject("rgb");
    rgb["enabled"] = dashboard.rgbEnabled();
    rgb["mode"] = dashboard.rgbEnabled() ? "ldr_automatic" : "stopped";
    rgb["led_color"] = dashboard.rgbEnabled() && lightValid ? ldrReading.ledName : "STOPPED";
    sendJson(200, json);
}

void initApi()
{
    apiServer.on("/", HTTP_GET, []() {
        apiServer.sendHeader("Cache-Control", "no-cache");
        apiServer.sendHeader("Content-Encoding", "gzip");
        apiServer.send_P(200, "text/html; charset=utf-8",
                         reinterpret_cast<const char *>(webdashboard::PAGE_GZIP), webdashboard::PAGE_GZIP_SIZE);
    });
    apiServer.on("/api/sensors", HTTP_GET, apiSensors);
    apiServer.on("/api/weather", HTTP_GET, apiWeather);
    apiServer.on("/api/rgb", HTTP_GET, apiRgb);
    apiServer.on("/api/rgb", HTTP_POST, apiRgb);
    apiServer.on("/api/locations", HTTP_GET, []() {
        DynamicJsonDocument json(api::LOCATIONS_JSON_CAPACITY);
        auto locations = json.createNestedArray("locations");
        for (uint8_t i = 0; i < weather::LOCATION_COUNT; ++i) {
            writePosition(locations.createNestedObject(), i, weatherService.get(i).position);
        }
        sendJson(200, json);
    });
    apiServer.on("/api/status", HTTP_GET, []() {
        StaticJsonDocument<384> json;
        json["firmware_version"] = app::FIRMWARE_VERSION;
        json["heap_free_bytes"] = ESP.getFreeHeap();
        json["heap_min_free_bytes"] = ESP.getMinFreeHeap();
        json["heap_largest_block_bytes"] = ESP.getMaxAllocHeap();
        const uint32_t stackFree = weatherService.workerStackMinFreeBytes();
        if (stackFree) json["weather_stack_min_free_bytes"] = stackFree;
        else json["weather_stack_min_free_bytes"] = nullptr;
        json["wifi_connected"] = WiFi.status() == WL_CONNECTED;
        json["ipv4"] = WiFi.localIP().toString();
        json["clock_synchronized"] = time(nullptr) >= 1704067200;
        json["weather_worker_ready"] = weatherService.ready();
        json["rgb_enabled"] = dashboard.rgbEnabled();
        sendJson(200, json);
    });
    apiServer.onNotFound([]() {
        StaticJsonDocument<192> json;
        json["error"] = "Not found";
        json["weather_endpoint"] = "/api/weather?location=chiang_mai";
        sendJson(404, json);
    });
    apiServer.begin();
    Serial.printf("Web dashboard ready: http://%s/\n", WiFi.localIP().toString().c_str());
}

}  // namespace

void setup()
{
    Serial.begin(app::SERIAL_BAUD);
    delay(200);  // รอให้ Serial พร้อมก่อนพิมพ์ข้อความแรก

    pinMode(cyd::LDR_PIN, INPUT);
    analogReadResolution(12);  // 12 บิต: ค่าดิบอยู่ในช่วง 0 ถึง 4095
    analogSetPinAttenuation(cyd::LDR_PIN, ADC_11db);
    adcCalibrationSource = esp_adc_cal_characterize(ADC_UNIT_1, static_cast<adc_atten_t>(ADC_11db), ADC_WIDTH_BIT_12,
                                                   app::ADC_DEFAULT_VREF_MV, &adcCalibration);

    printBanner();

    cyd::ledInit();
    initDisplay();
    updateLightLed();  // อ่านค่าและเลือกสีแรกทันทีหลังเริ่มโปรแกรม
    lastLdrReadAt = millis();
    lastRgbEnabled = dashboard.rgbEnabled();
    dhtSensor.begin(millis());
    dashboard.updateDht(dhtSensor.get());
    if (!weatherService.begin()) Serial.println("Weather worker could not start.");
    initWiFi();
}

void loop()
{
    const uint32_t now = millis();
    if (dhtSensor.poll(now)) dashboard.updateDht(dhtSensor.get());
    if (now - lastLdrReadAt >= app::LDR_READ_INTERVAL_MS) {
        lastLdrReadAt = now;
        updateLightLed();
    }
    updateWiFi(now);
    const bool connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress();
    if (connected && !wasNetworkConnected) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        weatherService.invalidateHome();
        if (!apiStarted) { initApi(); apiStarted = true; }
    }
    wasNetworkConnected = connected;
    weatherService.poll();
    if (now - lastWeatherScheduleAt >= weather::SCHEDULE_INTERVAL_MS) {
        lastWeatherScheduleAt = now;
        const uint8_t selected = dashboard.selectedLocation();
        weatherService.request(selected);
        for (uint8_t i = 0; i < weather::LOCATION_COUNT; ++i) {
            if (i != selected) weatherService.request(i);
        }
    }
    if (apiStarted) apiServer.handleClient();
    dashboard.poll(now, weatherService);
    if (lastRgbEnabled != dashboard.rgbEnabled()) {
        lastRgbEnabled = dashboard.rgbEnabled();
        updateLightLed();  // START/STOP มีผลทันที ไม่ต้องรอรอบ 500 ms
        lastLdrReadAt = millis();
    }
    delay(1);  // ให้ task ระบบทำงาน ระหว่างรอสัมผัส/ข้อมูลเครือข่าย
}
