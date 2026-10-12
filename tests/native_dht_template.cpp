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
#include "touch_accuracy.h"
#include "display_theme.h"
#include "mqtt_state.h"
#include "board_config.h"

uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }
time_t fakeEpoch=0;
extern "C" time_t time(time_t *out) noexcept {if(out)*out=fakeEpoch;return fakeEpoch;}
FakeSerial Serial;
int backlightLevel=HIGH;
unsigned backlightWrites=0;
uint32_t backlightDuty=255;
unsigned pwmWrites=0;
uint32_t ledcSetup(uint8_t channel,uint32_t hz,uint8_t bits){assert(channel==0 && hz==5000 && bits==8);return hz;}
void ledcAttachPin(uint8_t pin,uint8_t channel){assert(pin==cyd::TFT_PIN_BACKLIGHT && channel==0);}
void ledcWrite(uint8_t channel,uint32_t duty){assert(channel==0 && duty<=255);backlightDuty=duty;backlightLevel=duty>0?HIGH:LOW;++pwmWrites;}
void digitalWrite(int pin,int level) { assert(pin==cyd::TFT_PIN_BACKLIGHT); backlightLevel=level; ++backlightWrites; }
struct FakeWifi { int32_t RSSI(){return -65;} } WiFi;
constexpr uint16_t TFT_BLACK=0, TFT_RED=0xF800, TFT_WHITE=0xFFFF, TFT_LIGHTGREY=0xD69A,
                   TFT_YELLOW=0xFFE0, TFT_GREEN=0x07E0, TFT_CYAN=0x07FF;
