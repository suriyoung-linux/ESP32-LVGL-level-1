#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace touchgesture {
enum class Kind { None, Tap, SwipeLeft, SwipeRight };
struct Point { float x = 0, y = 0; };
struct Event { Kind kind = Kind::None; Point position; };

class Tracker {
public:
    void track(float x, float y, uint32_t now)
    {
        if (!active_) {
            active_ = true;
            first_ = {x, y};
            startedAt_ = now;
            maxX_ = maxY_ = 0;
        }
        last_ = {x, y};
        maxX_ = std::max(maxX_, std::abs(x - first_.x));
        maxY_ = std::max(maxY_, std::abs(y - first_.y));
    }

    Event release(uint32_t now)
    {
        if (!active_) return {};
        active_ = false;
        const float dx = last_.x - first_.x;
        const float dy = last_.y - first_.y;
        if (now - startedAt_ > 1500) return {};
        if (std::abs(dx) >= 60 && std::abs(dx) > 1.5f * maxY_ && std::abs(dy) <= 50) {
            return {dx < 0 ? Kind::SwipeLeft : Kind::SwipeRight, first_};
        }
        if (maxX_ <= 15 && maxY_ <= 15) return {Kind::Tap, first_};
        return {};  // Vertical drags and drags back to the starting point are not taps.
    }

    void cancel() { active_ = false; }

private:
    bool active_ = false;
    Point first_, last_;
    uint32_t startedAt_ = 0;
    float maxX_ = 0, maxY_ = 0;
};
}
