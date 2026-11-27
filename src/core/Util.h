#pragma once
// ============================================================
// Util.h - 通用数学 / 颜色 / 随机数工具
// 纯头文件，所有函数均为 inline，不产生链接依赖
// ============================================================
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <random>

namespace nz {

struct Color {
    uint8_t r, g, b, a;
};

struct Vec2 {
    float x, y;
};

inline Color color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return Color{r, g, b, a};
}

// 预定义的赛博朋克霓虹色板
namespace Palette {
    inline const Color bgDeep   = color(7, 8, 24);
    inline const Color bgPanel  = color(14, 16, 40);
    inline const Color cyan     = color(40, 240, 255);
    inline const Color blue     = color(60, 140, 255);
    inline const Color purple   = color(170, 80, 255);
    inline const Color pink     = color(255, 70, 170);
    inline const Color red      = color(255, 70, 90);
    inline const Color orange   = color(255, 160, 60);
    inline const Color yellow   = color(255, 225, 90);
    inline const Color green    = color(80, 255, 170);
    inline const Color white    = color(235, 245, 255);
    inline const Color dim      = color(120, 135, 175);
    inline const Color grid     = color(35, 70, 125);
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float smoothstep(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

inline float distance(float ax, float ay, float bx, float by) {
    return std::sqrt(dist2(ax, ay, bx, by));
}

inline float angleTo(float ax, float ay, float bx, float by) {
    return std::atan2(by - ay, bx - ax);
}

// 角度差规整到 [-PI, PI]
inline float angleDelta(float a, float b) {
    float d = std::fmod(b - a + 3.14159265f, 6.28318531f);
    if (d < 0) d += 6.28318531f;
    return d - 3.14159265f;
}

// 全局随机数（固定种子可复现，运行时以时间播种）
class RNG {
public:
    static RNG& instance() {
        static RNG rng;
        return rng;
    }
    float next() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen_);
    }
    float range(float a, float b) {
        return a + (b - a) * next();
    }
    int irange(int a, int b) {
        return std::uniform_int_distribution<int>(a, b)(gen_);
    }
    bool chance(float p) {
        return next() < p;
    }
    void seed(uint32_t s) { gen_.seed(s); }
private:
    RNG() : gen_(std::random_device{}()) {}
    std::mt19937 gen_;
};

inline float randf() { return RNG::instance().next(); }
inline float rrange(float a, float b) { return RNG::instance().range(a, b); }
inline int   rint(int a, int b) { return RNG::instance().irange(a, b); }

} // namespace nz
