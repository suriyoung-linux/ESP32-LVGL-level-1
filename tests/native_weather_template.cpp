
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
#include "weather_json.h"
#include "touch_calibration.h"
#include "api_contract.h"
#include "dht_reading.h"
#include "ldr_reading.h"
#include "web_dashboard.h"
using String=std::string;
std::map<int,int> pins;
unsigned writes=0;
void digitalWrite(int pin,int value){pins[pin]=value;++writes;}
void pinMode(int,int){}
struct FakeEsp { unsigned getFreeHeap(){return 123456;} unsigned getMinFreeHeap(){return 100000;} unsigned getMaxAllocHeap(){return 90000;} } ESP;
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
ldr::Reading latestLdrReading;
struct FakeDhtSensor {dhtsensor::Reading reading;const dhtsensor::Reading &get(){return reading;}} dhtSensor;
constexpr uint16_t TFT_RED=1,TFT_GREEN=2,TFT_BLUE=3,TFT_LIGHTGREY=4;
void updateDisplay(uint16_t raw,uint32_t mv,const char*,uint16_t color){assert(raw==fakeRaw && mv==raw);latestColor=color;}
struct IPAddress {String toString()const{return "192.168.1.123";}};
constexpr int WL_CONNECTED=3;
struct FakeWifi {int state=3;int status(){return state;}IPAddress localIP(){return {};}}WiFi;
constexpr int HTTP_GET=0,HTTP_POST=1;
struct FakeServer {
    int methodValue=HTTP_GET,httpStatus=0;
    String location,plain,response;
    std::map<String,String> headers;
    std::map<std::pair<String,int>,std::function<void()>> handlers;
    std::function<void()> missing;
    bool hasArg(const char *key){return String(key)=="location" && !location.empty();}
    String arg(const char *key){return String(key)=="plain" ? plain : location;}
    int method(){return methodValue;}
    void sendHeader(const char* key,const char* value){headers[key]=value;}
    void send(int status,const char*,const String &body){httpStatus=status;response=body;}
    void send_P(int status,const char*,const char* body,size_t size){httpStatus=status;response.assign(body,size);}
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
    uint32_t stackFree=4096;
    uint32_t workerStackMinFreeBytes(){return stackFree;}
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
    assert(conditions.valid && p.utcOffsetSeconds==25200);
    assert(std::abs(conditions.windSpeed-d["wind"]["speed"].as<float>()*3.6f)<0.001);
    assert(weather::parseAir(air.as<JsonObjectConst>(),pm) && pm.valid);
    // Exercise the same bounded, stream-mode filtering used on the ESP32.
    for(auto fixture:std::vector<std::pair<const char*,weather::ResponseKind>>{
        {"tests/fixtures/weather.json",weather::ResponseKind::Conditions},
        {"tests/fixtures/air.json",weather::ResponseKind::Air}}){
        StaticJsonDocument<weather::FILTER_JSON_CAPACITY> filter;
        weather::responseFilter(filter,fixture.second);assert(!filter.overflowed());
        DynamicJsonDocument small(weather::PROVIDER_JSON_CAPACITY);
        std::istringstream stream(read(fixture.first));
        assert(!deserializeJson(small,stream,DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(8)));
        assert(!small.overflowed());
        if(fixture.second==weather::ResponseKind::Conditions){
            weather::Conditions filtered;weather::Position position=p;
            assert(weather::parseConditions(small.as<JsonObjectConst>(),filtered,position));
            assert(filtered.temperature==conditions.temperature && filtered.windSpeed==conditions.windSpeed);
            assert(!small.containsKey("sys") && !small["main"].containsKey("pressure"));
            for(const char* rain:{"null","{}","\"invalid\"","{\"1h\":null}"}){
                std::string bad="{\"main\":{\"temp\":25,\"humidity\":50},\"clouds\":{\"all\":0},"
                    "\"wind\":{\"speed\":1},\"weather\":[{\"id\":800}],\"dt\":1791720000,\"timezone\":25200,\"cod\":200,\"rain\":";
                std::istringstream input(bad+rain+"}");
                assert(!deserializeJson(small,input,DeserializationOption::Filter(filter)));
                assert(!weather::parseConditions(small.as<JsonObjectConst>(),filtered,position));
            }
        }else{
            weather::AirQuality filtered;assert(weather::parseAir(small.as<JsonObjectConst>(),filtered));
            assert(filtered.pm25==pm.pm25 && !small["list"][0]["components"].containsKey("co"));
        }
    }
    auto original=read("tests/fixtures/weather.json");
    for(const char *key:{"temp","humidity"}){
        deserializeJson(d,original);d["main"][key]=nullptr;weather::Conditions bad;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),bad,p) && !bad.valid);
    }
    for(const char *key:{"wind","clouds","weather","dt","timezone","cod"}){
        deserializeJson(d,original);d[key]=nullptr;weather::Conditions bad;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),bad,p) && !bad.valid);
    }
    deserializeJson(d,original);d["main"]["humidity"]=101;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["rain"]["1h"]=1.25;
    assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p) && conditions.rain==1.25f);
    d.remove("rain");assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p) && conditions.rain==0);
    d["rain"]=nullptr;assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    for(double value:{-1.0,1e100}){
        deserializeJson(d,original);d["rain"]["1h"]=value;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    }
    deserializeJson(d,original);d["wind"].remove("deg");
    assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p) && std::isnan(conditions.windDirection));
    d["wind"]["deg"]=nullptr;assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    for(double value:{-1.0,361.0,1e100}){
        deserializeJson(d,original);d["wind"]["deg"]=value;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    }
    for(double value:{-1.0,1e100}){
        deserializeJson(d,original);d["wind"]["speed"]=value;
        assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    }
    deserializeJson(d,original);d["clouds"]["all"]=101;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["weather"][0]["id"]="800";
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["cod"]=401;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["dt"]=0;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["timezone"]=INT32_MIN;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);d["main"]["temp"]=1e100;
    assert(!weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    deserializeJson(d,original);assert(weather::parseConditions(d.as<JsonObjectConst>(),conditions,p));
    air["list"][0]["components"]["pm2_5"]=nullptr;weather::AirQuality invalidAir;
    assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir) && !invalidAir.valid);
    air["list"][0]["components"]["pm2_5"]=-1;assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    air["list"][0]["components"]["pm2_5"]=1e100;assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    deserializeJson(air,read("tests/fixtures/air.json"));air["list"][0]["dt"]=0;
    assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    air["list"].clear();assert(!weather::parseAir(air.as<JsonObjectConst>(),invalidAir));
    assert(std::string(weather::conditionName(800))=="Clear sky");
    assert(std::string(weather::conditionName(804))=="Overcast");
    assert(std::string(weather::conditionName(501))=="Moderate rain");
    assert(std::string(weather::conditionName(211))=="Thunderstorm");
    assert(std::string(weather::conditionName(0))=="Unknown condition");
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
    initApi();assert(apiServer.handlers.size()==7);
    apiServer.handlers.at({"/",HTTP_GET})();assert(apiServer.httpStatus==200);
    assert(apiServer.headers["Content-Encoding"]=="gzip" && apiServer.response.size()==webdashboard::PAGE_GZIP_SIZE);
    assert(static_cast<unsigned char>(apiServer.response[0])==0x1f && static_cast<unsigned char>(apiServer.response[1])==0x8b);
    latestLdrReading={};apiSensors();auto waiting=response();
    assert(waiting["dht"]["status"]=="waiting" && waiting["dht"]["age_ms"].isNull());
    assert(waiting["dht"]["temperature_c"].isNull() && waiting["ldr"]["raw_adc"].isNull());
    dashboard.enabled=true;
    dhtSensor.reading={dhtsensor::Status::Ready,26.5f,61.0f,fakeNow};
    latestLdrReading={700,700,12345.0f,fakeNow,"GREEN",true};apiSensors();auto sensorGood=response();
    assert(sensorGood["dht"]["valid"]==true && sensorGood["dht"]["temperature_c"]==26.5f);
    assert(sensorGood["dht"]["relative_humidity_percent"]==61.0f && sensorGood["dht"]["pin"]==27);
    assert(sensorGood["ldr"]["raw_adc"]==700 && sensorGood["ldr"]["resistance_ohms"]==12345.0f);
    assert(sensorGood["rgb"]["led_color"]=="GREEN");
    fakeNow+=4000;apiSensors();auto sensorStale=response();
    assert(sensorStale["dht"]["status"]=="stale" && sensorStale["dht"]["temperature_c"].isNull());
    assert(sensorStale["ldr"]["valid"]==false && sensorStale["ldr"]["millivolts"].isNull());
    dhtSensor.reading={dhtsensor::Status::Unavailable,NAN,NAN,fakeNow};apiSensors();
    auto sensorBad=response();assert(sensorBad["dht"]["status"]=="unavailable");
    assert(sensorBad["dht"]["temperature_c"].isNull() && sensorBad["dht"]["relative_humidity_percent"].isNull());
    fakeNow=10;dhtSensor.reading={dhtsensor::Status::Ready,25.0f,55.0f,UINT32_MAX-100};
    latestLdrReading={0,0,NAN,UINT32_MAX-100,"BLUE",true};apiSensors();auto wrapped=response();
    assert(wrapped["dht"]["valid"]==true && wrapped["dht"]["age_ms"]==111);
    assert(wrapped["ldr"]["raw_adc"]==0 && wrapped["ldr"]["resistance_ohms"].isNull());
    fakeNow=100000;
    apiServer.location="wrong";apiWeather();assert(apiServer.httpStatus==400);
    apiServer.location="chiang_mai";apiWeather();assert(apiServer.httpStatus==202);
    auto cold=response();assert(cold["weather"]["temperature_c"].isNull());
    weatherService.cache[0].conditions=conditions;weatherService.cache[0].air=pm;weatherService.cache[0].position=p;
    weatherService.cache[0].conditions.receivedAtMs=fakeNow;weatherService.cache[0].air.receivedAtMs=fakeNow;
    weatherService.cache[0].pending=false;
    apiWeather();assert(apiServer.httpStatus==200);auto good=response();
    assert(good["weather"]["temperature_c"].as<float>()==conditions.temperature);
    assert(good["source"]["weather"]=="OpenWeather");
    assert(good["source"]["air_quality"]=="OpenWeather Air Pollution");
    assert(good["status"]["weather_data_time_utc"].as<uint32_t>()==conditions.timestamp);
    assert(good["status"]["air_data_time_utc"].as<uint32_t>()==pm.timestamp);
    assert(good["rain_cloud"]["rain_rate_mm_per_hour"].as<float>()==conditions.rain);
    assert(good["air_quality"]["interval_seconds"].isNull());
    weatherService.cache[0].conditions.windDirection=NAN;apiWeather();auto noDirection=response();
    assert(noDirection["wind"]["from_degrees"].isNull() && noDirection["wind"]["from_compass"]=="N/A");
    assert(good["air_quality"]["pm2_5_ug_m3"].as<float>()==pm.pm25 && good["location"]["local_time"].is<const char*>());
    weatherService.cache[0].weatherFailed=true;apiWeather();auto stale=response();assert(stale["status"]["weather_stale"]==true);
    apiServer.methodValue=HTTP_POST;apiServer.plain="{\"enabled\":false}";apiRgb();assert(apiServer.httpStatus==200 && !dashboard.enabled);
    for(int pin:{cyd::LED_PIN_R,cyd::LED_PIN_G,cyd::LED_PIN_B})assert(pins[pin]==HIGH);
    apiServer.plain="{\"enabled\":\"false\"}";apiRgb();assert(apiServer.httpStatus==400 && !dashboard.enabled);
    apiServer.plain="{\"enabled\":true}";apiRgb();assert(apiServer.httpStatus==200 && dashboard.enabled);
    apiServer.handlers.at({"/api/locations",HTTP_GET})();auto locations=response();assert(locations["locations"].size()==3);
    apiServer.handlers.at({"/api/status",HTTP_GET})();auto health=response();
    assert(health["firmware_version"]==app::FIRMWARE_VERSION);
    assert(health["heap_free_bytes"]==123456 && health["weather_stack_min_free_bytes"]==4096);
    weatherService.stackFree=0;apiServer.handlers.at({"/api/status",HTTP_GET})();
    assert(response()["weather_stack_min_free_bytes"].isNull());
    StaticJsonDocument<16> tooSmall;tooSmall["large"]["nested"]=true;
    assert(tooSmall.overflowed());sendJson(200,tooSmall);assert(apiServer.httpStatus==500);
    // Maximum copied position/error strings must fit the calculated response capacity.
    for(auto &entry:weatherService.cache){
        memset(entry.position.city,'C',sizeof(entry.position.city)-1);
        memset(entry.position.timezone,'T',sizeof(entry.position.timezone)-1);
        memset(entry.error,'E',sizeof(entry.error)-1);
    }
    apiWeather();assert(apiServer.httpStatus==200);
    apiServer.handlers.at({"/api/locations",HTTP_GET})();assert(apiServer.httpStatus==200);
    auto fullLocations=response();assert(fullLocations["locations"].size()==3);
    apiServer.missing();assert(apiServer.httpStatus==404);
    puts("PASS: live OpenWeather fixtures, null/range rejection, rain rate and wind conversion, IP position validation, compass, touch transforms, GPIO thresholds/STOP, RGB request validation and actual JSON API handlers");
}

