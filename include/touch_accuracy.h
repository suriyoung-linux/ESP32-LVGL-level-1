#pragma once

#include <algorithm>
#include <cstdint>
#include "touch_calibration.h"

namespace touchcheck {
// These points are independent of the three points used to fit calibration.
constexpr touchcal::Point TARGETS[] = {{28, 72}, {292, 72}, {28, 166}, {292, 166}, {160, 120}};
constexpr uint8_t POINT_COUNT = 5, HOLD_SAMPLES = 12;
constexpr float MAX_ERROR_PX = 10, MAX_SPREAD_PX = 6;

class Session {
public:
    void reset() { *this = Session{}; }
    bool complete() const { return count_ == POINT_COUNT; }
    bool readyToRelease() const { return samples_ == HOLD_SAMPLES; }
    touchcal::Point heldPoint() const { return samples_ ? touchcal::Point{sum_.x / samples_, sum_.y / samples_} : touchcal::Point{0, 0}; }
    uint8_t count() const { return count_; }
    touchcal::Point target() const { return TARGETS[complete() ? POINT_COUNT - 1 : count_]; }
    touchcal::Point measured(uint8_t i) const { return measured_[i]; }
    float error(uint8_t i) const { return errors_[i]; }
    float averageError() const { return count_ ? total_ / count_ : 0; }
    float maxError() const { return maximum_; }
    bool passed() const { return complete() && maximum_ <= MAX_ERROR_PX; }

    void sample(touchcal::Point point) {
        if (complete() || readyToRelease()) return;
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) { clearHold(); return; }
        if (!samples_) minimum_ = maximumPoint_ = point;
        const touchcal::Point minimum = {std::min(minimum_.x, point.x), std::min(minimum_.y, point.y)};
        const touchcal::Point maximum = {std::max(maximumPoint_.x, point.x), std::max(maximumPoint_.y, point.y)};
        // A moving press must settle before it contributes a measurement.
        if (maximum.x - minimum.x > MAX_SPREAD_PX || maximum.y - minimum.y > MAX_SPREAD_PX) {
            clearHold();
            minimum_ = maximumPoint_ = point;
        } else { minimum_ = minimum; maximumPoint_ = maximum; }
        sum_.x += point.x;
        sum_.y += point.y;
        ++samples_;
    }

    void clearHold() { samples_ = 0; sum_ = {0, 0}; }

    bool release() {
        if (!readyToRelease() || complete()) { clearHold(); return false; }
        const touchcal::Point mean = {sum_.x / HOLD_SAMPLES, sum_.y / HOLD_SAMPLES};
        const auto expected = target();
        const float dx = mean.x - expected.x, dy = mean.y - expected.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        measured_[count_] = mean;
        errors_[count_++] = distance;
        total_ += distance;
        maximum_ = std::max(maximum_, distance);
        clearHold();
        return true;
    }

private:
    uint8_t count_ = 0, samples_ = 0;
    touchcal::Point sum_{0, 0}, minimum_{0, 0}, maximumPoint_{0, 0};
    touchcal::Point measured_[POINT_COUNT]{};
    float errors_[POINT_COUNT]{}, total_ = 0, maximum_ = 0;
};
}
