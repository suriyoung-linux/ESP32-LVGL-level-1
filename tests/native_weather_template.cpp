
#include <cassert>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "app_config.h"
#include "board_config.h"
#include "ldr_measurement.h"
#include "weather_config.h"
#include "weather_data.h"
#include "touch_calibration.h"
#include "api_contract.h"
using String=std::string;
std::map<int,int> pins;
unsigned writes=0;
void digitalWrite(int pin,int value){pins[pin]=value;++writes;}
void pinMode(int,int){}
uint32_t fakeNow=100000;uint16_t fakeRaw=700;
uint32_t millis(){return fakeNow;}
uint16_t analogRead(uint8_t pin){assert(pin==34);return fakeRaw;}
struct Calibration {} adcCalibration;
uint32_t esp_adc_cal_raw_to_voltage(uint16_t raw,Calibration*){return raw;}
struct FakeSerial {template<class...Args>void printf(const char*,Args...){} } Serial;
struct FakeDashboard {
    bool enabled=true;
    bool rgbEnabled()const{return enabled;}
    void setRgbEnabled(bool b){enabled=b;}
    uint8_t selectedLocation()const{return 0;}
} dashboard;
bool lastRgbEnabled=true;
uint32_t lastLdrReadAt=0;
uint16_t latestColor=0;
constexpr uint16_t TFT_RED=1,TFT_GREEN=2,TFT_BLUE=3,TFT_LIGHTGREY=4;
void updateDisplay(uint16_t raw,uint32_t mv,const char*,uint16_t color){assert(raw==fakeRaw && mv==raw);latestColor=color;}
struct IPAddress {String toString()const{return "192.168.1.123";}};
constexpr int WL_CONNECTED=3;
struct FakeWifi {int state=3;int status(){return state;}IPAddress localIP(){return {};}}WiFi;
constexpr int HTTP_GET=0,HTTP_POST=1;
struct FakeServer {
    int methodValue=HTTP_GET,httpStatus=0;
    String location,plain,response;
    std::map<std::pair<String,int>,std::function<void()>> handlers;
    std::function<void()> missing;
    bool hasArg(const char *key){return String(key)=="location" && !location.empty();}
    String arg(const char *key){return String(key)=="plain" ? plain : location;}
    int method(){return methodValue;}
    void sendHeader(const char*,const char*){}
    void send(int status,const char*,const String &body){httpStatus=status;response=body;}
    template<class Handler>void on(const char *path,int method,Handler handler){handlers[{path,method}]=handler;}
    template<class Handler>void onNotFound(Handler handler){missing=handler;}
    void begin(){}
} apiServer;
class WeatherService {
public:
    weather::Snapshot cache[3];
    bool request(uint8_t loc){if(!cache[loc].conditions.valid && !cache[loc].air.valid)cache[loc].pending=true;return true;}
    const weather::Snapshot &get(uint8_t loc){return cache[loc];}
    bool ready(){return true;}
    static const char *locationId(uint8_t loc){static const char *names[]={"chiang_mai","bangkok","home"};return names[loc];}
    static const char *locationName(uint8_t loc){static const char *names[]={"Chiang Mai","Bangkok","Home"};return names[loc];}
} weatherService;

