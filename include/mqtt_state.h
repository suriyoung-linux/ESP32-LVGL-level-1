#pragma once

#include <cstdint>

namespace mqttdata {
enum class State { Unavailable, WaitingWifi, Connecting, Connected, Retrying };
struct Status {
    State state = State::Unavailable;
    uint32_t acknowledgements = 0, lastAckAtMs = 0, queuedBatches = 0;
    bool queueFailed = false;
};
inline const char *stateName(State state) {
    switch (state) {
    case State::WaitingWifi: return "Waiting for WiFi";
    case State::Connecting: return "Connecting";
    case State::Connected: return "Connected";
    case State::Retrying: return "Retrying connection";
    default: return "Service unavailable";
    }
}
}
