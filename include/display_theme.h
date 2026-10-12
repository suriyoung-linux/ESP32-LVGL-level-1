#pragma once

#include <cstdint>

namespace displaytheme {
enum class Mode : uint16_t { Dark, Light, Auto };
struct Config {
    Mode mode = Mode::Dark;
    uint16_t lightStart = 7 * 60, darkStart = 19 * 60;
    bool valid() const {
        return static_cast<uint16_t>(mode) <= static_cast<uint16_t>(Mode::Auto) &&
               lightStart < 1440 && darkStart < 1440 && lightStart != darkStart;
    }
};

inline bool lightAt(const Config &config, uint16_t minute)
{
    if (config.mode != Mode::Auto) return config.mode == Mode::Light;
    if (config.lightStart < config.darkStart) return minute >= config.lightStart && minute < config.darkStart;
    return minute >= config.lightStart || minute < config.darkStart;
}

struct Palette {
    bool light = false;
    uint16_t background = 0, card = 0x18C3, inactive = 0x4208;
    uint16_t foreground = 0xFFFF, muted = 0xD69A;
    uint16_t cyan = 0x07FF, warning = 0xFFE0, success = 0x07E0, error = 0xF800, blue = 0x001F;
    uint16_t active = 0x03EF;
    static Palette make(bool light) {
        Palette p;
        p.light = light;
        if (light) {
            p.background = 0xFFFF; p.card = 0xEF7D; p.inactive = 0xBDF7;
            p.foreground = 0x18C3; p.muted = 0x5AEB;
            p.cyan = 0x03EF; p.warning = 0x8A60; p.success = 0x03E0;
            p.error = 0xB800; p.blue = 0x0018;
        }
        return p;
    }
    uint16_t text(uint16_t color) const {
        switch (color) {
        case 0xFFFF: return foreground;
        case 0xD69A: return muted;
        case 0x07FF: return cyan;
        case 0xFFE0: return warning;
        case 0x07E0: return success;
        case 0xF800: return error;
        case 0x001F: return blue;
        default: return color;
        }
    }
};
}
