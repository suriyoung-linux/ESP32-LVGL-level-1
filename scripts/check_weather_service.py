"""Check actual cache/queue scheduling and reconnect handling without network hardware."""
from pathlib import Path
import subprocess
import tempfile
from check_weather import firmware_function

ROOT = Path(__file__).resolve().parent.parent

def main():
    source = (ROOT / 'src/weather_service.cpp').read_text()
    functions = '\n'.join(firmware_function(source, 'WeatherService::' + name)
                          for name in ('begin', 'request', 'invalidateHome', 'poll'))
    template = (ROOT / 'tests/native_weather_service_template.cpp').read_text()
    with tempfile.TemporaryDirectory(prefix='cyd-cache-check-', dir='/tmp') as directory:
        target = Path(directory)
        (target / 'freertos').mkdir()
        (target / 'Arduino.h').write_text('#pragma once\n#include <cstdint>\nuint32_t millis();\n')
        (target / 'freertos/FreeRTOS.h').write_text('''#pragma once
#include <cstddef>
#include <deque>
#include <vector>
constexpr int pdPASS=1;
struct FakeQueue { size_t capacity,itemSize;std::deque<std::vector<char>> data; };
using QueueHandle_t=FakeQueue*;
QueueHandle_t xQueueCreate(size_t,size_t);
void vQueueDelete(QueueHandle_t);
int xQueueSend(QueueHandle_t,const void*,int);
int xQueueReceive(QueueHandle_t,void*,int);
int xTaskCreate(void (*)(void*),const char*,unsigned,void*,int,void*);
''')
        (target / 'freertos/queue.h').write_text('#pragma once\n#include "FreeRTOS.h"\n')
        (target / 'check.cpp').write_text(template.replace('// @SERVICE_FUNCTIONS@',functions))
        binary=target/'check'
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-I'+str(target),'-I'+str(ROOT/'include'),
                        '-I'+str(ROOT/'.pio/libdeps/esp32dev/ArduinoJson/src'),
                        str(target/'check.cpp'),'-o',str(binary)],check=True,cwd=ROOT)
        subprocess.run([str(binary)],check=True,cwd=ROOT)

if __name__=='__main__':
    main()
