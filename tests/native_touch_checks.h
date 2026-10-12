// Uses the actual dashboard functions injected by check_dht.py.
void hold(Dashboard &screen, WeatherService &weather, int x, int y, unsigned samples=12, int pressure=500)
{
    for (unsigned i=0;i<samples;++i) {
        fakeNow+=20;
        screen.touch_.down=true;
        screen.touch_.point={x,y,pressure};
        screen.pollTouch(fakeNow,weather);
    }
}

void checkTouchCalibration()
{
    using touchcal::Point;
    // Affine fit handles exchanged axes, inversion and skew, not just scaling.
    const Point fixtures[][4] = {
        {{200,300},{3800,300},{200,3700},{3800,3700}},
        {{3800,3700},{200,3700},{3800,300},{200,300}},
        {{300,200},{300,3800},{3700,200},{3700,3800}},
        {{400,400},{3600,600},{600,3500},{3800,3700}}
    };
    for (const auto &raw:fixtures) {
        touchcal::Calibration c;assert(touchcal::fit(raw,c) && c.valid());
        for (uint8_t i=0;i<4;++i) {
            const auto p=c.project(raw[i]);
            assert(std::abs(p.x-targets[i].x)<0.01f && std::abs(p.y-targets[i].y)<0.01f);
        }
    }
    const Point invalid[][4] = {
        {{200,200},{200,200},{200,200},{200,200}},
        {{200,200},{300,300},{400,400},{500,500}},
        {{NAN,200},{3800,200},{200,3700},{3800,3700}},
        {{200,200},{INFINITY,200},{200,3700},{3800,3700}},
        {{200,200},{201,200},{200,201},{201,201}}
    };
    for (const auto &raw:invalid) {
        touchcal::Calibration preserved{1,0,0,0,1,0};
        assert(!touchcal::fit(raw,preserved) && preserved.ax==1 && preserved.by==1);
    }

    // Independent checks use Euclidean error, one result per stable press/release.
    touchcheck::Session session;
    for (uint8_t i=0;i<touchcheck::POINT_COUNT;++i) {
        const auto p=session.target();
        session.sample(p);assert(!session.release() && session.count()==i); // Short tap ignored.
        for (uint8_t j=0;j<touchcheck::HOLD_SAMPLES;++j) session.sample({p.x+6,p.y+8});
        assert(session.readyToRelease() && session.count()==i);
        session.sample({NAN,NAN}); // A completed stable hold stays frozen until release.
        assert(session.release() && session.error(i)==10);
    }
    assert(session.complete() && session.passed() && session.averageError()==10 && session.maxError()==10);
    assert(!session.release());session.reset();
    for (uint8_t i=0;i<touchcheck::POINT_COUNT;++i) {
        auto p=session.target();
        for (uint8_t j=0;j<touchcheck::HOLD_SAMPLES;++j) session.sample({p.x+8,p.y+8});
        assert(session.release());
    }
    assert(session.complete() && !session.passed()); // 8 px on each axis exceeds 10 px radial error.
    session.reset();
    for (uint8_t i=0;i<24;++i) session.sample({i%2?28.0f:60.0f,72});
    assert(!session.readyToRelease() && !session.release()); // A moving finger cannot pass.
    session.sample({NAN,72});assert(!session.release());
    for (uint8_t j=0;j<touchcheck::HOLD_SAMPLES;++j) session.sample({j%2?27.0f:29.0f,72});
    assert(session.release() && session.error(0)==0); // Average stable jitter around the target.

    WeatherService weather;
    Dashboard screen;screen.preferencesReady_=true;screen.page_=Dashboard::Page::Settings;
    const auto original=screen.calibration_;
    const auto writes=FakePreferences::calWrites;
    screen.tap(200,225,weather);
    assert(screen.page_==Dashboard::Page::TouchTest && !screen.calibrating_);
    screen.labels.clear();screen.drawTouchTest();assert(contains(screen,"Touch accuracy test") && contains(screen,"RETEST"));
    // Entering touch and its release do not count as a measurement.
    hold(screen,weather,28,72);input(screen,weather,false);assert(screen.touchTest_.count()==0);
    screen.swipe(touchgesture::Kind::SwipeLeft);assert(screen.page_==Dashboard::Page::TouchTest);
    hold(screen,weather,28,72,6);input(screen,weather,false);assert(screen.touchTest_.count()==0);
    hold(screen,weather,28,72,6);hold(screen,weather,28,72,1,200);input(screen,weather,false);
    assert(screen.touchTest_.count()==0); // Weak pressure interrupts a hold.
    hold(screen,weather,28,72);assert(screen.touchTest_.readyToRelease());
    screen.layoutDirty_=false;hold(screen,weather,28,72,1,200);
    assert(!screen.touchTest_.readyToRelease() && screen.layoutDirty_);
    input(screen,weather,false);assert(screen.touchTest_.count()==0);
    for (uint8_t i=0;i<touchcheck::POINT_COUNT;++i) {
        auto p=screen.touchTest_.target();
        hold(screen,weather,p.x+3,p.y+4);
        assert(screen.touchTest_.readyToRelease() && screen.touchTest_.count()==i);
        screen.labels.clear();screen.drawTouchReadout();assert(contains(screen,"Raw X 31 Y 76 Z 500") || i>0);
        input(screen,weather,false);assert(screen.touchTest_.count()==i+1 && screen.touchTest_.error(i)==5);
    }
    screen.labels.clear();screen.drawTouchTest();
    assert(screen.touchTest_.passed() && contains(screen,"PASS: all points <= 10 px") && contains(screen,"Avg 5.0 px | Max 5.0 px"));
    assert(FakePreferences::calWrites==writes && screen.calibration_.ax==original.ax);
    screen.tap(30,225,weather);input(screen,weather,false);assert(screen.touchTest_.count()==0);
    for(uint8_t i=0;i<touchcheck::POINT_COUNT;++i) {
        const auto p=screen.touchTest_.target();hold(screen,weather,p.x+(i==2?11:0),p.y);
        input(screen,weather,false);
    }
    screen.labels.clear();screen.drawTouchTest();
    assert(!screen.touchTest_.passed() && contains(screen,"CHECK: recalibrate and retest") && contains(screen,"Avg 2.2 px | Max 11.0 px"));
    assert(FakePreferences::calWrites==writes);
    screen.sleepMinutes_=2;fakeNow+=600000;screen.poll(fakeNow,weather);
    assert(!screen.screenSleeping_ && !screen.screenDimmed_); // Diagnostics stay visible; logger still polls.
    assert(screen.logger_.polls>0);
    screen.tap(270,40,weather);assert(screen.page_==Dashboard::Page::Settings);
    screen.tap(270,40,weather);assert(screen.page_==Dashboard::Page::Weather);

    // Real 4-point calibration: moving samples and early releases cannot finish a point.
    screen.page_=Dashboard::Page::Settings;
    screen.tap(50,225,weather);assert(screen.calibrating_ && screen.waitRelease_);
    hold(screen,weather,200,300);assert(screen.calibrationStep_==0); // Must release the SET button first.
    input(screen,weather,false);
    hold(screen,weather,200,300,6);input(screen,weather,false);assert(screen.calibrationStep_==0 && screen.calibrationSamples_==0);
    hold(screen,weather,200,300,6);hold(screen,weather,200,300,1,100);assert(screen.calibrationSamples_==0);
    for(unsigned i=0;i<24;++i) hold(screen,weather,i%2?200:400,300,1);
    assert(screen.calibrationStep_==0);input(screen,weather,false);
    for (uint8_t i=0;i<4;++i) {
        hold(screen,weather,fixtures[0][i].x,fixtures[0][i].y);
        if(i<3) {
            assert(screen.calibrationStep_==i+1 && screen.waitRelease_);
            hold(screen,weather,fixtures[0][i].x,fixtures[0][i].y,20);
            assert(screen.calibrationStep_==i+1); // Held contact must not fill the next target.
        }
        input(screen,weather,false);
    }
    assert(!screen.calibrating_ && screen.page_==Dashboard::Page::TouchTest && screen.touchTest_.count()==0);
    assert(FakePreferences::calWrites==writes+1 && FakePreferences::hasCal);
    Dashboard reboot;reboot.calibration_={};reboot.begin();
    assert(reboot.calibration_.valid() && !reboot.calibrating_);
    const auto corner=reboot.calibration_.project(fixtures[0][3]);
    assert(std::abs(corner.x-300)<0.01f && std::abs(corner.y-220)<0.01f);
    const auto saved=FakePreferences::savedCal;

    // Bad fourth point is rejected; saved coefficients stay unchanged.
    screen.beginCalibration();input(screen,weather,false);
    for(uint8_t i=0;i<4;++i) {
        const auto p=i==3?fixtures[0][0]:fixtures[0][i];
        hold(screen,weather,p.x,p.y);input(screen,weather,false);
    }
    assert(screen.calibrating_ && screen.calibrationStep_==0 && FakePreferences::calWrites==writes+1);
    assert(memcmp(&saved,&FakePreferences::savedCal,sizeof(saved))==0);
    FakePreferences::savedCal={};Dashboard corrupt;corrupt.calibration_={};corrupt.begin();assert(corrupt.calibrating_);
    FakePreferences::hasCal=false;
    printf("PASS touch: affine/inverted/swapped/skewed fit, invalid fit, stable holds, radial accuracy, independent 5-point UI, retry/back, saved calibration/reboot, bad fourth point, corrupt NVS\n");
}
