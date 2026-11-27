#pragma once
// ============================================================
// Renderer.h - 零依赖 32 位 DIB 软件渲染器
// 特性：加法混合霓虹光、Bloom 泛光、扫描线、暗角、受击闪屏、
//       GDI 中文文字、屏幕震动呈现
// ============================================================
#include <windows.h>
#include <vector>
#include <cstdint>
#include "Util.h"

namespace nz {

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    int width() const { return w_; }
    int height() const { return h_; }

    // 每帧开始：清屏
    void beginFrame(Color c);

    // 基础像素 / 图元
    void pixel(int x, int y, Color c);              // 普通 alpha 混合
    void pixelAdd(int x, int y, Color c);           // 加法混合（发光）
    void fillRect(int x, int y, int w, int h, Color c);
    void fillRectAdd(int x, int y, int w, int h, Color c);
    void strokeRect(int x, int y, int w, int h, Color c, int t = 1);
    void line(float x0, float y0, float x1, float y1, Color c, float thickness = 1.0f);
    void lineAdd(float x0, float y0, float x1, float y1, Color c, float thickness = 1.0f);
    void circle(float cx, float cy, float r, Color c, bool filled = true);
    void circleAdd(float cx, float cy, float r, Color c, bool filled = true);
    void ring(float cx, float cy, float r, float thickness, Color c);

    // 霓虹特效图元
    void glowCircle(float cx, float cy, float r, Color core, float halo = 2.6f);
    void glowLine(float x0, float y0, float x1, float y1, Color core, float halo = 2.4f);
    void glowRing(float cx, float cy, float r, float thickness, Color core, float halo = 2.5f);
    void beam(float x0, float y0, float x1, float y1, Color c, float width);

    // 凸多边形（三角扇填充）：用于敌人、Boss 的独特造型
    void polygon(const Vec2* pts, int count, Color c);
    void polygonGlow(const Vec2* pts, int count, Color c, float halo = 2.0f);

    // 后处理（在 GDI 文字之前调用）
    void postProcess(float bloom = 0.85f, float scanline = 0.10f,
                     float flash = 0.0f, Color flashColor = Color{255, 40, 60, 255},
                     float vignette = 0.55f);

    // GDI 文字（在游戏逻辑与后处理之后调用，保证清晰）
    void flush();   // CPU 写像素后、GDI 绘制前调用
    void setFont(int pixelHeight, int weight = FW_NORMAL, const wchar_t* face = L"Microsoft YaHei");
    void text(int x, int y, const wchar_t* s, Color c, int align = 0);
    void textShadow(int x, int y, const wchar_t* s, Color c, int align = 0);
    int  textWidth(const wchar_t* s);

    // 呈现到窗口（shakeX/Y 为屏幕震动偏移）
    void present(HWND hwnd, int shakeX = 0, int shakeY = 0);

private:
    void blendPixel(uint32_t* dst, int x, int y, Color c, bool additive);
    void buildVignette();

    int w_, h_;
    HDC   memDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    uint32_t* pixels_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ oldFont_ = nullptr;

    int gw_, gh_;                         // 泛光缓冲尺寸 = 1/4
    std::vector<uint32_t> glowA_, glowB_;
    std::vector<uint8_t> vignette_;      // 0..255 暗角强度表
};

} // namespace nz
