"""Exercise the actual MQTT service with a fault-injectable ESP-MQTT backend."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    with tempfile.TemporaryDirectory(prefix="cyd-mqtt-check-", dir="/tmp") as directory:
        target = Path(directory)
        (target / "Arduino.h").write_text('''#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
inline uint32_t fakeNow=0;
inline uint32_t millis(){return fakeNow;}
struct FakeSerial {template<class...T>void printf(const char*,T...){}void println(const char*){}};
inline FakeSerial Serial;
''')
        (target / "Preferences.h").write_text('''#pragma once
#include <Arduino.h>
#include <cassert>
struct Preferences {
    inline static uint32_t id=0;
    inline static bool failBegin=false,failWrite=false;
    bool begin(const char *name,bool readOnly){assert(!readOnly && strcmp(name,"mqtt-device")==0);return !failBegin;}
    uint32_t getUInt(const char *key,uint32_t){assert(strcmp(key,"id")==0);return id;}
    size_t putUInt(const char *key,uint32_t value){assert(strcmp(key,"id")==0);if(failWrite)return 0;id=value;return sizeof(value);}
    void end(){}
};
''')
        (target / "esp_system.h").write_text('''#pragma once
#include <cstdint>
inline uint32_t fakeRandom=0x1234abcd;
inline uint32_t esp_random(){return fakeRandom;}
''')
        (target / "mqtt_client.h").write_text('''#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
constexpr int ESP_OK=0,MQTT_EVENT_ANY=-1,MQTT_EVENT_CONNECTED=1,MQTT_EVENT_BEFORE_CONNECT=2,
              MQTT_EVENT_ERROR=3,MQTT_EVENT_DISCONNECTED=4,MQTT_EVENT_PUBLISHED=5;
using esp_event_base_t=const char*;
using Handler=void(*)(void*,esp_event_base_t,int32_t,void*);
struct esp_mqtt_client_config_t {
    const char *uri=nullptr,*client_id=nullptr,*username=nullptr,*password=nullptr,*lwt_topic=nullptr,*lwt_msg=nullptr;
    int lwt_qos=0,lwt_retain=0,keepalive=0,reconnect_timeout_ms=0,network_timeout_ms=0,task_stack=0,buffer_size=0;
};
struct Message {std::string topic,data;};
struct FakeClient {esp_mqtt_client_config_t config;Handler handler=nullptr;void *context=nullptr;unsigned outbox=0;std::vector<Message> messages;};
using esp_mqtt_client_handle_t=FakeClient*;
inline FakeClient clients[16];
inline unsigned allocated=0,starts=0,destroys=0,enqueueCalls=0,failEnqueueAt=0;
inline bool failInit=false,failRegister=false,failStart=false;
inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *cfg){
    if(failInit){return nullptr;}assert(allocated<16);auto *c=&clients[allocated++];c->config=*cfg;return c;
}
inline int esp_mqtt_client_register_event(FakeClient *c,int event,Handler handler,void *context){
    assert(event==MQTT_EVENT_ANY);if(failRegister)return -1;c->handler=handler;c->context=context;return ESP_OK;
}
inline int esp_mqtt_client_destroy(FakeClient*){++destroys;return ESP_OK;}
inline int esp_mqtt_client_start(FakeClient*){++starts;return failStart?-1:ESP_OK;}
inline int esp_mqtt_client_get_outbox_size(FakeClient *c){return c->outbox;}
inline int esp_mqtt_client_enqueue(FakeClient *c,const char *topic,const char *data,int len,int qos,int retain,bool store){
    assert(len==0 && qos==1 && retain==1 && store);if(++enqueueCalls==failEnqueueAt)return -1;
    c->messages.push_back({topic,data});++c->outbox;return enqueueCalls;
}
inline void event(FakeClient *c,int id){assert(c->handler);c->handler(c->context,"MQTT",id,nullptr);}
inline void ackAll(FakeClient *c){while(c->outbox){--c->outbox;event(c,MQTT_EVENT_PUBLISHED);}}
''')
        (target / "check.cpp").write_text((ROOT / "tests/native_mqtt_template.cpp").read_text())
        for model in (11, 22):
            binary = target / f"check{model}"
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined,address", f"-DCYD_DHT_MODEL={model}",
                "-I" + str(target), "-I" + str(ROOT / "include"),
                "-I" + str(ROOT / ".pio/libdeps/esp32dev/ArduinoJson/src"),
                str(target / "check.cpp"), str(ROOT / "src/mqtt_service.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