constexpr uint8_t ML_DATUM=1, MC_DATUM=2;
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
    template<class T> void begin(T&) {}
    void setRotation(int) {}
    bool touched() { return down; }
    TouchPoint getPoint() { return point; }
};
struct FakePreferences {
    inline static uint16_t savedSleep=0;
    inline static unsigned sleepWrites=0;
    inline static touchcal::Calibration savedCal{};
    inline static bool hasCal=false;
    inline static unsigned calWrites=0;
    inline static uint16_t savedThemeMode=0;
    inline static uint32_t savedThemeTimes=(7UL*60<<16)|(19UL*60);
    inline static unsigned themeWrites=0;
    bool begin(const char*,bool){return true;}
    bool getBool(const char*,bool fallback){return fallback;}
    uint16_t getUShort(const char *key,uint16_t){return strcmp(key,"theme-mode")==0?savedThemeMode:savedSleep;}
    uint32_t getUInt(const char *key,uint32_t){assert(strcmp(key,"theme-times")==0);return savedThemeTimes;}
    void putUShort(const char* key,uint16_t value){
        if(strcmp(key,"theme-mode")==0){savedThemeMode=value;++themeWrites;}
        else {assert(strcmp(key,"sleep-min")==0);savedSleep=value;++sleepWrites;}
    }
    void putUInt(const char *key,uint32_t value){assert(strcmp(key,"theme-times")==0);savedThemeTimes=value;++themeWrites;}
    size_t getBytesLength(const char*){return hasCal?sizeof(savedCal):0;}
    void getBytes(const char*,void *out,size_t size){assert(size==sizeof(savedCal));memcpy(out,&savedCal,size);}
    void putBytes(const char *key,const void *value,size_t size){assert(strcmp(key,"cal")==0 && size==sizeof(savedCal));memcpy(&savedCal,value,size);hasCal=true;++calWrites;}
};
struct FakeSpi { template<class...T> void begin(T...){} };
struct DataLogger {
    enum class Card { Missing, Ready, Error };
    Card state=Card::Missing;
    bool recording=false;
    uint32_t changes=0;
    unsigned polls=0,checks=0;
    template<class T> void begin(T&){}
    Card card()const{return state;}
    bool enabled()const{return recording;}
    uint32_t revision()const{return changes;}
    bool checkCard(uint32_t){++checks;return state==Card::Ready;}
    bool setEnabled(bool enable,uint32_t){if(enable && state!=Card::Ready)return false;recording=enable;++changes;return true;}
    void poll(uint32_t,const WeatherService&,const dhtsensor::Reading&,bool,bool){++polls;}
};
struct FakeTft {
    int width=0,height=0;unsigned headerFills=0;
    uint16_t screenColor=0,foreground=0,background=0;unsigned fills=0;
    std::vector<std::string> labels;
    void init(){} void setRotation(int){} void fillScreen(uint16_t color){screenColor=color;++fills;}
    void fillRect(int x,int y,int w,int h,int){width=w;height=h;if(x==0 && y==0 && w==320 && h==HEADER_HEIGHT)++headerFills;}
    void drawCircle(int,int,int,int){}
    void drawFastHLine(int,int,int,int){}
    void drawFastVLine(int,int,int,int){}
    void setTextDatum(int){} void setTextSize(int){} void setTextPadding(int){}
    void setTextColor(uint16_t fg,uint16_t bg){foreground=fg;background=bg;}
    void drawString(const char *text,int,int,int){labels.emplace_back(text);}
    int textWidth(const char *text,int){return strlen(text)*8;}
    void fillRoundRect(int,int,int,int,int,int){}
};
struct Dashboard {
    enum class Page { Weather, Rgb, Dht, Settings, TouchTest, Appearance, Mqtt };
    Page page_=Page::Weather;
    FakeTouch touch_;
    FakeSpi touchSpi_;
    DataLogger logger_;
    uint32_t lastLogRevision_=UINT32_MAX;
    Page pageBeforeSettings_=Page::Weather;
    uint16_t sleepMinutes_=0;
    uint32_t lastActivityAt_=0;
    bool screenSleeping_=false,screenDimmed_=false,headerDirty_=true;
    uint8_t backlightDuty_=0;
    FakePreferences preferences_;
    FakeTft tft_;
    touchcal::Calibration calibration_{1,0,0,0,1,0};
    touchcal::Point calibrationPoints_[4];
    touchcal::Point calibrationMin_{0,0},calibrationMax_{0,0};
    touchcheck::Session touchTest_;
    touchcal::Point testRaw_{0,0},testProjected_{0,0};
    int16_t testPressure_=0;
    bool testHasReading_=false;
    displaytheme::Config theme_;
    displaytheme::Palette palette_;
    bool themeClockReady_=false;
    mqttdata::Status mqttStatus_;
    char mqttTopicRoot_[48]="";
    touchgesture::Tracker gesture_;
    dhtsensor::Reading dhtReading_;
    bool preferencesReady_=false, calibrating_=false, waitRelease_=false, touchHeld_=false;
    bool rgbEnabled_=true, layoutDirty_=false, dataDirty_=false;
    uint8_t location_=0, group_=0, calibrationStep_=0, calibrationSamples_=0;
    uint32_t lastTouchAt_=0, lastClockAt_=0, lastWeatherRevision_=UINT32_MAX;
    bool connected_=true,lastWeatherStale_=false;
    unsigned weatherDraws=0,rgbDraws=0;
    uint16_t raw_=0,ledColor_=0;uint32_t millivolts_=0;char resistance_[24]="";
    float calibrationSumX_=0, calibrationSumY_=0;
    std::vector<std::string> &labels=tft_.labels;
    void beginCalibration();
    void drawCalibrationTarget();
    void beginTouchTest();
    void drawTouchTest();
    void drawTouchReadout();
    void setRgbEnabled(bool value) { rgbEnabled_=value; }
    void label(const char*,int16_t,int16_t,uint16_t=TFT_WHITE,uint8_t=2);
    void weatherLabel(const char*,int16_t,uint16_t=TFT_WHITE);
    void button(int16_t,int16_t,int16_t,int16_t,const char*,bool);
    void begin();
    void drawSettings();
    void drawAppearance();
    void drawMqtt();
    void updateMqtt(const mqttdata::Status&,const char*);
    void setThemeMode(displaytheme::Mode);
    void adjustThemeTime(bool,int16_t);
    void updateTheme(const weather::Snapshot&);
    void setSleepMinutes(uint16_t,uint32_t);
    void setBacklight(uint8_t);
    void pollTouch(uint32_t, WeatherService&);
    void tap(int16_t,int16_t,WeatherService&);
    void swipe(touchgesture::Kind);
    void drawDht();
    void drawDhtAge();
    void updateDht(const dhtsensor::Reading&);
    void updateLdr(uint16_t,uint32_t,const char*,uint16_t);
    void drawLayout();
    void drawWeather(const weather::Snapshot&,bool){++weatherDraws;}
    void drawHeader(const weather::Snapshot&);
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
#include "native_touch_checks.h"
#include "native_theme_checks.h"
#include "native_mqtt_dashboard_checks.h"
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
    screen.tap(290,10,weather);assert(screen.page_==Dashboard::Page::Settings && !screen.calibrating_);
    screen.swipe(Kind::SwipeLeft);assert(screen.page_==Dashboard::Page::Settings);
    screen.tap(250,30,weather);assert(screen.page_==Dashboard::Page::Weather); // BACK returns to the former page.
    screen.tap(290,10,weather);screen.tap(50,225,weather);assert(screen.calibrating_);

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
    assert(screen.tft_.headerFills==2); // Clock redraw stays in the top bar.
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
    // Every duration uses inactivity; rendering and sensor updates do not extend it.
    for (uint16_t minutes : {2,5,10}) {
        Dashboard sleeper;fakeNow=1000;sleeper.begin();
        sleeper.page_=Dashboard::Page::Rgb;
        sleeper.setSleepMinutes(minutes,fakeNow);
        const uint32_t deadline=fakeNow+minutes*60000UL;
        fakeNow=deadline-15001;sleeper.poll(fakeNow,weather);
        assert(!sleeper.screenSleeping_ && !sleeper.screenDimmed_ && backlightDuty==255);
        fakeNow=deadline-15000;sleeper.poll(fakeNow,weather);
        assert(!sleeper.screenSleeping_ && sleeper.screenDimmed_ && backlightDuty==64);
        const auto dimWrites=pwmWrites;
        fakeNow=deadline-1;sleeper.poll(fakeNow,weather);
        assert(!sleeper.screenSleeping_ && sleeper.screenDimmed_ && backlightDuty==64 && pwmWrites==dimWrites);
        fakeNow=deadline;sleeper.poll(fakeNow,weather);
        assert(sleeper.screenSleeping_ && !sleeper.screenDimmed_ && backlightDuty==0);
        const auto draws=sleeper.rgbDraws;
        sleeper.updateLdr(900,900,"R: N/A",TFT_GREEN);
        fakeNow+=1000;sleeper.poll(fakeNow,weather);assert(sleeper.rgbDraws==draws && sleeper.raw_==900);
        input(sleeper,weather,true,250,75);assert(!sleeper.screenSleeping_ && backlightLevel==HIGH);
        input(sleeper,weather,true,150,77);input(sleeper,weather,false);
        assert(sleeper.rgbEnabled_ && sleeper.page_==Dashboard::Page::Rgb); // Wake press cannot STOP or swipe.
        input(sleeper,weather,true,250,75);input(sleeper,weather,false);assert(!sleeper.rgbEnabled_);
        sleeper.poll(fakeNow,weather);assert(sleeper.rgbDraws==draws+1);
    }
    Dashboard settings;settings.begin();settings.page_=Dashboard::Page::Dht;
    input(settings,weather,true,290,10);input(settings,weather,false);
    assert(settings.page_==Dashboard::Page::Settings);
    settings.labels.clear();settings.drawSettings();
    assert(contains(settings,"2 minutes") && contains(settings,"5 minutes") && contains(settings,"10 minutes") && contains(settings,"Never sleep"));
    assert(!contains(settings,"Temp 26.5 C"));
    for (uint8_t i=0;i<4;++i) {
        input(settings,weather,true,SLEEP_OPTION_X+(i%2)*OPTION_COLUMN_STEP+30,SLEEP_OPTION_Y+(i/2)*SLEEP_OPTION_STEP+10);input(settings,weather,false);
        assert(settings.sleepMinutes_==SLEEP_MINUTES[i] && FakePreferences::savedSleep==SLEEP_MINUTES[i]);
    }
    settings.labels.clear();settings.drawSettings();assert(contains(settings,"microSD: Not found / cannot mount") && contains(settings,"CHECK SD"));
    settings.tap(60,LOG_OPTION_Y+10,weather);assert(settings.logger_.checks==1 && !settings.logger_.enabled());
    settings.logger_.state=DataLogger::Card::Ready;
    settings.labels.clear();settings.drawSettings();assert(contains(settings,"microSD: Found") && contains(settings,"LOG ON"));
    settings.tap(60,LOG_OPTION_Y+10,weather);assert(settings.logger_.enabled());
    settings.tap(200,LOG_OPTION_Y+10,weather);assert(!settings.logger_.enabled());
    settings.logger_.state=DataLogger::Card::Error;settings.labels.clear();settings.drawSettings();assert(contains(settings,"microSD: Read/write error"));
    settings.screenSleeping_=true;const auto logPolls=settings.logger_.polls;
    settings.poll(fakeNow,weather);assert(settings.logger_.polls==logPolls+1);
    settings.screenSleeping_=false;
    input(settings,weather,true,270,40);input(settings,weather,false);assert(settings.page_==Dashboard::Page::Dht);
    settings.setSleepMinutes(5,fakeNow);
    const auto writes=FakePreferences::sleepWrites;
    settings.setSleepMinutes(5,fakeNow);settings.setSleepMinutes(3,fakeNow);
    assert(FakePreferences::sleepWrites==writes && settings.sleepMinutes_==5);
    Dashboard restarted;restarted.begin();assert(restarted.sleepMinutes_==5);
    FakePreferences::savedSleep=17;Dashboard invalid;invalid.begin();assert(invalid.sleepMinutes_==0);
    invalid.setSleepMinutes(0,fakeNow);fakeNow+=3600000;invalid.poll(fakeNow,weather);assert(!invalid.screenSleeping_);
    fakeNow=UINT32_MAX-60000;invalid.setSleepMinutes(2,fakeNow);
    fakeNow=59998;invalid.poll(fakeNow,weather);assert(!invalid.screenSleeping_);
    fakeNow=59999;invalid.poll(fakeNow,weather);assert(invalid.screenSleeping_); // Timeout survives millis wrap.
    input(invalid,weather,true,290,10);input(invalid,weather,false);assert(invalid.page_==Dashboard::Page::Weather);
    input(invalid,weather,true,290,10);input(invalid,weather,false);assert(invalid.page_==Dashboard::Page::Settings);
    invalid.labels.clear();invalid.connected_=false;invalid.drawHeader(weather.snapshot);
    assert(contains(invalid,"Serial ready") && !contains(invalid,"WiFi connected"));
    invalid.labels.clear();invalid.connected_=true;invalid.drawHeader(weather.snapshot);
    assert(contains(invalid,"WiFi connected"));
    Dashboard fast;fakeNow=0;fast.begin();fast.page_=Dashboard::Page::Rgb;fast.poll(fakeNow,weather);
    const auto initialHeaders=fast.tft_.headerFills;
    for (uint16_t raw=1;raw<=6;++raw) {
        fakeNow+=500;fast.updateLdr(raw,raw,"R: N/A",TFT_GREEN);fast.poll(fakeNow,weather);
    }
    assert(fast.tft_.headerFills==initialHeaders+3); // Frequent LDR updates cannot starve the top clock.
    Dashboard active;fakeNow=0;active.begin();active.setSleepMinutes(2,fakeNow);
    fakeNow=100000;input(active,weather,true,150,140);input(active,weather,false);
    fakeNow=120000;active.poll(fakeNow,weather);assert(!active.screenSleeping_); // Touch resets timeout.
    active.calibrating_=true;fakeNow+=600000;active.poll(fakeNow,weather);assert(!active.screenSleeping_);
    // A warning can be cancelled by touch, including a drag over an action button.
    Dashboard dimmed;fakeNow=0;dimmed.begin();dimmed.page_=Dashboard::Page::Rgb;
    dimmed.setSleepMinutes(2,fakeNow);fakeNow=105000;dimmed.poll(fakeNow,weather);
    assert(dimmed.screenDimmed_ && backlightDuty==64);
    input(dimmed,weather,true,250,75);
    assert(!dimmed.screenDimmed_ && !dimmed.screenSleeping_ && backlightDuty==255);
    input(dimmed,weather,true,150,77);input(dimmed,weather,false);
    assert(dimmed.rgbEnabled_ && dimmed.page_==Dashboard::Page::Rgb);
    const uint32_t touchedAt=dimmed.lastActivityAt_;
    fakeNow=120000;dimmed.poll(fakeNow,weather);assert(!dimmed.screenSleeping_ && !dimmed.screenDimmed_);
    fakeNow=touchedAt+105000;dimmed.poll(fakeNow,weather);assert(dimmed.screenDimmed_);
    dimmed.setSleepMinutes(0,fakeNow);assert(!dimmed.screenDimmed_ && backlightDuty==255);
    fakeNow+=3600000;dimmed.poll(fakeNow,weather);assert(!dimmed.screenDimmed_ && !dimmed.screenSleeping_ && backlightDuty==255);
    dimmed.setSleepMinutes(2,fakeNow);fakeNow+=105000;dimmed.poll(fakeNow,weather);assert(dimmed.screenDimmed_);
    dimmed.beginCalibration();assert(!dimmed.screenDimmed_ && backlightDuty==255);
    fakeNow+=600000;dimmed.poll(fakeNow,weather);assert(!dimmed.screenSleeping_ && !dimmed.screenDimmed_);
    Dashboard rollover;fakeNow=UINT32_MAX-60000;rollover.begin();rollover.setSleepMinutes(2,fakeNow);
    fakeNow=44998;rollover.poll(fakeNow,weather);assert(!rollover.screenDimmed_);
    fakeNow=44999;rollover.poll(fakeNow,weather);assert(rollover.screenDimmed_ && backlightDuty==64);
    fakeNow=59999;rollover.poll(fakeNow,weather);assert(rollover.screenSleeping_ && backlightDuty==0);
    checkTouchCalibration();
    checkDisplayTheme();
    checkMqttDashboard();
    printf("PASS DHT%d: missing/fault/recovery, real-read scheduling, wrap, screen placeholders, tabs/swipes, no accidental STOP/REFRESH, screen dim-warning/touch-cancel/sleep/wake/settings/persistence/wrap, top header, touch calibration/accuracy\n",CYD_DHT_MODEL);
}
