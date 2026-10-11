#include <cassert>
#include <ctime>
#include <cstring>
#include <vector>
#include <ArduinoJson.h>
#include "weather_data.h"
#include <freertos/FreeRTOS.h>
#define private public
#include "weather_service.h"
#undef private

uint32_t fakeNow=100;
uint32_t millis(){return fakeNow;}
constexpr int WL_CONNECTED=3;
struct FakeWiFi { int state=WL_CONNECTED;int status(){return state;} } WiFi;
struct FakeSerial {template<class...Args>void printf(const char*,Args...){} } Serial;
int created=0,failAt=0;
bool taskFails=false;
QueueHandle_t xQueueCreate(size_t capacity,size_t bytes){
    if(++created==failAt)return nullptr;
    return new FakeQueue{capacity,bytes,{}};
}
void vQueueDelete(QueueHandle_t q){delete q;}
int xTaskCreate(void (*)(void*),const char*,unsigned stack,void*,int,void*){
    assert(stack==weather::WORKER_STACK_BYTES);return taskFails ? 0 : pdPASS;
}
int xQueueSend(QueueHandle_t q,const void *item,int){
    if(q->data.size()==q->capacity)return 0;
    q->data.emplace_back(static_cast<const char*>(item),static_cast<const char*>(item)+q->itemSize);
    return pdPASS;
}
int xQueueReceive(QueueHandle_t q,void *item,int){
    if(q->data.empty())return 0;
    memcpy(item,q->data.front().data(),q->itemSize);q->data.pop_front();return pdPASS;
}
void WeatherService::worker(void*){}
const char *WeatherService::locationId(uint8_t){return "test";}
// @SERVICE_FUNCTIONS@
int main(){
    for(int failure:{1,2,0}){
        created=0;failAt=failure;taskFails=failure==0;
        WeatherService broken;assert(!broken.begin() && !broken.ready());
        assert(!broken.jobs_ && !broken.results_);
    }
    taskFails=false;failAt=0;WeatherService service;assert(service.begin());
    assert(service.workerStackMinFreeBytes()==0);
    WiFi.state=0;assert(!service.request(0));WiFi.state=WL_CONNECTED;
    assert(!service.request(weather::LOCATION_COUNT));
    assert(service.request(0) && service.get(0).revision==1 && service.get(0).pending);
    assert(!service.request(0,true)); // No duplicate job while pending.
    WeatherService::Job job;assert(xQueueReceive(service.jobs_,&job,0)==pdPASS && job.location==0);
    WeatherService::Result result{};result.location=0;result.stackFreeBytes=4000;
    result.snapshot.position=job.position;
    result.snapshot.conditions.valid=true;result.snapshot.conditions.temperature=27;
    result.snapshot.air.valid=true;result.snapshot.air.pm25=8;
    assert(xQueueSend(service.results_,&result,0)==pdPASS);service.poll();
    assert(!service.get(0).pending && service.get(0).revision==2);
    assert(service.get(0).conditions.temperature==27 && service.workerStackMinFreeBytes()==4000);
    assert(!service.request(0));fakeNow+=weather::MIN_REQUEST_INTERVAL_MS;
    assert(service.request(0,true));assert(xQueueReceive(service.jobs_,&job,0)==pdPASS);
    result.snapshot.conditions={};result.snapshot.air={};result.snapshot.weatherFailed=true;
    result.snapshot.airFailed=true;strcpy(result.snapshot.error,"HTTP 401");
    assert(xQueueSend(service.results_,&result,0)==pdPASS);service.poll();
    assert(service.get(0).conditions.valid && service.get(0).conditions.temperature==27);
    assert(service.get(0).air.valid && service.get(0).weatherFailed && service.get(0).airFailed);
    assert(service.get(0).revision==4 && !service.get(0).pending); // Cache is preserved on errors.
    assert(service.request(2));assert(xQueueReceive(service.jobs_,&job,0)==pdPASS && job.resolvePosition);
    const uint32_t previous=service.get(2).revision;service.invalidateHome();
    assert(service.get(2).revision==previous+1 && !service.get(2).pending);
    result={};result.location=2;result.generation=job.generation;
    result.snapshot.conditions.valid=true;
    assert(xQueueSend(service.results_,&result,0)==pdPASS);service.poll();
    assert(!service.get(2).conditions.valid && service.get(2).revision==previous+1);
    // Queue saturation must not mark a cache pending or attempted when no job was accepted.
    for(int i=0;i<weather::LOCATION_COUNT;++i)assert(xQueueSend(service.jobs_,&job,0)==pdPASS);
    service.cache_[1]={};assert(!service.request(1));assert(!service.get(1).pending && !service.get(1).attempted);
    vQueueDelete(service.jobs_);vQueueDelete(service.results_);
    puts("PASS: queue allocation/task failures, request throttling, cache/error revisions, reconnect generation, telemetry and queue saturation");
}
