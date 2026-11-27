// ============================================================
// Entities.cpp - 实体绘制（每种敌人拥有独立造型与动画）
// ============================================================
#include "Entities.h"
#include <cmath>

namespace nz {

static Vec2 turn(float x, float y, float a) {
    float c = std::cos(a), s = std::sin(a);
    return Vec2{x * c - y * s, x * s + y * c};
}

// ---------- 场景障碍：霓虹数据柱 ----------
void drawObstacle(Renderer& r, const Obstacle& o, float camX, float camY, float time) {
    float sx = o.pos.x - camX, sy = o.pos.y - camY;
    float hw = o.w * 0.5f, hh = o.h * 0.5f;
    if (sx + hw < -40 || sx - hw > r.width() + 40 || sy + hh < -40 || sy - hh > r.height() + 40) return;

    Color edge = o.variant % 2 ? Palette::purple : Palette::blue;
    r.fillRect(int(sx - hw), int(sy - hh), int(o.w), int(o.h), color(10, 12, 32, 235));
    r.fillRect(int(sx - hw + 4), int(sy - hh + 4), int(o.w - 8), int(o.h - 8), color(16, 20, 48, 220));
    r.strokeRect(int(sx - hw), int(sy - hh), int(o.w), int(o.h), color(edge.r, edge.g, edge.b, 150), 2);

    // 内部电路纹路
    int step = 12;
    for (int yy = int(sy - hh + step); yy < sy + hh - 4; yy += step) {
        r.line(sx - hw + 5, float(yy), sx + hw - 5, float(yy), color(edge.r, edge.g, edge.b, 30), 1);
    }
    for (int xx = int(sx - hw + step); xx < sx + hw - 4; xx += step) {
        r.line(float(xx), sy - hh + 5, float(xx), sy + hh - 5, color(edge.r, edge.g, edge.b, 24), 1);
    }
    // 四角高亮 + 中央呼吸灯
    float pulse = 0.5f + 0.5f * std::sin(time * 2.4f + o.pos.x * 0.01f);
    Color gc{edge.r, edge.g, edge.b, uint8_t(70 + pulse * 90)};
    r.glowCircle(sx - hw + 5, sy - hh + 5, 2.5f, gc, 1.8f);
    r.glowCircle(sx + hw - 5, sy + hh - 5, 2.5f, gc, 1.8f);
    r.glowCircle(sx, sy, 3.0f + pulse * 2.0f, gc, 2.0f);
}

// ---------- 玩家：机械猎人 ----------
void drawPlayer(Renderer& r, const Player& p, float camX, float camY, float time) {
    float sx = p.pos.x - camX, sy = p.pos.y - camY;
    float blink = (p.invuln > 0 && int(p.invuln * 16) % 2 == 0);
    Color main = p.flash > 0 ? Palette::white : Palette::cyan;
    Color body = p.flash > 0 ? color(255, 220, 220, 255) : color(18, 32, 62, 250);
    if (blink) { main.a = 90; body.a = 140; }

    // 地面投影
    r.circle(sx, sy + 12, 15, color(0, 0, 0, 90), true);

    // 推进器尾焰（朝移动反方向）
    float sp = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
    if (sp > 20) {
        Vec2 back = turn(-p.angle, 0, 0);
        float bx = -std::cos(p.angle), by = -std::sin(p.angle);
        float flick = 8.0f + 5.0f * std::sin(time * 30.0f);
        r.glowLine(sx + bx * 8, sy + by * 8,
                   sx + bx * (10 + flick), sy + by * (10 + flick),
                   color(60, 170, 255, 150), 1.6f);
        (void)back;
    }

    float bob = std::sin(p.walkPhase) * 1.5f;
    // 双腿：随移动摆动
    Vec2 perp{-std::sin(p.angle), std::cos(p.angle)};
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        float sw = std::sin(p.walkPhase + i * 3.14159f) * 5.0f * std::min(1.0f, sp / 100.0f);
        float lx = sx + perp.x * 7 * s + std::cos(p.angle) * sw;
        float ly = sy + perp.y * 7 * s + std::sin(p.angle) * sw;
        r.glowLine(sx + perp.x * 4 * s, sy + perp.y * 4 * s,
                   lx + std::cos(p.angle) * 4, ly + std::sin(p.angle) * 4 + bob,
                   color(70, 140, 220, 200), 1.2f);
    }

