// ============================================================
// Renderer.cpp - 软件渲染器实现
// 像素格式：32bit DIB (内存字节序 B,G,R,A)，uint32 = AABBGGRR
// ============================================================
#include "Renderer.h"
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <cmath>

namespace nz {

static inline uint32_t packRGB(Color c) {
    return (255u << 24) | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | uint32_t(c.b);
}

// ---------- 构造 / 析构 ----------
Renderer::Renderer(int width, int height)
    : w_(width), h_(height), gw_(width / 4), gh_(height / 4) {
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w_;
    bi.biHeight = -h_;              // top-down，行序从屏幕上方开始
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    memDC_ = CreateCompatibleDC(screen);
    bitmap_ = CreateDIBSection(memDC_, reinterpret_cast<BITMAPINFO*>(&bi),
                               DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels_),
                               nullptr, 0);
    SelectObject(memDC_, bitmap_);   // 关键：把 DIB 选入内存 DC，BitBlt 才能呈现
    oldFont_ = SelectObject(memDC_, GetStockObject(DEFAULT_GUI_FONT));
    ReleaseDC(nullptr, screen);

    glowA_.resize(size_t(gw_) * gh_);
    glowB_.resize(size_t(gw_) * gh_);
    buildVignette();
    beginFrame(color(0, 0, 0));
}

Renderer::~Renderer() {
    if (memDC_) {
        if (font_) {
            SelectObject(memDC_, oldFont_);   // 先恢复 stock 字体，再删除自建字体
            DeleteObject(font_);
            font_ = nullptr;
        }
        DeleteDC(memDC_);
    }
    if (bitmap_) DeleteObject(bitmap_);
}

void Renderer::buildVignette() {
    vignette_.resize(size_t(w_) * h_);
    float cx = w_ * 0.5f, cy = h_ * 0.5f;
    float maxD = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            float dx = (x - cx) / maxD, dy = (y - cy) / maxD;
            float d = std::sqrt(dx * dx + dy * dy);
            float t = smoothstep((d - 0.38f) / 0.72f);
            vignette_[size_t(y) * w_ + x] = uint8_t(t * 255);
        }
    }
}

// ---------- 像素混合 ----------
void Renderer::blendPixel(uint32_t* dst, int x, int y, Color c, bool additive) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    uint32_t& d = dst[size_t(y) * w_ + x];
    int a = c.a;
    if (a <= 0) return;
    int dr = (d >> 16) & 0xff, dg = (d >> 8) & 0xff, db = d & 0xff;
    int r, g, b;
    if (additive) {
        r = std::min(255, dr + c.r * a / 255);
        g = std::min(255, dg + c.g * a / 255);
        b = std::min(255, db + c.b * a / 255);
    } else {
        int ia = 255 - a;
        r = (dr * ia + c.r * a) / 255;
        g = (dg * ia + c.g * a) / 255;
        b = (db * ia + c.b * a) / 255;
    }
    d = 0xff000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

void Renderer::pixel(int x, int y, Color c)            { blendPixel(pixels_, x, y, c, false); }
void Renderer::pixelAdd(int x, int y, Color c)         { blendPixel(pixels_, x, y, c, true); }

void Renderer::beginFrame(Color c) {
    uint32_t v = packRGB(c);
    for (size_t i = 0, n = size_t(w_) * h_; i < n; ++i) pixels_[i] = v;
}

// ---------- 矩形 ----------
void Renderer::fillRect(int x, int y, int rw, int rh, Color c) {
    if (rw <= 0 || rh <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w_, x + rw), y1 = std::min(h_, y + rh);
    if (x0 >= x1 || y0 >= y1) return;
    if (c.a >= 250) {
        uint32_t v = packRGB(c);
        for (int yy = y0; yy < y1; ++yy) {
            uint32_t* row = pixels_ + size_t(yy) * w_;
            for (int xx = x0; xx < x1; ++xx) row[xx] = v;
        }
    } else {
        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx) blendPixel(pixels_, xx, yy, c, false);
    }
}