// @FIRMWARE_FUNCTIONS@
std::string read(const char *path){std::ifstream f(path);std::stringstream s;s<<f.rdbuf();return s.str();}
DynamicJsonDocument response(){DynamicJsonDocument d(4096);assert(!deserializeJson(d,apiServer.response));return d;}
int main(){
    DynamicJsonDocument d(4096), air(4096);
    assert(!deserializeJson(d,read("tests/fixtures/weather.json")));
    assert(!deserializeJson(air,read("tests/fixtures/air.json")));
    weather::Conditions conditions;weather::AirQuality pm;weather::Position p;
    p.latitude=18.7883;p.longitude=98.9853;
    assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    assert(conditions.valid && conditions.intervalSeconds==900 && p.utcOffsetSeconds==25200);
    assert(weather::parseAir(air.as<JsonObjectConst>(),pm) && pm.valid && pm.intervalSeconds==3600);
    auto original=read("tests/fixtures/weather.json");
    for(const char *key:{"temperature_2m","relative_humidity_2m","rain","showers","cloud_cover","wind_speed_10m","wind_direction_10m"}){
        deserializeJson(d,original);d["current"][key]=nullptr;weather::Conditions bad;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),bad,p) && !bad.valid);
    }
    deserializeJson(d,original);d["current"]["relative_humidity_2m"]=101;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["current"]["rain"]=1.25;d["current"]["showers"]=2.5;
    assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p) && conditions.rain==3.75f);
    deserializeJson(d,original);d["utc_offset_seconds"]=INT32_MIN;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["current"]["temperature_2m"]=1e100;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    air["current"]["pm2_5"]=nullptr;weather::AirQuality invalidAir;
    assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir) && !invalidAir.valid);
    air["current"]["pm2_5"]=-1;assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    air["current"]["pm2_5"]=1e100;assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    DynamicJsonDocument geo(1024);
    deserializeJson(geo,R"({"success":true,"latitude":13.7563,"longitude":100.5018,"city":"Bangkok","timezone":{"id":"Asia/Bangkok","offset":25200}})");
    weather::Position home;assert(weather::parsePosition(geo.as<JsonObjectConst>(),home) && home.approximate);
    geo["success"]=false;assert(!weather::parsePosition(geo.as<JsonObjectConst>(),home));
    geo["success"]=true;geo["latitude"]=91;assert(!weather::parsePosition(geo.as<JsonObjectConst>(),home));
    assert(std::string(weather::windCompass(0))=="N" && std::string(weather::windCompass(360))=="N");
    assert(std::string(weather::windCompass(90))=="E" && std::string(weather::windCompass(332))=="NNW");
    assert(std::string(weather::windCompass(NAN))=="N/A");
    for(const auto &points:std::vector<std::vector<touchcal::Point>>{
        {{400,600},{3600,600},{400,3500}},{{3600,3500},{400,3500},{3600,600}},{{600,3600},{600,400},{3500,3600}}}){
        touchcal::Calibration cal;assert(touchcal::fit(points.data(),cal));
        auto a=cal.project(points[0]);auto b=cal.project(points[1]);auto c=cal.project(points[2]);
        assert(std::abs(a.x-20)<.01 && std::abs(a.y-20)<.01 && std::abs(b.x-300)<.01 && std::abs(c.y-220)<.01);
    }
    touchcal::Point same[]={{500,500},{500,500},{500,500}};touchcal::Calibration cal;
    assert(!touchcal::fit(same,cal) && !cal.valid());
    for(auto sample:std::vector<std::pair<uint16_t,uint16_t>>{{499,TFT_BLUE},{500,TFT_GREEN},{999,TFT_GREEN},{1000,TFT_GREEN},{1001,TFT_RED}}){
        fakeRaw=sample.first;dashboard.enabled=true;writes=0;updateLightLed();assert(latestColor==sample.second && writes==3);
        unsigned on=0;for(int pin:{cyd::LED_PIN_R,cyd::LED_PIN_G,cyd::LED_PIN_B})on+=pins[pin]==LOW;assert(on==1);
    }
    dashboard.enabled=false;writes=0;updateLightLed();assert(writes==3 && latestColor==TFT_LIGHTGREY);
    for(int pin:{cyd::LED_PIN_R,cyd::LED_PIN_G,cyd::LED_PIN_B})assert(pins[pin]==HIGH);
    bool enabled=true;assert(parseRgbCommand("{\"enabled\":false}",17,enabled) && !enabled);
    for(const auto &bad:std::vector<std::string>{"", "{}", "not json", "{\"enabled\":1}", "{\"enabled\":\"false\"}", "null", "[]"}){
        bool unchanged=true;assert(!parseRgbCommand(bad.c_str(),bad.size(),unchanged) && unchanged);
    }
    initApi();assert(apiServer.handlers.size()==5);
    apiServer.location="wrong";apiWeather();assert(apiServer.httpStatus==400);
    apiServer.location="chiang_mai";apiWeather();assert(apiServer.httpStatus==202);
    auto cold=response();assert(cold["weather"]["temperature_c"].isNull());
    weatherService.cache[0].conditions=conditions;weatherService.cache[0].air=pm;weatherService.cache[0].position=p;
    weatherService.cache[0].conditions.receivedAtMs=fakeNow;weatherService.cache[0].air.receivedAtMs=fakeNow;
    weatherService.cache[0].pending=false;
    apiWeather();assert(apiServer.httpStatus==200);auto good=response();
    assert(good["weather"]["temperature_c"].as<float>()==conditions.temperature);
    assert(good["air_quality"]["pm2_5_ug_m3"].as<float>()==pm.pm25 && good["location"]["local_time"].is<const char*>());
    weatherService.cache[0].weatherFailed=true;apiWeather();auto stale=response();assert(stale["status"]["weather_stale"]==true);
    apiServer.methodValue=HTTP_POST;apiServer.plain="{\"enabled\":false}";apiRgb();assert(apiServer.httpStatus==200 && !dashboard.enabled);
    for(int pin:{cyd::LED_PIN_R,cyd::LED_PIN_G,cyd::LED_PIN_B})assert(pins[pin]==HIGH);
    apiServer.plain="{\"enabled\":\"false\"}";apiRgb();assert(apiServer.httpStatus==400 && !dashboard.enabled);
    apiServer.plain="{\"enabled\":true}";apiRgb();assert(apiServer.httpStatus==200 && dashboard.enabled);
    apiServer.handlers.at({"/api/locations",HTTP_GET})();auto locations=response();assert(locations["locations"].size()==3);
    apiServer.missing();assert(apiServer.httpStatus==404);
    puts("PASS: live provider fixtures, null/range rejection, rain interval, IP position validation, compass, touch transforms, GPIO thresholds/STOP, RGB request validation and actual JSON API handlers");
}

