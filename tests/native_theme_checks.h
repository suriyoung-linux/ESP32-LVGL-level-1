void checkDisplayTheme()
{
    using displaytheme::Mode;
    displaytheme::Config config;
    config.mode=Mode::Auto;
    assert(config.valid() && config.lightStart==420 && config.darkStart==1140);
    assert(!displaytheme::lightAt(config,419) && displaytheme::lightAt(config,420));
    assert(displaytheme::lightAt(config,1139) && !displaytheme::lightAt(config,1140));
    assert(!displaytheme::lightAt(config,0) && !displaytheme::lightAt(config,1439));
    config.lightStart=22*60;config.darkStart=6*60;
    assert(displaytheme::lightAt(config,22*60) && displaytheme::lightAt(config,0));
    assert(displaytheme::lightAt(config,6*60-1) && !displaytheme::lightAt(config,6*60));
    assert(!displaytheme::lightAt(config,22*60-1));
    config.mode=Mode::Light;assert(displaytheme::lightAt(config,0) && displaytheme::lightAt(config,12*60));
    config.mode=Mode::Dark;assert(!displaytheme::lightAt(config,0) && !displaytheme::lightAt(config,12*60));
    config.lightStart=config.darkStart;assert(!config.valid());
    config.lightStart=1440;assert(!config.valid());
    config=displaytheme::Config{};config.mode=static_cast<Mode>(3);assert(!config.valid());

    FakePreferences::hasCal=false;FakePreferences::savedThemeMode=0;
    FakePreferences::savedThemeTimes=(420UL<<16)|1140;
    Dashboard screen;fakeNow=1000;screen.begin();
    screen.page_=Dashboard::Page::Dht;
    WeatherService weather;
    weather.snapshot.position.latitude=18.79;weather.snapshot.position.longitude=98.98;
    weather.snapshot.position.utcOffsetSeconds=7*3600;
    auto localMinute=[&](uint16_t minute){fakeEpoch=1704067200+minute*60-7*3600+86400;};
    input(screen,weather,true,290,10);input(screen,weather,false);
    input(screen,weather,true,80,40);input(screen,weather,false);
    assert(screen.page_==Dashboard::Page::Appearance && screen.pageBeforeSettings_==Dashboard::Page::Dht);
    screen.swipe(touchgesture::Kind::SwipeLeft);assert(screen.page_==Dashboard::Page::Appearance);
    screen.labels.clear();screen.drawAppearance();
    assert(contains(screen,"LIGHT") && contains(screen,"DARK") && contains(screen,"AUTO"));
    assert(contains(screen,"07:00") && contains(screen,"19:00"));
    const auto activity=screen.lastActivityAt_;
    screen.tap(40,THEME_MODE_Y+10,weather);screen.updateTheme(weather.snapshot);
    assert(screen.theme_.mode==Mode::Light && screen.palette_.light && screen.layoutDirty_);
    screen.drawLayout();assert(screen.tft_.screenColor==TFT_WHITE);
    screen.label("Text",0,0);assert(screen.tft_.foreground==screen.palette_.foreground && screen.tft_.background==TFT_WHITE);
    assert(screen.tft_.foreground!=screen.tft_.background);
    screen.weatherLabel("Weather",100);assert(screen.tft_.foreground==screen.palette_.foreground);
    screen.label("Warning",0,0,TFT_YELLOW);assert(screen.tft_.foreground==screen.palette_.warning && screen.tft_.foreground!=TFT_YELLOW);
    screen.button(0,0,80,28,"Unselected",false);
    assert(screen.tft_.foreground==screen.palette_.foreground && screen.tft_.background==screen.palette_.card);
    screen.button(0,0,80,28,"Selected",true);assert(screen.tft_.foreground==TFT_WHITE && screen.tft_.background==screen.palette_.active);
    localMinute(0);screen.updateTheme(weather.snapshot);assert(screen.palette_.light); // Fixed mode ignores night.
    screen.tap(140,THEME_MODE_Y+10,weather);screen.updateTheme(weather.snapshot);
    localMinute(12*60);screen.updateTheme(weather.snapshot);assert(!screen.palette_.light);
    screen.drawLayout();assert(screen.tft_.screenColor==TFT_BLACK);
    screen.label("Text",0,0);assert(screen.tft_.foreground==TFT_WHITE && screen.tft_.background==TFT_BLACK);
    const auto writes=FakePreferences::themeWrites;
    screen.setThemeMode(Mode::Dark);screen.setThemeMode(static_cast<Mode>(3));assert(FakePreferences::themeWrites==writes);

    screen.tap(250,THEME_MODE_Y+10,weather);screen.updateTheme(weather.snapshot);
    assert(screen.theme_.mode==Mode::Auto && screen.palette_.light);
    localMinute(18*60+59);screen.updateTheme(weather.snapshot);assert(screen.palette_.light);
    localMinute(19*60);screen.updateTheme(weather.snapshot);assert(!screen.palette_.light);
    localMinute(6*60+59);screen.updateTheme(weather.snapshot);assert(!screen.palette_.light);
    localMinute(7*60);screen.updateTheme(weather.snapshot);assert(screen.palette_.light);
    assert(screen.lastActivityAt_==activity); // Schedule changes are not user activity.
    weather.snapshot.position.utcOffsetSeconds=0;fakeEpoch=1704067200+19*3600;
    screen.updateTheme(weather.snapshot);assert(!screen.palette_.light);
    weather.snapshot.position.utcOffsetSeconds=-5*3600;
    screen.updateTheme(weather.snapshot);assert(screen.palette_.light); // Uses the top-bar offset.
    fakeEpoch=0;screen.updateTheme(weather.snapshot);assert(!screen.themeClockReady_ && screen.palette_.light);
    screen.labels.clear();screen.drawAppearance();assert(contains(screen,"Waiting for clock; keeping current theme"));
    localMinute(12*60);weather.snapshot.position.latitude=NAN;
    screen.updateTheme(weather.snapshot);assert(!screen.themeClockReady_ && screen.palette_.light);
    weather.snapshot.position.latitude=18.79;weather.snapshot.position.utcOffsetSeconds=7*3600;
    screen.updateTheme(weather.snapshot);assert(screen.themeClockReady_ && screen.palette_.light);

    const auto start=screen.theme_.lightStart,end=screen.theme_.darkStart;
    screen.tap(280,THEME_LIGHT_Y+10,weather);assert(screen.theme_.lightStart==start+15);
    screen.tap(170,THEME_LIGHT_Y+10,weather);assert(screen.theme_.lightStart==start);
    screen.tap(280,THEME_DARK_Y+10,weather);assert(screen.theme_.darkStart==end+15);
    screen.tap(170,THEME_DARK_Y+10,weather);assert(screen.theme_.darkStart==end);
    screen.theme_.lightStart=0;screen.adjustThemeTime(true,-15);assert(screen.theme_.lightStart==1425);
    screen.adjustThemeTime(true,15);assert(screen.theme_.lightStart==0);
    screen.theme_.lightStart=1125;screen.adjustThemeTime(true,15);assert(screen.theme_.lightStart==1155 && screen.theme_.valid());
    screen.theme_.darkStart=1440-15;screen.adjustThemeTime(false,15);assert(screen.theme_.darkStart==0);
    const auto adjustmentWrites=FakePreferences::themeWrites;screen.adjustThemeTime(false,0);assert(FakePreferences::themeWrites==adjustmentWrites);
    // A saved cross-midnight schedule survives reboot and is still evaluated.
    Dashboard reboot;reboot.begin();assert(reboot.theme_.mode==Mode::Auto && reboot.theme_.lightStart==1155 && reboot.theme_.darkStart==0);
    localMinute(23*60);reboot.updateTheme(weather.snapshot);assert(reboot.palette_.light);
    localMinute(0);reboot.updateTheme(weather.snapshot);assert(!reboot.palette_.light);
    reboot.setThemeMode(Mode::Light);Dashboard fixed;fixed.begin();assert(fixed.palette_.light && fixed.theme_.mode==Mode::Light);
    FakePreferences::savedThemeMode=9;Dashboard invalidMode;invalidMode.begin();assert(invalidMode.theme_.mode==Mode::Dark && invalidMode.theme_.lightStart==420);
    FakePreferences::savedThemeMode=2;FakePreferences::savedThemeTimes=(1440UL<<16)|1140;
    Dashboard invalidTime;invalidTime.begin();assert(invalidTime.theme_.valid() && invalidTime.theme_.mode==Mode::Dark);
    FakePreferences::savedThemeTimes=(420UL<<16)|420;
    Dashboard equalTimes;equalTimes.begin();assert(equalTimes.theme_.valid() && equalTimes.theme_.darkStart==1140);

    screen.theme_=displaytheme::Config{};screen.setThemeMode(Mode::Auto);
    screen.sleepMinutes_=2;screen.lastActivityAt_=fakeNow;
    localMinute(12*60);screen.poll(fakeNow,weather);
    fakeNow+=120000;screen.poll(fakeNow,weather);assert(screen.screenSleeping_ && backlightDuty==0);
    const auto beforePwm=pwmWrites;
    localMinute(19*60);screen.poll(fakeNow,weather);
    assert(screen.screenSleeping_ && !screen.palette_.light && backlightDuty==0 && pwmWrites==beforePwm);
    input(screen,weather,true,150,100);input(screen,weather,false);
    screen.poll(fakeNow,weather);assert(!screen.screenSleeping_ && screen.tft_.screenColor==TFT_BLACK);
    input(screen,weather,true,270,40);input(screen,weather,false);assert(screen.page_==Dashboard::Page::Settings);
    input(screen,weather,true,270,40);input(screen,weather,false);assert(screen.page_==Dashboard::Page::Dht);
    // Calibration is redrawn in the new palette without losing its progress.
    screen.beginCalibration();screen.calibrationStep_=2;
    screen.setThemeMode(Mode::Light);screen.updateTheme(weather.snapshot);
    assert(screen.calibrating_ && screen.calibrationStep_==2 && screen.tft_.screenColor==TFT_WHITE);
    FakePreferences::savedThemeMode=0;FakePreferences::savedThemeTimes=(420UL<<16)|1140;
    fakeEpoch=0;
    printf("PASS theme: fixed Light/Dark, scheduled boundaries/midnight/timezone, no-clock fallback, SET controls, saved mode/times, invalid NVS, themed rendering, sleep and calibration preserved\n");
}