    // 躯干：菱形装甲
    Vec2 hull[4];
    Vec2 local[4] = {{16, 0}, {0, 13}, {-15, 0}, {0, -13}};
    for (int i = 0; i < 4; ++i) {
        Vec2 v = turn(local[i].x, local[i].y, p.angle);
        hull[i] = {sx + v.x, sy + v.y + bob};
    }
    r.polygon(hull, 4, body);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        r.glowLine(hull[i].x, hull[i].y, hull[j].x, hull[j].y, main, 1.3f);
    }

    // 肩甲
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        float px = sx + perp.x * 13 * s - std::cos(p.angle) * 2;
        float py = sy + perp.y * 13 * s - std::sin(p.angle) * 2 + bob;
        r.circleAdd(px, py, 5.5f, color(main.r, main.g, main.b, 45), true);
        r.circle(px, py, 3.2f, color(main.r, main.g, main.b, 220), true);
    }

    // 头部 + 面罩（朝向鼠标）
    float hx = sx + std::cos(p.angle) * 5, hy = sy + std::sin(p.angle) * 5 + bob - 2;
    r.circle(hx, hy, 7.5f, color(12, 22, 46, 255), true);
    r.ring(hx, hy, 7.5f, 1.5f, color(main.r, main.g, main.b, 200));
    Vec2 v0 = turn(1, -3, p.angle), v1 = turn(9, -2, p.angle), v2 = turn(9, 2, p.angle), v3 = turn(1, 3, p.angle);
    r.glowLine(hx + v0.x, hy + v0.y, hx + v1.x, hy + v1.y, Palette::pink, 1.0f);
    r.glowLine(hx + v1.x, hy + v1.y, hx + v2.x, hy + v2.y, Palette::pink, 1.0f);
    r.glowLine(hx + v2.x, hy + v2.y, hx + v3.x, hy + v3.y, Palette::pink, 1.0f);

    // 武器：伸出枪管
    float mx = sx + std::cos(p.angle) * 20, my = sy + std::sin(p.angle) * 20 + bob;
    float ex = sx + std::cos(p.angle) * 31, ey = sy + std::sin(p.angle) * 31 + bob;
    r.glowLine(mx, my, ex, ey, Palette::blue, 1.3f);
    r.circleAdd(ex, ey, 3.0f, color(80, 200, 255, 90), true);

    // 能量核心
    float pulse = 0.6f + 0.4f * std::sin(time * 5.0f);
    r.glowCircle(sx, sy + bob, 3.0f + pulse, color(main.r, main.g, main.b, 230), 2.2f);

    // 护盾：旋转的 4 段弧
    if (p.shield > 0.5f) {
        float ratio = p.shield / std::max(1.0f, p.maxShield);
        for (int k = 0; k < 4; ++k) {
            if (ratio < (k + 0.5f) / 4) break;
            float a0 = time * 0.7f + k * 1.5707963f;
            float a1 = a0 + 1.05f;
            int steps = 8;
            for (int i = 0; i < steps; ++i) {
                float t0 = a0 + (a1 - a0) * i / steps;
                float t1 = a0 + (a1 - a0) * (i + 1) / steps;
                r.lineAdd(sx + std::cos(t0) * 23, sy + std::sin(t0) * 23,
                          sx + std::cos(t1) * 23, sy + std::sin(t1) * 23,
                          color(60, 210, 255, 85), 2.0f);
            }
        }
    }
}