void Renderer::fillRectAdd(int x, int y, int rw, int rh, Color c) {
    if (rw <= 0 || rh <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w_, x + rw), y1 = std::min(h_, y + rh);
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) blendPixel(pixels_, xx, yy, c, true);
}

void Renderer::strokeRect(int x, int y, int rw, int rh, Color c, int t) {
    fillRect(x, y, rw, t, c);
    fillRect(x, y + rh - t, rw, t, c);
    fillRect(x, y, t, rh, c);
    fillRect(x + rw - t, y, t, rh, c);
}

// ---------- 线段 ----------
static void segment(uint32_t* px, int w, int h, float x0, float y0, float x1, float y1,
                    Color c, float thick, bool additive) {
    float dx = x1 - x0, dy = y1 - y0;
    int steps = int(std::max(std::fabs(dx), std::fabs(dy))) + 1;
    int half = int(thick * 0.5f);
    if (steps > 4096) steps = 4096;
    for (int i = 0; i <= steps; ++i) {
        float t = float(i) / steps;
        int x = int(x0 + dx * t + 0.5f), y = int(y0 + dy * t + 0.5f);
        if (half <= 0) {
            if (additive) {
                if (x >= 0 && y >= 0 && x < w && y < h) {
                    uint32_t& d = px[size_t(y) * w + x];
                    int a = c.a;
                    d = 0xff000000u |
                        (uint32_t(std::min(255, int((d >> 16) & 255) + c.r * a / 255)) << 16) |
                        (uint32_t(std::min(255, int((d >> 8) & 255) + c.g * a / 255)) << 8) |
                        uint32_t(std::min(255, int(d & 255) + c.b * a / 255));
                }
            } else {
                if (x >= 0 && y >= 0 && x < w && y < h) {
                    uint32_t& d = px[size_t(y) * w + x];
                    int a = c.a, ia = 255 - a;
                    d = 0xff000000u |
                        (uint32_t((int((d >> 16) & 255) * ia + c.r * a) / 255) << 16) |
                        (uint32_t((int((d >> 8) & 255) * ia + c.g * a) / 255) << 8) |
                        uint32_t((int(d & 255) * ia + c.b * a) / 255);
                }
            }
        } else {
            for (int oy = -half; oy <= half; ++oy)
                for (int ox = -half; ox <= half; ++ox) {
                    int sx = x + ox, sy = y + oy;
                    if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
                    uint32_t& d = px[size_t(sy) * w + sx];
                    int a = c.a;
                    if (additive) {
                        d = 0xff000000u |
                            (uint32_t(std::min(255, int((d >> 16) & 255) + c.r * a / 255)) << 16) |
                            (uint32_t(std::min(255, int((d >> 8) & 255) + c.g * a / 255)) << 8) |
                            uint32_t(std::min(255, int(d & 255) + c.b * a / 255));
                    } else {
                        int ia = 255 - a;
                        d = 0xff000000u |
                            (uint32_t((int((d >> 16) & 255) * ia + c.r * a) / 255) << 16) |
                            (uint32_t((int((d >> 8) & 255) * ia + c.g * a) / 255) << 8) |
                            uint32_t((int(d & 255) * ia + c.b * a) / 255);
                    }
                }
        }
    }
}

void Renderer::line(float x0, float y0, float x1, float y1, Color c, float thickness) {
    segment(pixels_, w_, h_, x0, y0, x1, y1, c, thickness, false);
}
void Renderer::lineAdd(float x0, float y0, float x1, float y1, Color c, float thickness) {
    segment(pixels_, w_, h_, x0, y0, x1, y1, c, thickness, true);
}

// ---------- 圆 ----------
void Renderer::circle(float cx, float cy, float r, Color c, bool filled) {
    if (r <= 0) return;
    if (filled) {
        int y0 = std::max(0, int(cy - r)), y1 = std::min(h_ - 1, int(cy + r));
        for (int y = y0; y <= y1; ++y) {
            float dy = y + 0.5f - cy;
            float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
            int xa = int(cx - half), xb = int(cx + half);
            for (int x = std::max(0, xa); x <= std::min(w_ - 1, xb); ++x)
                blendPixel(pixels_, x, y, c, false);
        }
    } else {
        int steps = std::max(12, int(r * 6));
        for (int i = 0; i < steps; ++i) {
            float a0 = 6.2831853f * i / steps, a1 = 6.2831853f * (i + 1) / steps;
            line(cx + std::cos(a0) * r, cy + std::sin(a0) * r,
                 cx + std::cos(a1) * r, cy + std::sin(a1) * r, c, 1.0f);
        }
    }
}

