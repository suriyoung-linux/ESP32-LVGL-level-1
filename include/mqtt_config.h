#pragma once

#include <cstddef>
#include <cstdint>

namespace mqttconfig {
constexpr char URI[] = "mqtt://broker.hivemq.com:1883";
constexpr char HOST[] = "broker.hivemq.com";
constexpr uint16_t PORT = 1883;
constexpr uint32_t PUBLISH_INTERVAL_MS = 10000, RECONNECT_INTERVAL_MS = 15000;
constexpr int KEEPALIVE_SECONDS = 30, NETWORK_TIMEOUT_MS = 3000, TASK_STACK_BYTES = 6144;
constexpr size_t JSON_CAPACITY = 256, PAYLOAD_BYTES = 256;
}