// ---------- 敌人 ----------
void drawEnemy(Renderer& r, const Enemy& e, float camX, float camY, float time) {
    float sx = e.pos.x - camX, sy = e.pos.y - camY;
    if (sx < -90 || sx > r.width() + 90 || sy < -90 || sy > r.height() + 90) return;

    float grow = smoothstep(e.spawnT);
    float scale = 0.25f + 0.75f * grow;
    float rad = e.radius * scale;
    if (e.spawnT < 1.0f) {
        r.glowRing(sx, sy, rad + (1.0f - grow) * 45, 2, Palette::purple, 2.0f);
        r.fillRect(int(sx - 2), int(sy - rad - 20 - (1.0f - grow) * 20), 4,
                   int(rad * 2 + 20), color(180, 80, 255, 120));
    }
    if (grow <= 0.02f) return;

    Color flashCol = e.flash > 0 ? Palette::white : Palette::red;
    float wob = std::sin(e.phase) * 0.12f;

    switch (e.type) {
    case EnemyType::Grunt: {
        // 六边形机械兵：紫色装甲 + 红色独眼 + 两条机械腿
        Color fill = e.flash > 0 ? color(255, 230, 235, 255) : color(38, 16, 60, 245);
        Color edge = e.flash > 0 ? Palette::white : color(215, 75, 190, 240);
        for (int i = 0; i < 2; ++i) {
            float s = i ? 1 : -1;
            r.glowLine(sx + s * 6, sy + 6, sx + s * 11 + std::sin(e.phase + i * 2) * 4, sy + 16,
                       color(150, 70, 200, 180), 1.1f);
        }
        Vec2 pts[6];
        for (int i = 0; i < 6; ++i) {
            float a = 6.2831853f * i / 6 + wob;
            pts[i] = {sx + std::cos(a) * rad, sy + std::sin(a) * rad};
        }
        r.polygon(pts, 6, fill);
        for (int i = 0; i < 6; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 6].x, pts[(i + 1) % 6].y, edge, 1.0f);
        r.glowCircle(sx + std::cos(e.angle) * 4, sy + std::sin(e.angle) * 4, 3.2f, flashCol, 1.8f);
        break;
    }
    case EnemyType::Chaser: {
        // 快速追踪怪：粉红飞镖形态 + 拖尾刀片
        Color fill = e.flash > 0 ? Palette::white : color(58, 12, 58, 245);
        Vec2 pts[4];
        Vec2 loc[4] = {{rad * 1.45f, 0}, {0, rad * 0.75f}, {-rad, 0}, {0, -rad * 0.75f}};
        for (int i = 0; i < 4; ++i) {
            Vec2 v = turn(loc[i].x, loc[i].y, e.angle);
            pts[i] = {sx + v.x, sy + v.y};
        }
        r.polygon(pts, 4, fill);
        for (int i = 0; i < 4; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 4].x, pts[(i + 1) % 4].y, Palette::pink, 1.0f);
        for (int i = 0; i < 2; ++i) {
            float s = i ? 1 : -1;
            Vec2 a = turn(-rad * 0.9f, s * rad * 0.5f, e.angle);
            Vec2 b = turn(-rad * 2.3f, s * rad * 1.1f, e.angle);
            r.glowLine(sx + a.x, sy + a.y, sx + b.x, sy + b.y, color(255, 90, 200, 170), 1.1f);
        }
        r.glowCircle(sx + std::cos(e.angle) * rad * 0.55f, sy + std::sin(e.angle) * rad * 0.55f,
                     2.6f, Palette::yellow, 1.6f);
        break;
    }
    case EnemyType::Turret: {
        // 远程炮台：八角基座 + 旋转炮管 + 蓝色核心
        Vec2 pts[8];
        for (int i = 0; i < 8; ++i) {
            float a = 6.2831853f * i / 8 + 0.392699f;
            pts[i] = {sx + std::cos(a) * rad, sy + std::sin(a) * rad};
        }
        r.polygon(pts, 8, e.flash > 0 ? color(230, 245, 255, 255) : color(12, 30, 58, 250));
        for (int i = 0; i < 8; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 8].x, pts[(i + 1) % 8].y, Palette::blue, 1.0f);
        float bx = sx + std::cos(e.angle) * (rad + 13), by = sy + std::sin(e.angle) * (rad + 13);
        r.glowLine(sx, sy, bx, by, Palette::cyan, 1.4f);
        r.circle(bx, by, 4, color(30, 60, 100), true);
        r.glowCircle(sx, sy, rad * 0.35f, Palette::cyan, 2.0f);
        r.ring(sx, sy, rad * 0.72f, 1.5f, color(60, 180, 255, 160));
        break;
    }
    case EnemyType::Elite: {
        // 精英怪：八向尖刺菱形 + 双层护盾 + 橙色核心
        Vec2 pts[8];
        for (int i = 0; i < 8; ++i) {
            float a = 6.2831853f * i / 8 + wob;
            float rr = (i & 1) ? rad * 0.62f : rad * 1.35f;
            pts[i] = {sx + std::cos(a) * rr, sy + std::sin(a) * rr};
        }
        r.polygon(pts, 8, e.flash > 0 ? color(255, 245, 220, 255) : color(50, 26, 12, 250));
        for (int i = 0; i < 8; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 8].x, pts[(i + 1) % 8].y, Palette::orange, 1.2f);
        Vec2 inner[3];
        for (int i = 0; i < 3; ++i) {
            float a = -e.phase * 1.4f + i * 2.094395f;
            inner[i] = {sx + std::cos(a) * rad * 0.5f, sy + std::sin(a) * rad * 0.5f};
        }
        r.polygon(inner, 3, color(120, 55, 15, 200));
        r.glowCircle(sx, sy, rad * 0.22f, e.flash > 0 ? Palette::white : Palette::red, 2.4f);
        for (int k = 0; k < 6; ++k) {
            float a = e.phase * 0.8f + k * 1.047197f;
            r.lineAdd(sx + std::cos(a) * rad * 1.55f, sy + std::sin(a) * rad * 1.55f,
                      sx + std::cos(a) * rad * 1.85f, sy + std::sin(a) * rad * 1.85f,
                      color(255, 170, 60, 100), 2.0f);
        }
        break;
    }
    case EnemyType::Boss: {
        // 最终 Boss：机械核心，多层旋转装甲 + 四条机械臂
        float t = time;
        r.circleAdd(sx, sy, rad * 2.4f, color(180, 40, 220, 18), true);
        // 外圈装甲板
        for (int k = 0; k < 8; ++k) {
            float a = t * 0.35f + k * 0.785398f;
            float ar = rad * 1.72f;
            float px = sx + std::cos(a) * ar, py = sy + std::sin(a) * ar;
            Vec2 plate[4];
            Vec2 l[4] = {{18, -9}, {18, 9}, {-12, 7}, {-12, -7}};
            for (int i = 0; i < 4; ++i) {
                Vec2 v = turn(l[i].x * scale, l[i].y * scale, a);
                plate[i] = {px + v.x, py + v.y};
            }
            r.polygon(plate, 4, color(38, 14, 62, 250));
            for (int i = 0; i < 4; ++i)
                r.glowLine(plate[i].x, plate[i].y, plate[(i + 1) % 4].x, plate[(i + 1) % 4].y,
                           Palette::purple, 1.1f);
            r.glowCircle(px, py, 3 * scale, Palette::pink, 1.6f);
        }
        // 机械臂
        for (int k = 0; k < 4; ++k) {
            float a = -t * 0.6f + k * 1.570796f;
            float r1 = rad * 1.15f, r2 = rad * 1.85f;
            float x1 = sx + std::cos(a) * r1, y1 = sy + std::sin(a) * r1;
            float x2 = sx + std::cos(a + 0.25f) * r2, y2 = sy + std::sin(a + 0.25f) * r2;
            r.glowLine(x1, y1, x2, y2, Palette::blue, 1.5f);
            r.glowCircle(x2, y2, 5 * scale, Palette::cyan, 2.0f);
        }
        // 核心
        r.glowRing(sx, sy, rad * 0.95f, 4, Palette::pink, 2.6f);
        r.glowRing(sx, sy, rad * 0.62f, 2, Palette::purple, 2.2f);
        float pulse = 0.75f + 0.25f * std::sin(t * 4.0f);
        r.glowCircle(sx, sy, rad * 0.40f * pulse,
                     e.flash > 0 ? Palette::white : Palette::red, 2.8f);
        r.line(sx - 10 * scale, sy, sx + 10 * scale, sy, color(255, 220, 230, 160), 2);
        r.line(sx, sy - 10 * scale, sx, sy + 10 * scale, color(255, 220, 230, 160), 2);
        break;
    }
    }

    // 血条（非 Boss）
    if (e.type != EnemyType::Boss && e.hp < e.maxHp) {
        float bw = std::max(24.0f, rad * 2.2f);
        int x = int(sx - bw / 2), y = int(sy - rad - 12);
        r.fillRect(x, y, int(bw), 4, color(10, 8, 20, 220));
        r.fillRect(x, y, int(bw * std::max(0.0f, e.hp / e.maxHp)), 4,
                   e.type == EnemyType::Elite ? Palette::orange : Palette::pink);
    }
}