void Renderer::circleAdd(float cx, float cy, float r, Color c, bool filled) {
    if (r <= 0) return;
    if (filled) {
        int y0 = std::max(0, int(cy - r)), y1 = std::min(h_ - 1, int(cy + r));
        for (int y = y0; y <= y1; ++y) {
            float dy = y + 0.5f - cy;
            float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
            int xa = int(cx - half), xb = int(cx + half);
            for (int x = std::max(0, xa); x <= std::min(w_ - 1, xb); ++x)
                blendPixel(pixels_, x, y, c, true);
        }
    } else {
        int steps = std::max(12, int(r * 6));
        for (int i = 0; i < steps; ++i) {
            float a0 = 6.2831853f * i / steps, a1 = 6.2831853f * (i + 1) / steps;
            lineAdd(cx + std::cos(a0) * r, cy + std::sin(a0) * r,
                    cx + std::cos(a1) * r, cy + std::sin(a1) * r, c, 1.0f);
        }
    }
}

void Renderer::ring(float cx, float cy, float r, float thickness, Color c) {
    float inner = std::max(0.0f, r - thickness * 0.5f), outer = r + thickness * 0.5f;
    int steps = std::max(20, int(r * 4));
    for (int i = 0; i < steps; ++i) {
        float a0 = 6.2831853f * i / steps, a1 = 6.2831853f * (i + 1) / steps;
        line(cx + std::cos(a0) * inner, cy + std::sin(a0) * inner,
             cx + std::cos(a1) * outer, cy + std::sin(a1) * outer, c, 1.0f);
    }
}

// ---------- 霓虹图元 ----------
void Renderer::glowCircle(float cx, float cy, float r, Color core, float halo) {
    circleAdd(cx, cy, r * halo,       Color{core.r, core.g, core.b, 14});
    circleAdd(cx, cy, r * halo * 0.7f, Color{core.r, core.g, core.b, 22});
    circleAdd(cx, cy, r * 1.25f,       Color{core.r, core.g, core.b, 45});
    circle(cx, cy, r, core, true);
}

void Renderer::glowLine(float x0, float y0, float x1, float y1, Color core, float halo) {
    lineAdd(x0, y0, x1, y1, Color{core.r, core.g, core.b, 18}, halo * 4.0f);
    lineAdd(x0, y0, x1, y1, Color{core.r, core.g, core.b, 40}, halo * 2.0f);
    line(x0, y0, x1, y1, core, 1.6f);
}

void Renderer::glowRing(float cx, float cy, float r, float thickness, Color core, float halo) {
    circleAdd(cx, cy, r + halo * 3.0f, Color{core.r, core.g, core.b, 12}, false);
    circleAdd(cx, cy, r + halo * 1.5f, Color{core.r, core.g, core.b, 26}, false);
    ring(cx, cy, r, thickness, core);
}

void Renderer::beam(float x0, float y0, float x1, float y1, Color c, float width) {
    lineAdd(x0, y0, x1, y1, Color{c.r, c.g, c.b, 26}, width * 4.0f);
    lineAdd(x0, y0, x1, y1, Color{c.r, c.g, c.b, 70}, width * 2.0f);
    line(x0, y0, x1, y1, c, width);
}

