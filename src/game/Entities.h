#pragma once
// ============================================================
// Entities.h - 游戏实体数据结构与绘制函数声明
// 逻辑更新在 Game.cpp，绘制在本文件的 .cpp 中
// ============================================================
#include "../core/Renderer.h"
#include "../core/Util.h"
#include <vector>

namespace nz {

enum class EnemyType { Grunt, Chaser, Turret, Elite, Boss };

struct Player {
    Vec2 pos{0, 0}, vel{0, 0};
    float hp = 100, maxHp = 100;
    float shield = 50, maxShield = 50;
    float angle = 0;
    float fireCd = 0, invuln = 0, flash = 0, regenDelay = 0;
    float walkPhase = 0;
    int level = 1, xp = 0, xpNeed = 30, gold = 0, kills = 0;
    float damage = 14, fireInterval = 0.20f, speed = 250;
    float bulletSpeed = 720, pierce = 0, blast = 0;
    float regen = 0, lifesteal = 0;
    int multishot = 1;
    bool alive = true;
};

struct Enemy {
    EnemyType type = EnemyType::Grunt;
    Vec2 pos{0, 0}, vel{0, 0};
    float hp = 30, maxHp = 30, radius = 16, speed = 90;
    float attackCd = 0, flash = 0, angle = 0, phase = 0;
    float spawnT = 0;           // 0..1 传送登场动画
    float stun = 0, auxCd = 0, summonCd = 8;
    int xp = 5, gold = 3;
    bool dead = false;
};

struct Bullet {
    Vec2 pos{0, 0}, vel{0, 0};
    float damage = 10, radius = 4, life = 2.5f;
    int pierce = 0;
    float blast = 0;
    bool enemy = false;
    Color color = Palette::cyan;
    std::vector<Vec2> trail;
};

struct Particle {
    Vec2 pos{0, 0}, vel{0, 0};
    float life = 0.5f, maxLife = 0.5f, size = 3;
    Color color = Palette::cyan;
    int kind = 0;   // 0火花 1烟雾 2冲击环 3碎片
    float spin = 0;
};

struct FloatText {
    Vec2 pos{0, 0};
    float life = 0.8f, maxLife = 0.8f;
    wchar_t text[32]{};
    Color color = Palette::white;
};

struct Pickup {
    Vec2 pos{0, 0}, vel{0, 0};
    int kind = 0;   // 0经验 1金币 2能量
    float value = 1;
    float life = 18, phase = 0;
};

struct Obstacle {
    Vec2 pos{0, 0};     // 中心
    float w = 60, h = 60;
    int variant = 0;
};

// ---------- 绘制 ----------
void drawObstacle(Renderer& r, const Obstacle& o, float camX, float camY, float time);
void drawPlayer(Renderer& r, const Player& p, float camX, float camY, float time);
void drawEnemy(Renderer& r, const Enemy& e, float camX, float camY, float time);
void drawBullet(Renderer& r, const Bullet& b, float camX, float camY);
void drawParticle(Renderer& r, const Particle& p, float camX, float camY);
void drawPickup(Renderer& r, const Pickup& p, float camX, float camY, float time);

} // namespace nz