// ---------- 子弹：带拖尾 ----------
void drawBullet(Renderer& r, const Bullet& b, float camX, float camY) {
    float sx = b.pos.x - camX, sy = b.pos.y - camY;
    if (sx < -40 || sx > r.width() + 40 || sy < -40 || sy > r.height() + 40) return;
    size_t n = b.trail.size();
    for (size_t i = 1; i < n; ++i) {
        float a = float(i) / n;
        Color c{b.color.r, b.color.g, b.color.b, uint8_t(30 + a * 110)};
        r.lineAdd(b.trail[i - 1].x - camX, b.trail[i - 1].y - camY,
                  b.trail[i].x - camX, b.trail[i].y - camY, c, a * b.radius * 1.1f);
    }
    r.circleAdd(sx, sy, b.radius * 2.6f,
                Color{b.color.r, b.color.g, b.color.b, uint8_t(b.enemy ? 45 : 35)}, true);
    r.glowCircle(sx, sy, b.radius, b.color, 2.0f);
    if (b.enemy) r.ring(sx, sy, b.radius + 2, 1, color(255, 180, 120, 150));
}

// ---------- 粒子 ----------
void drawParticle(Renderer& r, const Particle& p, float camX, float camY) {
    float sx = p.pos.x - camX, sy = p.pos.y - camY;
    float t = p.life / std::max(0.001f, p.maxLife);
    if (sx < -30 || sx > r.width() + 30 || sy < -30 || sy > r.height() + 30) return;
    Color c = p.color;
    c.a = uint8_t(c.a * t);
    switch (p.kind) {
    case 0: { // 火花：沿速度方向拉长
        float len = std::min(14.0f, std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y) * 0.045f);
        float inv = 1.0f / std::max(0.001f, std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y));
        r.lineAdd(sx, sy, sx - p.vel.x * inv * len, sy - p.vel.y * inv * len, c, p.size * t);
        break;
    }
    case 1: // 烟雾
        r.circle(sx, sy, p.size * (2.0f - t), Color{c.r, c.g, c.b, uint8_t(c.a * 0.35f)}, true);
        break;
    case 2: // 冲击环
        r.circleAdd(sx, sy, p.size, c, false);
        r.circleAdd(sx, sy, p.size - 2, Color{c.r, c.g, c.b, uint8_t(c.a * 0.5f)}, false);
        break;
    case 3: { // 碎片
        Vec2 pts[3];
        for (int i = 0; i < 3; ++i) {
            float a = p.spin + i * 2.094395f;
            pts[i] = {sx + std::cos(a) * p.size * 0.6f, sy + std::sin(a) * p.size * 0.6f};
        }
        r.polygon(pts, 3, c);
        break;
    }
    }
}

