"""Run actual firmware parsers, RGB logic and API handlers against host-side fakes."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def firmware_function(source, name):
    start = source.index("void " + name + "(")
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    source = (ROOT / "src/main.cpp").read_text()
    functions = firmware_function(source, "formatResistance") + "\n"
    functions += firmware_function(source, "updateLightLed") + "\n"
    functions += source[source.index("void sendJson("):source.index("}  // namespace")]
    template = (ROOT / "tests/native_weather_template.cpp").read_text()
    with tempfile.TemporaryDirectory(prefix="cyd-weather-check-", dir="/tmp") as directory:
        target = Path(directory)
        (target / "Arduino.h").write_text(
            "#pragma once\n#include <cstdint>\n"
            "constexpr int VSPI=3, LOW=0, HIGH=1, INPUT=0, OUTPUT=1;\n"
            "void digitalWrite(int,int);\nvoid pinMode(int,int);\n"
        )
        (target / "check.cpp").write_text(template.replace("// @FIRMWARE_FUNCTIONS@", functions))
        binary = target / "check"
        subprocess.run([
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined,address", "-I" + str(target),
            "-I" + str(ROOT / "include"),
            "-I" + str(ROOT / ".pio/libdeps/esp32dev/ArduinoJson/src"),
            str(target / "check.cpp"), "-o", str(binary),
        ], check=True, cwd=ROOT)
        subprocess.run([str(binary)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