// ---------- 凸多边形 ----------
static void fillTri(uint32_t* px, int w, int h, const Vec2& a, const Vec2& b,
                    const Vec2& c, Color col) {
    float minx = std::min({a.x, b.x, c.x}), maxx = std::max({a.x, b.x, c.x});
    float miny = std::min({a.y, b.y, c.y}), maxy = std::max({a.y, b.y, c.y});
    int x0 = std::max(0, int(minx)), x1 = std::min(w - 1, int(maxx));
    int y0 = std::max(0, int(miny)), y1 = std::min(h - 1, int(maxy));
    float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(area) < 1e-5f) return;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float px0 = x + 0.5f - a.x, py0 = y + 0.5f - a.y;
            float w0 = ((b.x - a.x) * py0 - (b.y - a.y) * px0) / area;
            float w1 = ((px0 * (c.y - a.y) - py0 * (c.x - a.x))) / area;
            // 注意 w1 符号：使用上面叉积形式时 w1 = cross(c-a, p-a)/area 的负值处理
            float w2 = 1.0f - w0 - w1;
            // 统一按绝对值判断（area 可为负）
            if (w0 >= -0.001f && w1 >= -0.001f && w2 >= -0.001f) {
                uint32_t& d = px[size_t(y) * w + x];
                int al = col.a, ia = 255 - al;
                d = 0xff000000u |
                    (uint32_t((int((d >> 16) & 255) * ia + col.r * al) / 255) << 16) |
                    (uint32_t((int((d >> 8) & 255) * ia + col.g * al) / 255) << 8) |
                    uint32_t((int(d & 255) * ia + col.b * al) / 255);
            }
        }
    }
}

void Renderer::polygon(const Vec2* pts, int count, Color c) {
    if (count < 3) return;
    for (int i = 1; i < count - 1; ++i) fillTri(pixels_, w_, h_, pts[0], pts[i], pts[i + 1], c);
}

void Renderer::polygonGlow(const Vec2* pts, int count, Color c, float halo) {
    if (count < 3) return;
    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        glowLine(pts[i].x, pts[i].y, pts[j].x, pts[j].y, c, halo);
    }
    polygon(pts, count, Color{c.r, c.g, c.b, 70});
}

// ---------- 后处理 ----------
void Renderer::postProcess(float bloom, float scanline, float flash,
                           Color flashColor, float vignette) {
    // 1) 亮部下采样到 1/4 缓冲
    std::fill(glowA_.begin(), glowA_.end(), 0u);
    for (int y = 0; y < gh_; ++y) {
        for (int x = 0; x < gw_; ++x) {
            int sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int dy = 0; dy < 4; ++dy) {
                int sy = y * 4 + dy;
                if (sy >= h_) break;
                const uint32_t* src = pixels_ + size_t(sy) * w_ + x * 4;
                for (int dx = 0; dx < 4; ++dx) {
                    int sx = x * 4 + dx;
                    if (sx >= w_) break;
                    uint32_t v = src[dx];
                    int r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
                    if (std::max(r, std::max(g, b)) > 105) {
                        sr += r; sg += g; sb += b; ++cnt;
                    }
                }
            }
            if (cnt) glowA_[size_t(y) * gw_ + x] =
                (uint32_t(std::min(255, sr / cnt)) << 16) |
                (uint32_t(std::min(255, sg / cnt)) << 8) |
                uint32_t(std::min(255, sb / cnt));
        }
    }

    // 2) 可分离盒式模糊（半径 2）
    const int R = 2;
    for (int y = 0; y < gh_; ++y) {
        const uint32_t* src = glowA_.data() + size_t(y) * gw_;
        uint32_t* dst = glowB_.data() + size_t(y) * gw_;
        for (int x = 0; x < gw_; ++x) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int k = -R; k <= R; ++k) {
                int sx = x + k;
                if (sx < 0 || sx >= gw_) continue;
                uint32_t v = src[sx];
                sr += (v >> 16) & 255; sg += (v >> 8) & 255; sb += v & 255; ++n;
            }
            if (n) dst[x] = (uint32_t(sr / n) << 16) | (uint32_t(sg / n) << 8) | uint32_t(sb / n);
            else dst[x] = 0;
        }
    }
    for (int x = 0; x < gw_; ++x) {
        for (int y = 0; y < gh_; ++y) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int k = -R; k <= R; ++k) {
                int sy = y + k;
                if (sy < 0 || sy >= gh_) continue;
                uint32_t v = glowB_[size_t(sy) * gw_ + x];
                sr += (v >> 16) & 255; sg += (v >> 8) & 255; sb += v & 255; ++n;
            }
            if (n) glowA_[size_t(y) * gw_ + x] =
                (uint32_t(sr / n) << 16) | (uint32_t(sg / n) << 8) | uint32_t(sb / n);
            else glowA_[size_t(y) * gw_ + x] = 0;
        }
    }

    // 3) 合成泛光 + 扫描线 + 暗角 + 受击闪屏
    const float bScale = bloom;
    for (int y = 0; y < h_; ++y) {
        uint32_t* row = pixels_ + size_t(y) * w_;
        const uint8_t* vigRow = vignette_.data() + size_t(y) * w_;
        const uint32_t* grow = glowA_.data() + size_t(y / 4) * gw_;
        float scanMul = ((y & 1) == 0) ? 1.0f : (1.0f - scanline);
        for (int x = 0; x < w_; ++x) {
            uint32_t v = row[x];
            uint32_t g = grow[x >> 2];
            int r = int(std::min(255.0f, float((v >> 16) & 255) + float((g >> 16) & 255) * bScale));
            int gg = int(std::min(255.0f, float((v >> 8) & 255) + float((g >> 8) & 255) * bScale));
            int b = int(std::min(255.0f, float(v & 255) + float(g & 255) * bScale));
            float dim = scanMul * (1.0f - vignette * (vigRow[x] / 255.0f));
            r = int(r * dim); gg = int(gg * dim); b = int(b * dim);
            if (flash > 0.001f) {
                float a = std::min(1.0f, flash);
                r = int(r * (1 - a) + flashColor.r * a);
                gg = int(gg * (1 - a) + flashColor.g * a);
                b = int(b * (1 - a) + flashColor.b * a);
            }
            row[x] = 0xff000000u | (uint32_t(r) << 16) | (uint32_t(gg) << 8) | uint32_t(b);
        }
    }
}