// ---------- 掉落物 ----------
void drawPickup(Renderer& r, const Pickup& p, float camX, float camY, float time) {
    float sx = p.pos.x - camX, sy = p.pos.y - camY;
    if (sx < -30 || sx > r.width() + 30 || sy < -30 || sy > r.height() + 30) return;
    float bob = std::sin(time * 4 + p.phase) * 2.5f;
    sy += bob;
    float a = time * 2.2f + p.phase;
    if (p.kind == 0) {
        Vec2 pts[4] = {{sx, sy - 5}, {sx + 4, sy}, {sx, sy + 5}, {sx - 4, sy}};
        r.polygon(pts, 4, color(20, 70, 100, 230));
        for (int i = 0; i < 4; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 4].x, pts[(i + 1) % 4].y, Palette::cyan, 0.8f);
        r.glowCircle(sx, sy, 1.6f, Palette::white, 1.4f);
    } else if (p.kind == 1) {
        float c = std::cos(a), s = std::sin(a);
        Vec2 pts[4];
        Vec2 l[4] = {{-4, -4}, {4, -4}, {4, 4}, {-4, 4}};
        for (int i = 0; i < 4; ++i) pts[i] = {sx + l[i].x * c - l[i].y * s, sy + l[i].x * s + l[i].y * c};
        r.polygon(pts, 4, color(90, 70, 10, 240));
        for (int i = 0; i < 4; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 4].x, pts[(i + 1) % 4].y, Palette::yellow, 0.8f);
    } else {
        Vec2 pts[6];
        for (int i = 0; i < 6; ++i) {
            float ang = a + i * 1.047197f;
            pts[i] = {sx + std::cos(ang) * 6, sy + std::sin(ang) * 6};
        }
        r.polygon(pts, 6, color(70, 18, 60, 240));
        for (int i = 0; i < 6; ++i)
            r.glowLine(pts[i].x, pts[i].y, pts[(i + 1) % 6].x, pts[(i + 1) % 6].y, Palette::pink, 0.9f);
        r.glowCircle(sx, sy, 2.0f, Palette::white, 1.5f);
    }
}

} // namespace nz
