"""Exercise the actual DHT service and dashboard handlers with fake hardware."""

from pathlib import Path
import subprocess
import tempfile

from check_weather import firmware_function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / "src/dashboard.cpp").read_text()
    constants = source[source.index("constexpr uint16_t BACKGROUND"):source.index("void localTimeText")]
    functions = "\n".join(firmware_function(source, "Dashboard::" + name)
                          for name in ("pollTouch", "tap", "swipe", "drawDht", "drawWeatherClock", "drawDhtAge", "updateDht", "updateLdr", "poll"))
    functions = firmware_function(source, "localTimeText") + "\n" + functions
    template = (ROOT / "tests/native_dht_template.cpp").read_text()
    with tempfile.TemporaryDirectory(prefix="cyd-dht-check-", dir="/tmp") as directory:
        target = Path(directory)
        (target / "Arduino.h").write_text(
            "#pragma once\n#include <cstdint>\n"
            "constexpr int VSPI=3, LOW=0, HIGH=1, INPUT=0, OUTPUT=1;\n"
            "inline void digitalWrite(int,int){}\ninline void pinMode(int,int){}\n"
            "uint32_t millis();\n"
            "struct FakeSerial { template<class...T> void printf(const char*,T...){} "
            "void println(const char*){} };\nextern FakeSerial Serial;\n"
        )
        (target / "DHT.h").write_text(
            "#pragma once\n#include <Arduino.h>\n#include <cassert>\n#include <cmath>\n"
            "constexpr uint8_t DHT11=11,DHT22=22;\n"
            "struct DHT { inline static unsigned reads=0; "
            "inline static float temperature=NAN,humidity=NAN; "
            "DHT(uint8_t pin,uint8_t model){assert(pin==27 && model==CYD_DHT_MODEL);} "
            "void begin(){} "
            "float readHumidity(bool force=false){assert(force);++reads;return humidity;} "
            "float readTemperature(bool fahrenheit=false,bool force=false){"
            "assert(!fahrenheit && !force);return temperature;} };\n"
        )
        (target / "check.cpp").write_text(template.replace("// @CONSTANTS@", constants)
                                          .replace("// @DASHBOARD_FUNCTIONS@", functions))
        for model in (11, 22):
            binary = target / ("check" + str(model))
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined,address", "-DCYD_DHT_MODEL=" + str(model),
                "-I" + str(target), "-I" + str(ROOT / "include"),
                "-I" + str(ROOT / ".pio/libdeps/esp32dev/ArduinoJson/src"),
                str(target / "check.cpp"), str(ROOT / "src/dht_sensor.cpp"),
                "-o", str(binary),
            ], check=True, cwd=ROOT)
            subprocess.run([str(binary)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
