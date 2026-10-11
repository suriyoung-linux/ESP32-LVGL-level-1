#pragma once

#include <cmath>

namespace touchcal {
struct Point { float x; float y; };
struct Calibration {
    float ax = 0, bx = 0, cx = 0;
    float ay = 0, by = 0, cy = 0;
    Point project(Point p) const { return {ax * p.x + bx * p.y + cx, ay * p.x + by * p.y + cy}; }
    bool valid() const {
        return std::isfinite(ax) && std::isfinite(bx) && std::isfinite(cx) &&
               std::isfinite(ay) && std::isfinite(by) && std::isfinite(cy) &&
               std::abs(ax * by - bx * ay) > 0.000001f &&
               std::abs(ax) < 2 && std::abs(bx) < 2 && std::abs(ay) < 2 && std::abs(by) < 2;
    }
};

inline bool fit(const Point *raw, Calibration &out)
{
    // จุดบนจอ: บนซ้าย (20,20), บนขวา (300,20), ล่างซ้าย (20,220)
    const float dx1 = raw[1].x - raw[0].x, dy1 = raw[1].y - raw[0].y;
    const float dx2 = raw[2].x - raw[0].x, dy2 = raw[2].y - raw[0].y;
    const float det = dx1 * dy2 - dy1 * dx2;
    if (!std::isfinite(det) || std::abs(det) < 10000) return false;
    Calibration c;
    c.ax = 280 * dy2 / det; c.bx = -280 * dx2 / det;
    c.ay = -200 * dy1 / det; c.by = 200 * dx1 / det;
    c.cx = 20 - c.ax * raw[0].x - c.bx * raw[0].y;
    c.cy = 20 - c.ay * raw[0].x - c.by * raw[0].y;
    if (!c.valid()) return false;
    out = c;
    return true;
}
}