// ---------- GDI 文字 ----------
void Renderer::flush() { GdiFlush(); }

void Renderer::setFont(int pixelHeight, int weight, const wchar_t* face) {
    static int curH = -1, curW = -1;
    static wchar_t curFace[64] = L"";
    if (curH == pixelHeight && curW == weight && wcscmp(curFace, face) == 0) return;
    curH = pixelHeight; curW = weight;
    wcsncpy(curFace, face, 63); curFace[63] = 0;
    if (font_) { SelectObject(memDC_, oldFont_); DeleteObject(font_); font_ = nullptr; }
    font_ = CreateFontW(-pixelHeight, 0, 0, 0, weight, 0, 0, 0,
                        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    if (font_) SelectObject(memDC_, font_);
}

void Renderer::text(int x, int y, const wchar_t* s, Color c, int align) {
    if (!s || !*s) return;
    if (align != 0) {
        SIZE sz{};
        GetTextExtentPoint32W(memDC_, s, int(wcslen(s)), &sz);
        if (align == 1) x -= sz.cx / 2;
        else if (align == 2) x -= sz.cx;
    }
    SetBkMode(memDC_, TRANSPARENT);
    SetTextColor(memDC_, RGB(c.r, c.g, c.b));
    TextOutW(memDC_, x, y, s, int(wcslen(s)));
}

void Renderer::textShadow(int x, int y, const wchar_t* s, Color c, int align) {
    text(x + 2, y + 2, s, color(0, 0, 0, 200), align);
    text(x, y, s, c, align);
}

int Renderer::textWidth(const wchar_t* s) {
    if (!s) return 0;
    SIZE sz{};
    GetTextExtentPoint32W(memDC_, s, int(wcslen(s)), &sz);
    return sz.cx;
}

// ---------- 呈现 ----------
void Renderer::present(HWND hwnd, int shakeX, int shakeY) {
    GdiFlush();
    HDC dc = GetDC(hwnd);
    if (dc) {
        if (shakeX || shakeY) {
            PatBlt(dc, 0, 0, w_, h_, BLACKNESS);
            BitBlt(dc, shakeX, shakeY, w_, h_, memDC_, 0, 0, SRCCOPY);
        } else {
            BitBlt(dc, 0, 0, w_, h_, memDC_, 0, 0, SRCCOPY);
        }
        ReleaseDC(hwnd, dc);
    }
}

} // namespace nz
