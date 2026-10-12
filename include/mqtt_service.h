#pragma once

#include <Arduino.h>
#include <atomic>
#include <mqtt_client.h>
#include "mqtt_config.h"
#include "mqtt_state.h"
#include "dht_reading.h"

class MqttService {
public:
    bool begin();
    void poll(uint32_t now, bool wifiConnected, const dhtsensor::Reading &dht);
    mqttdata::Status status(bool wifiConnected) const;
    const char *topicRoot() const { return root_; }
    const char *deviceId() const { return deviceId_; }

private:
    esp_mqtt_client_handle_t client_ = nullptr;
    char deviceId_[13] = "", root_[48] = "", statusTopic_[64] = "";
    bool started_ = false, attempted_ = false, queueFailed_ = false;
    uint32_t lastAttemptAt_ = 0, seenGeneration_ = 0, queuedBatches_ = 0;
    std::atomic<mqttdata::State> state_{mqttdata::State::Connecting};
    std::atomic<uint32_t> generation_{0}, acknowledgements_{0}, lastAckAtMs_{0};
    static void onEvent(void *context, esp_event_base_t base, int32_t eventId, void *event);
    bool enqueue(const char *suffix, const char *payload);
};
