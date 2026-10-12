#include <atomic>
#include <cassert>
#include <string>
#include <Preferences.h>
#include <esp_system.h>
#include "mqtt_service.h"
#include "mqtt_payload.h"

int main()
{
    Preferences::failBegin=true;MqttService noNvs;assert(!noNvs.begin());Preferences::failBegin=false;
    assert(noNvs.status(true).state==mqttdata::State::Unavailable);
    Preferences::failWrite=true;MqttService noId;assert(!noId.begin());Preferences::failWrite=false;
    failInit=true;MqttService noClient;assert(!noClient.begin());failInit=false;
    failRegister=true;MqttService noEvents;assert(!noEvents.begin() && destroys==1);failRegister=false;
    MqttService service;assert(service.begin());const unsigned allocations=allocated;
    assert(service.begin() && allocated==allocations); // Begin is idempotent.
    auto *client=&clients[allocated-1];
    assert(std::string(service.deviceId())=="cyd-1234abcd");
    assert(std::string(service.topicRoot())=="esp32/cyd/cyd-1234abcd");
    assert(std::string(client->config.uri)=="mqtt://broker.hivemq.com:1883");
    assert(!client->config.username && !client->config.password);
    assert(std::string(client->config.lwt_topic)=="esp32/cyd/cyd-1234abcd/status");
    assert(std::string(client->config.lwt_msg)=="offline" && client->config.lwt_qos==1 && client->config.lwt_retain==1);
    assert(client->config.keepalive==30 && client->config.reconnect_timeout_ms==15000 && client->config.network_timeout_ms==3000);
    dhtsensor::Reading reading;
    service.poll(100,false,reading);assert(starts==0 && client->messages.empty());
    assert(service.status(false).state==mqttdata::State::WaitingWifi);
    failStart=true;service.poll(100,true,reading);assert(starts==1);
    service.poll(15099,true,reading);assert(starts==1);
    failStart=false;service.poll(15100,true,reading);assert(starts==2);
    assert(client->messages.empty());event(client,MQTT_EVENT_BEFORE_CONNECT);
    assert(service.status(true).state==mqttdata::State::Connecting);
    event(client,MQTT_EVENT_CONNECTED);service.poll(15100,true,reading);
    assert(client->messages.size()==4 && service.status(true).queuedBatches==1);
    assert(client->messages[0].data=="online" && client->messages[1].data=="null" && client->messages[2].data=="null");
    StaticJsonDocument<256> json;
    assert(!deserializeJson(json,client->messages[3].data));
    assert(json.size()==4 && json["status"]=="waiting" && !json["valid"].as<bool>());
    assert(json["temperature_c"].isNull() && json["humidity_percent"].isNull());
    // No extra device metadata or sensor values are sent without authorization.
    assert(!json.containsKey("ldr") && !json.containsKey("device_id") && !json.containsKey("wifi_rssi_dbm"));
    service.poll(25100,true,reading);assert(client->messages.size()==4); // Outbox backpressure.
    fakeNow=25100;ackAll(client);assert(service.status(true).acknowledgements==4 && service.status(true).lastAckAtMs==fakeNow);
    reading={dhtsensor::Status::Ready,26.5f,61.0f,25100};service.poll(25100,true,reading);
    assert(client->messages.size()==8 && client->messages[5].data=="26.50" && client->messages[6].data=="61.00");
    assert(client->messages[5].topic=="esp32/cyd/cyd-1234abcd/temperature" && client->messages[6].topic=="esp32/cyd/cyd-1234abcd/humidity");
    assert(!deserializeJson(json,client->messages[7].data) && json["valid"].as<bool>() && json["status"]=="ready");
    ackAll(client);service.poll(35099,true,reading);assert(client->messages.size()==8);
    service.poll(35100,true,reading);assert(client->messages.size()==12);
    assert(client->messages[9].data=="null"); // Old sensor readings are not re-published as fresh.
    assert(!deserializeJson(json,client->messages.back().data) && json["status"]=="stale");ackAll(client);
    event(client,MQTT_EVENT_ERROR);assert(service.status(true).state==mqttdata::State::Retrying);
    service.poll(45100,true,reading);assert(client->messages.size()==12);
    event(client,MQTT_EVENT_DISCONNECTED);event(client,MQTT_EVENT_CONNECTED);
    reading={dhtsensor::Status::Ready,0,0,35101};service.poll(35101,true,reading);
    assert(client->messages.size()==16 && client->messages[13].data=="0.00" && client->messages[14].data=="0.00");ackAll(client);
    const auto before=client->messages.size();service.poll(45101,false,reading);assert(client->messages.size()==before);
    reading={dhtsensor::Status::Unavailable,NAN,NAN,45101};
    service.poll(45101,true,reading);assert(!deserializeJson(json,client->messages.back().data) && json["status"]=="unavailable");ackAll(client);
    failEnqueueAt=enqueueCalls+2;service.poll(55101,true,reading);assert(service.status(true).queueFailed);
    const auto failedCalls=enqueueCalls;service.poll(55102,true,reading);assert(enqueueCalls==failedCalls);
    ackAll(client);failEnqueueAt=0;service.poll(65101,true,reading);assert(!service.status(true).queueFailed);ackAll(client);
    event(client,MQTT_EVENT_DISCONNECTED);event(client,MQTT_EVENT_CONNECTED);
    reading={dhtsensor::Status::Ready,25,55,UINT32_MAX-1000};
    service.poll(UINT32_MAX-1000,true,reading);const auto wrapSize=client->messages.size();ackAll(client);
    service.poll(8998,true,reading);assert(client->messages.size()==wrapSize);
    reading.attemptedAtMs=8999;service.poll(8999,true,reading);assert(client->messages.size()==wrapSize+4);ackAll(client);
    MqttService reboot;assert(reboot.begin() && std::string(reboot.topicRoot())==service.topicRoot());
    Preferences::id=0;fakeRandom=0;MqttService zeroRandom;assert(zeroRandom.begin() && std::string(zeroRandom.deviceId())=="cyd-00000001");
    printf("PASS DHT%d MQTT: anonymous/LWT, persistent random topic, init/start failures, event ACKs/reconnect, bounded outbox, intervals/wrap, null/stale/zero values, payload scope\n",CYD_DHT_MODEL);
}
