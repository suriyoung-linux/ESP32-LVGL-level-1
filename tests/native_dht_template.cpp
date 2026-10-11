#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstring>
#include "weather_config.h"
#include "weather_data.h"
#include <string>
#include <vector>
#include "dht_sensor.h"
#include "touch_gesture.h"
#include "touch_calibration.h"

uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }
FakeSerial Serial;
constexpr uint16_t TFT_BLACK=0, TFT_RED=1, TFT_WHITE=2, TFT_LIGHTGREY=3,
                   TFT_YELLOW=4, TFT_GREEN=5;
// @CONSTANTS@

struct WeatherService {
    unsigned requests = 0;
    weather::Snapshot snapshot;
    const weather::Snapshot &get(uint8_t){return snapshot;}
    bool ready(){return true;}
    void request(uint8_t, bool force) { assert(force); ++requests; }
};
struct TouchPoint { int x=0, y=0, z=500; };
struct FakeTouch {
    bool down=false;
    TouchPoint point;
    bool touched() { return down; }
    TouchPoint getPoint() { return point; }
};
struct FakePreferences { void putBytes(const char*, const void*, size_t) {} };
struct FakeTft { int width=0,height=0; void fillRect(int,int,int w,int h,int){width=w;height=h;} };
struct Dashboard {
    enum class Page { Weather, Rgb, Dht };
    Page page_=Page::Weather;
    FakeTouch touch_;
    FakePreferences preferences_;
    FakeTft tft_;
    touchcal::Calibration calibration_{1,0,0,0,1,0};
    touchcal::Point calibrationPoints_[4];
    touchgesture::Tracker gesture_;
    dhtsensor::Reading dhtReading_;
    bool preferencesReady_=false, calibrating_=false, waitRelease_=false, touchHeld_=false;
    bool rgbEnabled_=true, layoutDirty_=false, dataDirty_=false;
    uint8_t location_=0, group_=0, calibrationStep_=0, calibrationSamples_=0;
    uint32_t lastTouchAt_=0, lastFrameAt_=0, lastWeatherRevision_=UINT32_MAX;
    bool connected_=true,lastWeatherStale_=false;
    unsigned weatherDraws=0,rgbDraws=0;
    uint16_t raw_=0,ledColor_=0;uint32_t millivolts_=0;char resistance_[24]="";
    float calibrationSumX_=0, calibrationSumY_=0;
    std::vector<std::string> labels;
    void beginCalibration() { calibrating_=true; gesture_.cancel(); }
    void drawCalibrationTarget() {}
    void setRgbEnabled(bool value) { rgbEnabled_=value; }
    void label(const char* text,int,int,uint16_t=0,uint8_t=2) { labels.emplace_back(text); }
    void weatherLabel(const char* text,int,uint16_t=0) { labels.emplace_back(text); }
    void pollTouch(uint32_t, WeatherService&);
    void tap(int16_t,int16_t,WeatherService&);
    void swipe(touchgesture::Kind);
    void drawDht();
    void drawDhtAge();
    void updateDht(const dhtsensor::Reading&);
    void updateLdr(uint16_t,uint32_t,const char*,uint16_t);
    void drawLayout(){layoutDirty_=false;dataDirty_=true;}
    void drawWeather(const weather::Snapshot&,bool){++weatherDraws;}
    void drawWeatherClock(const weather::Snapshot&);
    void drawRgb(){++rgbDraws;}
    void poll(uint32_t,WeatherService&);
};
// @DASHBOARD_FUNCTIONS@

