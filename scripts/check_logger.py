"""Compile the real CSV logger against fault-injectable SD/SPI hardware."""

import csv
import io
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    with tempfile.TemporaryDirectory(prefix="cyd-logger-check-", dir="/tmp") as directory:
        target = Path(directory)
        (target / "Arduino.h").write_text('''#pragma once
#include <cstdint>
#include <cstring>
constexpr int VSPI=3,LOW=0,HIGH=1,INPUT=0,OUTPUT=1;
inline uint32_t fakeNow=0;
inline uint32_t millis(){return fakeNow;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
struct FakeSerial {void println(const char*){}};
inline FakeSerial Serial;
''')
        (target / "DHT.h").write_text('''#pragma once
#include <cstdint>
constexpr uint8_t DHT11=11,DHT22=22;
struct DHT {DHT(uint8_t,uint8_t){}};
''')
        (target / "SPI.h").write_text('''#pragma once
#include <Arduino.h>
#include <cassert>
struct SPIClass {
    bool active=false; int clock=-1; unsigned switches=0;
    void end(){assert(active);active=false;}
    void begin(int clk,int,int,int){assert(!active);active=true;clock=clk;++switches;}
};
''')
        (target / "weather_service.h").write_text('''#pragma once
#include "weather_config.h"
#include "weather_data.h"
struct WeatherService {
    weather::Snapshot snapshots[weather::LOCATION_COUNT];
    const weather::Snapshot &get(uint8_t i)const{return snapshots[i];}
    static const char *locationId(uint8_t i){return i==0?"chiang_mai":i==1?"bangkok":"home";}
};
''')
        (target / "SD.h").write_text('''#pragma once
#include <SPI.h>
#include <string>
#include <cassert>
constexpr int CARD_NONE=0,FILE_APPEND=1;
struct FakeSD;
struct File {
    FakeSD *sd=nullptr;
    operator bool()const;
    size_t size()const;
    size_t print(const char*);
    void flush();
    int getWriteError()const;
    void close();
};
struct FakeSD {
    bool present=false,mountable=true,mounted=false,openFail=false,shortWrite=false,flushFail=false;
    unsigned opens=0,flushes=0;SPIClass *spi=nullptr;
    std::string contents;
    void bus()const{assert(spi && spi->active && spi->clock==18);}
    bool begin(int cs,SPIClass &s,uint32_t hz,const char*,uint8_t files,bool format){
        spi=&s;bus();assert(cs==5 && hz==4000000 && files==1 && !format);
        return mounted=present && mountable;
    }
    void end(){bus();mounted=false;}
    int cardType()const{bus();return mounted?1:CARD_NONE;}
    bool readRAW(uint8_t*,uint32_t sector){bus();assert(sector==0);return mounted && present;}
    File open(const char *path,int mode){bus();assert(std::string(path)=="/weather_log.csv" && mode==FILE_APPEND);++opens;return File{this};}
};
inline FakeSD SD;
inline File::operator bool()const{return sd && sd->mounted && sd->present && !sd->openFail;}
inline size_t File::size()const{sd->bus();return sd->contents.size();}
inline size_t File::print(const char *text){sd->bus();size_t n=std::strlen(text);if(sd->shortWrite)--n;sd->contents.append(text,n);return n;}
inline void File::flush(){sd->bus();++sd->flushes;}
inline int File::getWriteError()const{return sd->flushFail?1:0;}
inline void File::close(){sd->bus();}
''')
        (target / "check.cpp").write_text(r'''#include "data_logger.h"
#include "weather_service.h"
#include <SD.h>
#include <iostream>
#include <ctime>
time_t fakeEpoch=0;
extern "C" time_t time(time_t *out) noexcept {if(out)*out=fakeEpoch;return fakeEpoch;}
void touchBus(const SPIClass &spi){assert(spi.active && spi.clock==25);}
int main(){
    SPIClass spi;spi.begin(25,39,32,33);
    DataLogger log;log.begin(spi);touchBus(spi);
    assert(log.card()==DataLogger::Card::Missing && !log.enabled() && SD.opens==0);
    assert(!log.setEnabled(true,0));touchBus(spi);
    WeatherService weather;dhtsensor::Reading dht;
    auto poll=[&](uint32_t now,bool visible,bool connected,uint32_t lag=0){fakeNow=now+lag;log.poll(now,weather,dht,visible,connected);};
    SD.present=true;
    poll(5000,true,true);touchBus(spi);
    assert(log.card()==DataLogger::Card::Ready && !log.enabled() && SD.opens==0);
    assert(log.setEnabled(true,5000));
    poll(5000,true,true);touchBus(spi);
    assert(SD.opens==1 && SD.flushes==1);
    poll(64999,false,true);assert(SD.opens==1);touchBus(spi);
    auto &c=weather.snapshots[0].conditions;
    c.valid=true;c.temperature=0;c.humidity=0;c.rain=0;c.clouds=0;c.windSpeed=0;c.windDirection=0;c.code=800;
    c.timestamp=1800000000;c.receivedAtMs=65000;
    dht={dhtsensor::Status::Ready,26.5,61.0,65100};
    fakeEpoch=1800000000;
    poll(65000,false,true,200);touchBus(spi);assert(SD.opens==2);
    std::cout<<SD.contents;
    weather.snapshots[0].weatherFailed=true;
    dht={dhtsensor::Status::Unavailable,NAN,NAN,124900};
    poll(125000,false,true);assert(SD.contents.find(",stale,0.00,")!=std::string::npos);
    assert(SD.contents.find(",unavailable,,,\n")!=std::string::npos);
    log.setEnabled(false,125000);auto previous=SD.contents;
    poll(250000,false,true);assert(SD.contents==previous);
    log.setEnabled(true,250000);poll(250000,false,true);
    assert(SD.contents.find("logged_at_utc",1)==std::string::npos); // Never duplicate CSV header.
    SD.present=false;poll(255000,false,true);touchBus(spi);
    assert(log.card()==DataLogger::Card::Missing && !log.enabled());
    SD.present=true;poll(260000,true,true);assert(!log.enabled());
    log.setEnabled(true,260000);SD.openFail=true;poll(260000,true,true);touchBus(spi);
    assert(log.card()==DataLogger::Card::Error && !log.enabled());SD.openFail=false;
    poll(265000,true,true);assert(log.card()==DataLogger::Card::Error); // Error stays visible for a decision.
    log.checkCard(270000);log.setEnabled(true,270000);SD.shortWrite=true;
    poll(270000,false,true);touchBus(spi);assert(log.card()==DataLogger::Card::Error && !log.enabled());
    SD.shortWrite=false;log.checkCard(280000);log.setEnabled(true,280000);SD.flushFail=true;
    poll(280000,false,true);touchBus(spi);assert(!log.enabled() && log.card()==DataLogger::Card::Error);
    SD.flushFail=false;log.checkCard(UINT32_MAX-10000);log.setEnabled(true,UINT32_MAX-10000);
    poll(UINT32_MAX-10000,false,true);auto opens=SD.opens;
    poll(49998,false,true);assert(SD.opens==opens);
    poll(49999,false,true);assert(SD.opens==opens+1);touchBus(spi);
    log.setEnabled(false,50000);SD.present=false;log.checkCard(50000);
    SD.present=true;SD.mountable=false;assert(!log.checkCard(55000));touchBus(spi);
    assert(log.card()==DataLogger::Card::Missing && !log.enabled());
}
''')
        for model in (11, 22):
            binary = target / f"check{model}"
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined,address", f"-DCYD_DHT_MODEL={model}",
                "-I" + str(target), "-I" + str(ROOT / "include"),
                "-I" + str(ROOT / ".pio/libdeps/esp32dev/ArduinoJson/src"),
                str(target / "check.cpp"), str(ROOT / "src/data_logger.cpp"),
                "-o", str(binary),
            ], check=True)
            output = subprocess.check_output([str(binary)], text=True)
            rows = list(csv.DictReader(io.StringIO(output)))
            assert len(rows) == 6
            assert all(len(row) == 17 and None not in row for row in rows)
            assert {row["location"] for row in rows} == {"chiang_mai", "bangkok", "home"}
            assert rows[0]["weather_status"] == "unavailable" and rows[0]["weather_temp_c"] == ""
            assert rows[0]["dht_status"] == "waiting" and rows[0]["dht_temp_c"] == ""
            assert rows[0]["logged_at_utc"] == ""
            assert rows[3]["weather_status"] == "ok" and rows[3]["weather_temp_c"] == "0.00"
            assert rows[3]["weather_observed_utc"] == "2027-01-15T08:00:00Z"
            assert rows[3]["logged_at_utc"] == "2027-01-15T08:00:00Z"
            assert rows[3]["dht_model"] == f"DHT{model}"
            assert rows[3]["dht_temp_c"] == "26.50" and rows[3]["dht_humidity_pct"] == "61.00"
            assert rows[3]["dht_age_ms"] == "100"
            assert rows[3]["uptime_ms"] == "65200"
            print(f"PASS DHT{model} CSV: opt-in, removal/reinsert, errors, interval/wrap, append, missing/zero values, SPI restored")


if __name__ == "__main__":
    main()
