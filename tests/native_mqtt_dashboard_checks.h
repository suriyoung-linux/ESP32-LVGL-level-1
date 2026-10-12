void checkMqttDashboard()
{
    Dashboard screen;WeatherService weather;
    screen.page_=Dashboard::Page::Dht;
    input(screen,weather,true,290,10);input(screen,weather,false);
    screen.labels.clear();screen.drawSettings();assert(contains(screen,"MQTT") && contains(screen,"LIGHT / DARK"));
    input(screen,weather,true,170,40);input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Mqtt);
    screen.swipe(touchgesture::Kind::SwipeLeft);assert(screen.page_==Dashboard::Page::Mqtt);
    mqttdata::Status status;status.state=mqttdata::State::WaitingWifi;
    screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");
    screen.labels.clear();screen.drawHeader(weather.snapshot);assert(contains(screen,"MQTT") && contains(screen,"OFF"));
    status.state=mqttdata::State::Connecting;screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");
    screen.labels.clear();screen.drawHeader(weather.snapshot);assert(contains(screen,"WAIT"));
    status.state=mqttdata::State::Connected;status.acknowledgements=4;status.queuedBatches=1;status.lastAckAtMs=fakeNow;
    screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");
    screen.labels.clear();screen.drawLayout();screen.drawHeader(weather.snapshot);
    assert(contains(screen,"MQTT: Connected") && contains(screen,"OK"));
    assert(contains(screen,"Topics: esp32/cyd/cyd-1234abcd/") && contains(screen,"Every 10 s | QoS 1 | Retained"));
    assert(contains(screen,"Last ACK 0 s ago | ACKs 4"));
    screen.headerDirty_=false;screen.dataDirty_=false;
    screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");assert(!screen.headerDirty_ && !screen.dataDirty_);
    ++status.acknowledgements;screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");assert(screen.dataDirty_ && !screen.headerDirty_);
    status.queueFailed=true;screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");
    screen.labels.clear();screen.drawHeader(weather.snapshot);assert(contains(screen,"ERR"));
    screen.screenSleeping_=true;const auto before=pwmWrites;
    status.state=mqttdata::State::Retrying;screen.updateMqtt(status,"esp32/cyd/cyd-1234abcd");
    assert(screen.screenSleeping_ && pwmWrites==before); // MQTT events are not touch activity.
    screen.screenSleeping_=false;
    input(screen,weather,true,270,40);input(screen,weather,false);assert(screen.page_==Dashboard::Page::Settings);
    input(screen,weather,true,270,40);input(screen,weather,false);assert(screen.page_==Dashboard::Page::Dht);
    puts("PASS MQTT TFT: header state, SET details/topics/ACK counters, navigation, no automatic wake");
}
