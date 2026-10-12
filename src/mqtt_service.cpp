#include "mqtt_service.h"

#include <Preferences.h>
#include <esp_system.h>
#include "mqtt_payload.h"

bool MqttService::begin()
{
    if (client_) return true;
    Preferences preferences;
    if (!preferences.begin("mqtt-device", false)) return false;
    uint32_t id = preferences.getUInt("id", 0);
    if (!id) {
        id = esp_random();
        if (!id) id = 1;
        if (preferences.putUInt("id", id) != sizeof(id)) { preferences.end(); return false; }
    }
    preferences.end();
    snprintf(deviceId_, sizeof(deviceId_), "cyd-%08lx", static_cast<unsigned long>(id));
    snprintf(root_, sizeof(root_), "esp32/cyd/%s", deviceId_);
    snprintf(statusTopic_, sizeof(statusTopic_), "%s/status", root_);
    esp_mqtt_client_config_t config{};
    config.uri = mqttconfig::URI;
    config.client_id = deviceId_;
    config.username = nullptr;
    config.password = nullptr;
    config.lwt_topic = statusTopic_;
    config.lwt_msg = "offline";
    config.lwt_qos = 1;
    config.lwt_retain = 1;
    config.keepalive = mqttconfig::KEEPALIVE_SECONDS;
    config.reconnect_timeout_ms = mqttconfig::RECONNECT_INTERVAL_MS;
    config.network_timeout_ms = mqttconfig::NETWORK_TIMEOUT_MS;
    config.task_stack = mqttconfig::TASK_STACK_BYTES;
    config.buffer_size = mqttconfig::PAYLOAD_BYTES + 128;
    client_ = esp_mqtt_client_init(&config);
    if (!client_) return false;
    if (esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, onEvent, this) != ESP_OK) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return false;
    }
    Serial.printf("MQTT: %s (anonymous), device %s\n", mqttconfig::URI, deviceId_);
    Serial.printf("MQTT topics: %s/temperature | %s/humidity | %s/telemetry | %s/status\n", root_, root_, root_, root_);
    return true;
}

void MqttService::onEvent(void *context, esp_event_base_t, int32_t eventId, void *)
{
    auto &self = *static_cast<MqttService *>(context);
    switch (eventId) {
    case MQTT_EVENT_CONNECTED:
        self.generation_.fetch_add(1);
        self.state_.store(mqttdata::State::Connected);
        Serial.println("MQTT: HiveMQ connected");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        self.state_.store(mqttdata::State::Connecting);
        break;
    case MQTT_EVENT_ERROR:
    case MQTT_EVENT_DISCONNECTED:
        self.state_.store(mqttdata::State::Retrying);
        break;
    case MQTT_EVENT_PUBLISHED:
        self.lastAckAtMs_.store(millis());
        self.acknowledgements_.fetch_add(1);
        break;
    default: break;
    }
}

mqttdata::Status MqttService::status(bool wifiConnected) const
{
    mqttdata::Status result;
    result.state = !client_ ? mqttdata::State::Unavailable : !wifiConnected ? mqttdata::State::WaitingWifi : state_.load();
    result.acknowledgements = acknowledgements_.load();
    result.lastAckAtMs = lastAckAtMs_.load();
    result.queuedBatches = queuedBatches_;
    result.queueFailed = queueFailed_;
    return result;
}

bool MqttService::enqueue(const char *suffix, const char *payload)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/%s", root_, suffix);
    // ESP-MQTT's own task performs TCP sends and reconnects.
    return esp_mqtt_client_enqueue(client_, topic, payload, 0, 1, 1, true) >= 0;
}

void MqttService::poll(uint32_t now, bool wifiConnected, const dhtsensor::Reading &dht)
{
    if (!client_ || !wifiConnected) return;
    if (!started_) {
        if (attempted_ && now - lastAttemptAt_ < mqttconfig::RECONNECT_INTERVAL_MS) return;
        attempted_ = true;
        lastAttemptAt_ = now;
        if (esp_mqtt_client_start(client_) != ESP_OK) { state_.store(mqttdata::State::Retrying); return; }
        started_ = true;
        attempted_ = false;
    }
    if (state_.load() != mqttdata::State::Connected) return;
    const uint32_t generation = generation_.load();
    if (seenGeneration_ != generation) {
        seenGeneration_ = generation;
        attempted_ = false;
    }
    if (attempted_ && now - lastAttemptAt_ < mqttconfig::PUBLISH_INTERVAL_MS) return;
    // One acknowledged batch at a time bounds RAM when PUBACK is delayed.
    if (esp_mqtt_client_get_outbox_size(client_) != 0) return;
    attempted_ = true;
    lastAttemptAt_ = now;
    StaticJsonDocument<mqttconfig::JSON_CAPACITY> json;
    mqttdata::payload(json, now, dht);
    char message[mqttconfig::PAYLOAD_BYTES];
    if (json.overflowed() || measureJson(json) >= sizeof(message)) { queueFailed_ = true; return; }
    serializeJson(json, message, sizeof(message));
    char temperature[24] = "null", humidity[24] = "null";
    if (mqttdata::dhtValid(dht, now)) {
        snprintf(temperature, sizeof(temperature), "%.2f", dht.temperature);
        snprintf(humidity, sizeof(humidity), "%.2f", dht.humidity);
    }
    const bool ok = enqueue("status", "online") && enqueue("temperature", temperature) &&
                    enqueue("humidity", humidity) && enqueue("telemetry", message);
    queueFailed_ = !ok;
    if (ok) ++queuedBatches_;
}