void input(Dashboard &screen, WeatherService &weather, bool down, int x=0, int y=0)
{
    fakeNow += 20;
    screen.touch_.down = down;
    screen.touch_.point = {x,y,500};
    screen.pollTouch(fakeNow, weather);
}
bool contains(const Dashboard &screen, const char *text)
{
    return std::find(screen.labels.begin(),screen.labels.end(),text) != screen.labels.end();
}
int main()
{
    DhtSensor sensor;
    assert(!sensor.poll(2000));
    sensor.begin(0);
    assert(sensor.get().status==dhtsensor::Status::Waiting);
    assert(!sensor.poll(1999) && DHT::reads==0);
    fakeNow=2000;
    assert(sensor.poll(fakeNow) && DHT::reads==1);
    assert(sensor.get().status==dhtsensor::Status::Unavailable);
    assert(std::isnan(sensor.get().temperature) && std::isnan(sensor.get().humidity));
    assert(!sensor.poll(3999) && DHT::reads==1);
    DHT::temperature=26.5; DHT::humidity=61.0; fakeNow=4000;
    assert(sensor.poll(fakeNow) && DHT::reads==2);
    assert(sensor.get().status==dhtsensor::Status::Ready);
    assert(sensor.get().temperature==26.5 && sensor.get().humidity==61.0);
    const auto good=sensor.get();
    for (float invalid : {NAN, INFINITY, 101.0f, -1.0f}) {
        DHT::humidity=invalid; fakeNow+=2000;
        assert(sensor.poll(fakeNow));
        assert(sensor.get().status==dhtsensor::Status::Unavailable);
        assert(std::isnan(sensor.get().temperature) && std::isnan(sensor.get().humidity));
    }
    DHT::humidity=50; DHT::temperature=NAN; fakeNow+=2000;
    assert(sensor.poll(fakeNow) && sensor.get().status==dhtsensor::Status::Unavailable);
    DHT::temperature=-5; DHT::humidity=0; fakeNow+=2000;
    assert(sensor.poll(fakeNow) && sensor.get().status==(CYD_DHT_MODEL==22 ? dhtsensor::Status::Ready : dhtsensor::Status::Unavailable));
    DHT::temperature=app::DHT_MIN_TEMPERATURE;DHT::humidity=app::DHT_MIN_HUMIDITY;fakeNow+=2000;
    assert(sensor.poll(fakeNow) && sensor.get().status==dhtsensor::Status::Ready);
    DHT::temperature=app::DHT_MAX_TEMPERATURE+0.1f;fakeNow+=2000;
    assert(sensor.poll(fakeNow) && sensor.get().status==dhtsensor::Status::Unavailable);
    fakeNow=UINT32_MAX-1000; sensor.begin(fakeNow);
    fakeNow=999; assert(sensor.poll(fakeNow)); // millis wrap still waits exactly 2 seconds.

    using touchgesture::Kind;
    touchgesture::Tracker gesture;
    assert(gesture.release(0).kind==Kind::None);
    gesture.track(200,100,100);gesture.track(125,103,200);
    assert(gesture.release(250).kind==Kind::SwipeLeft);
    gesture.track(100,100,300);gesture.track(180,105,400);
    assert(gesture.release(450).kind==Kind::SwipeRight);
    gesture.track(100,100,500);gesture.track(103,102,520);
    assert(gesture.release(550).kind==Kind::Tap);
    gesture.track(100,100,600);gesture.track(105,190,700);
    assert(gesture.release(750).kind==Kind::None);
    gesture.track(100,100,800);gesture.track(180,100,850);gesture.track(100,100,900);
    assert(gesture.release(950).kind==Kind::None);
    gesture.track(100,100,1000);gesture.track(200,190,1100);
    assert(gesture.release(1150).kind==Kind::None);
    gesture.track(100,100,1200);gesture.track(200,100,1300);
    assert(gesture.release(2800).kind==Kind::None);
    gesture.track(100,100,UINT32_MAX-100);gesture.track(180,100,50);
    assert(gesture.release(100).kind==Kind::SwipeRight);

    Dashboard screen; WeatherService weather;
    input(screen,weather,true,250,30);
    assert(screen.page_==Dashboard::Page::Weather); // Tab waits until release.
    input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Dht);
    screen.page_=Dashboard::Page::Rgb;
    input(screen,weather,true,250,75); // Start swiping on STOP RGB.
    input(screen,weather,true,150,77);
    input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Dht && screen.rgbEnabled_);
    input(screen,weather,true,100,140);input(screen,weather,true,200,140);input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Rgb);
    input(screen,weather,true,250,75);input(screen,weather,false);
    assert(!screen.rgbEnabled_);
    screen.page_=Dashboard::Page::Weather;
    input(screen,weather,true,285,228);input(screen,weather,true,180,225);input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Rgb && weather.requests==0);
    screen.page_=Dashboard::Page::Weather;
    input(screen,weather,true,285,228);input(screen,weather,false);
    assert(weather.requests==1);
    screen.page_=Dashboard::Page::Weather; screen.swipe(Kind::SwipeRight);
    assert(screen.page_==Dashboard::Page::Dht);
    screen.swipe(Kind::SwipeLeft);assert(screen.page_==Dashboard::Page::Weather);
    screen.page_=Dashboard::Page::Weather; screen.touchHeld_=true;
    input(screen,weather,false);assert(screen.page_==Dashboard::Page::Weather);
    screen.tap(290,10,weather);assert(screen.calibrating_);

    screen.dhtReading_=good;screen.drawDht();
    assert(contains(screen,"Temp 26.5 C") && contains(screen,"Humidity 61.0 %"));
    screen.labels.clear();screen.dhtReading_={dhtsensor::Status::Unavailable,NAN,NAN,0};
    screen.drawDht();
    assert(contains(screen,"No sensor / sensor error") && contains(screen,"Temp --.- C"));
    assert(!contains(screen,"Temp 26.5 C") && !contains(screen,"Humidity 61.0 %"));
    screen.labels.clear();screen.dhtReading_={};screen.drawDht();
    assert(contains(screen,"Waiting for first sensor reading..."));
    screen.calibrating_=false;screen.page_=Dashboard::Page::Weather;screen.layoutDirty_=true;
    fakeNow=20000;screen.poll(fakeNow,weather);assert(screen.weatherDraws==1);
    fakeNow+=1000;screen.poll(fakeNow,weather);assert(screen.weatherDraws==1);
    assert(screen.tft_.width==REFRESH_X-1 && screen.tft_.height==18); // Clock-only redraw.
    ++weather.snapshot.revision;fakeNow+=1000;screen.poll(fakeNow,weather);assert(screen.weatherDraws==2);
    weather.snapshot.conditions.valid=true;weather.snapshot.conditions.receivedAtMs=fakeNow;
    ++weather.snapshot.revision;fakeNow+=1000;screen.poll(fakeNow,weather);assert(screen.weatherDraws==3);
    fakeNow+=weather::REFRESH_INTERVAL_MS+60001;screen.poll(fakeNow,weather);
    assert(screen.weatherDraws==4 && screen.lastWeatherStale_);
    screen.page_=Dashboard::Page::Rgb;screen.dataDirty_=true;fakeNow+=1000;screen.poll(fakeNow,weather);
    assert(screen.rgbDraws==1);fakeNow+=1000;screen.poll(fakeNow,weather);assert(screen.rgbDraws==1);
    screen.dataDirty_=true;fakeNow+=20;screen.poll(fakeNow,weather);assert(screen.rgbDraws==2);
    screen.page_=Dashboard::Page::Dht;screen.dhtReading_=good;screen.dataDirty_=false;
    auto unchanged=good;unchanged.attemptedAtMs=fakeNow;
    screen.updateDht(unchanged);assert(!screen.dataDirty_ && screen.dhtReading_.attemptedAtMs==fakeNow);
    unchanged.temperature+=0.1f;screen.updateDht(unchanged);assert(screen.dataDirty_);
    screen.dataDirty_=false;screen.updateDht({dhtsensor::Status::Unavailable,NAN,NAN,fakeNow});
    assert(screen.dataDirty_);screen.dataDirty_=false;
    screen.updateDht({dhtsensor::Status::Unavailable,NAN,NAN,fakeNow+2000});assert(!screen.dataDirty_);
    screen.page_=Dashboard::Page::Rgb;screen.updateLdr(700,700,"R: N/A",TFT_GREEN);assert(screen.dataDirty_);
    screen.dataDirty_=false;screen.updateLdr(700,700,"R: N/A",TFT_GREEN);assert(!screen.dataDirty_);
    screen.updateLdr(701,701,"R: N/A",TFT_GREEN);assert(screen.dataDirty_);
    printf("PASS DHT%d: missing/fault/recovery, real-read scheduling, wrap, screen placeholders, tabs/swipes, no accidental STOP/REFRESH\n",CYD_DHT_MODEL);
}
