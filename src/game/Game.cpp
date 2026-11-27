// ============================================================
// Game.cpp - 主状态机、战斗系统、UI 实现
// ============================================================
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace nz {

// 可用编译宏缩短 Boss 时间做自动化测试：-DNZ_BOSS_TIME=12.f -DNZ_BOSS_HP=120.f
#ifndef NZ_BOSS_TIME
#define NZ_BOSS_TIME 180.0f
#endif
#ifndef NZ_BOSS_HP
#define NZ_BOSS_HP 4600.0f
#endif
const float Game::BOSS_TIME = NZ_BOSS_TIME;

const Game::Skill Game::skills_[] = {
    {0,  L"高爆弹头",   L"子弹伤害 +20%"},
    {1,  L"超频扳机",   L"射击速度 +14%"},
    {2,  L"疾行协议",   L"移动速度 +12%"},
    {3,  L"纳米修复",   L"恢复生命 +40，上限 +10"},
    {4,  L"穿甲涂层",   L"子弹穿透 +1 个目标"},
    {5,  L"范围爆破",   L"命中产生爆炸 +26 范围"},
    {6,  L"护盾扩容",   L"护盾上限 +25 并充满"},
    {7,  L"磁轨加速",   L"弹速 +180，伤害 +4"},
    {8,  L"生体汲取",   L"造成伤害的 4% 转化为生命"},
    {9,  L"自愈回路",   L"每秒恢复 1.6 点生命"},
    {10, L"多重射击",   L"每次额外发射 1 枚子弹"},
    {11, L"应急充能",   L"护盾立即充满，短时强化"}
};

Game::Game(Renderer& renderer, HWND hwnd) : r_(renderer), hwnd_(hwnd) {
    Audio::instance().setMusicVolume(musicVol_);
    Audio::instance().setSfxVolume(sfxVol_);
}

// ============================================================
// 工具 UI
// ============================================================
void Game::addText(int x, int y, int size, const wchar_t* s, Color c,
                   int align, int weight, bool shadow) {
    textQueue_.push_back(TextItem{x, y, size, weight, align, std::wstring(s), c, shadow});
}

void Game::flushText() {
    for (const TextItem& t : textQueue_) {
        r_.setFont(t.size, t.weight);
        if (t.shadow) r_.textShadow(t.x, t.y, t.str.c_str(), t.color, t.align);
        else r_.text(t.x, t.y, t.str.c_str(), t.color, t.align);
    }
    textQueue_.clear();
}

void Game::panel(int x, int y, int w, int h, Color border, Color fill) {
    r_.fillRect(x, y, w, h, fill);
    r_.strokeRect(x, y, w, h, border, 2);
    int c = 14;
    r_.line(x, y, x + c, y, Palette::white, 2);
    r_.line(x, y, x, y + c, Palette::white, 2);
    r_.line(x + w - c, y + h, x + w, y + h, Palette::white, 2);
    r_.line(x + w, y + h - c, x + w, y + h, Palette::white, 2);
    r_.line(x + 6, y + h - 1, x + w - 6, y + h - 1, Color{border.r, border.g, border.b, 60}, 1);
}

void Game::bar(int x, int y, int w, int h, float ratio, Color fill, Color back) {
    ratio = clampf(ratio, 0.0f, 1.0f);
    r_.fillRect(x, y, w, h, color(6, 8, 20, 240));
    r_.fillRect(x, y, int(w * ratio), h, back);
    r_.fillRect(x, y, int(w * ratio), std::max(2, h / 3), Color{fill.r, fill.g, fill.b, 90});
    r_.strokeRect(x, y, w, h, Color{fill.r, fill.g, fill.b, 170}, 1);
}

bool Game::button(int x, int y, int w, int h, const wchar_t* label, Color accent, int fontSize) {
    int mx = Input::mx(), my = Input::my();
    bool hover = mx >= x && mx <= x + w && my >= y && my <= y + h;
    Color border = hover ? Palette::white : accent;
    r_.fillRect(x, y, w, h, hover ? Color{accent.r, accent.g, accent.b, 70} : color(12, 14, 36, 220));
    r_.strokeRect(x, y, w, h, border, hover ? 2 : 1);
    r_.fillRect(x, y, hover ? w : w / 3, 2, border);
    if (hover) {
        r_.glowCircle(x + 12, y + h / 2, 3, accent, 1.8f);
        r_.glowCircle(x + w - 12, y + h / 2, 3, accent, 1.8f);
    }
    addText(x + w / 2, y + (h - fontSize) / 2 - 2, fontSize, label,
            hover ? Palette::white : Color{accent.r, accent.g, accent.b, 240}, 1,
            hover ? FW_BOLD : FW_NORMAL);
    return hover && Input::mousePressed(0);
}

void Game::neonTitle(int x, int y, int size, const wchar_t* s, Color c) {
    addText(x + 4, y + 4, size, s, color(90, 20, 110, 160), 1, FW_BOLD);
    addText(x - 3, y - 2, size, s, color(30, 90, 160, 150), 1, FW_BOLD);
    addText(x, y, size, s, c, 1, FW_BOLD, true);
}

// ============================================================
// 坐标 / 碰撞
// ============================================================
Vec2 Game::screenToWorld(int sx, int sy) const {
    return Vec2{float(sx) + cam_.x, float(sy) + cam_.y};
}

Vec2 Game::clampWorld(Vec2 p) const {
    p.x = clampf(p.x, 20.0f, float(WORLD_W - 20));
    p.y = clampf(p.y, 20.0f, float(WORLD_H - 20));
    return p;
}

bool Game::hitObstacle(Vec2 p, float radius) const {
    for (const Obstacle& o : obstacles_) {
        float hw = o.w * 0.5f, hh = o.h * 0.5f;
        float cx = clampf(p.x, o.pos.x - hw, o.pos.x + hw);
        float cy = clampf(p.y, o.pos.y - hh, o.pos.y + hh);
        if (dist2(p.x, p.y, cx, cy) < radius * radius) return true;
    }
    return false;
}

void Game::pushOutOfObstacles(Vec2& p, float radius) const {
    for (const Obstacle& o : obstacles_) {
        float hw = o.w * 0.5f, hh = o.h * 0.5f;
        float cx = clampf(p.x, o.pos.x - hw, o.pos.x + hw);
        float cy = clampf(p.y, o.pos.y - hh, o.pos.y + hh);
        float dx = p.x - cx, dy = p.y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 >= radius * radius) continue;
        if (d2 > 0.01f) {
            float d = std::sqrt(d2);
            p.x += dx / d * (radius - d);
            p.y += dy / d * (radius - d);
        } else {
            // 圆心恰好在矩形内：推向最近边
            float left = p.x - (o.pos.x - hw), right = (o.pos.x + hw) - p.x;
            float top = p.y - (o.pos.y - hh), bottom = (o.pos.y + hh) - p.y;
            float m = std::min(std::min(left, right), std::min(top, bottom));
            if (m == left) p.x = o.pos.x - hw - radius;
            else if (m == right) p.x = o.pos.x + hw + radius;
            else if (m == top) p.y = o.pos.y - hh - radius;
            else p.y = o.pos.y + hh + radius;
        }
    }
    p = clampWorld(p);
}

// ============================================================
// 开局重置
// ============================================================
void Game::resetGame() {
    enemies_.clear(); bullets_.clear(); particles_.clear();
    pickups_.clear(); texts_.clear(); textQueue_.clear();
    obstacles_.clear();
    enemies_.reserve(256); bullets_.reserve(512); particles_.reserve(1400);
    pickups_.reserve(512); texts_.reserve(256);

    player_ = Player{};
    player_.pos = Vec2{float(WORLD_W / 2), float(WORLD_H / 2)};
    cam_ = Vec2{player_.pos.x - r_.width() / 2.0f, player_.pos.y - r_.height() / 2.0f};
    gameTime_ = 0; spawnTimer_ = 2.0f; eliteTimer_ = 42.0f;
    bossSpawned_ = false; bossWarnShown_ = -1;
    bannerTimer_ = 0; shake_ = 0; flash_ = 0;
    dragSlider_ = -1;

    // 场景障碍：避开出生点，保证有掩体但不堵死
    for (int i = 0; i < 16; ++i) {
        Obstacle o;
        o.w = float(rint(68, 150));
        o.h = float(rint(68, 150));
        for (int tries = 0; tries < 30; ++tries) {
            o.pos = Vec2{rrange(160.0f, float(WORLD_W - 160)), rrange(160.0f, float(WORLD_H - 160))};
            if (distance(o.pos.x, o.pos.y, player_.pos.x, player_.pos.y) > 230) break;
        }
        o.variant = i % 3;
        obstacles_.push_back(o);
    }
    // 固定两处大型掩体，让场景更有结构感
    obstacles_.push_back(Obstacle{Vec2{float(WORLD_W / 2 - 430), float(WORLD_H / 2 - 300)}, 190, 70, 1});
    obstacles_.push_back(Obstacle{Vec2{float(WORLD_W / 2 + 430), float(WORLD_H / 2 + 300)}, 190, 70, 2});

    state_ = State::Playing;
    bannerText_ = L"任务开始 // 清除机械病毒";
    bannerColor_ = Palette::cyan;
    bannerTimer_ = 2.4f;
    Audio::instance().play(Sfx::Energy, 0.7f);
}

// ============================================================
// 生成
// ============================================================
void Game::spawnEnemy(EnemyType type, float x, float y) {
    Enemy e;
    e.type = type;
    e.pos = clampWorld(Vec2{x, y});
    e.spawnT = 0;
    e.phase = rrange(0.0f, 6.28f);
    switch (type) {
    case EnemyType::Grunt:
        e.hp = e.maxHp = 34 + gameTime_ * 0.12f; e.radius = 16; e.speed = rrange(78, 104);
        e.xp = 5; e.gold = rint(2, 5); break;
    case EnemyType::Chaser:
        e.hp = e.maxHp = 22 + gameTime_ * 0.08f; e.radius = 13; e.speed = rrange(165, 205);
        e.xp = 7; e.gold = rint(3, 6); break;
    case EnemyType::Turret:
        e.hp = e.maxHp = 55 + gameTime_ * 0.16f; e.radius = 19; e.speed = 52;
        e.attackCd = rrange(0.8f, 1.8f); e.xp = 11; e.gold = rint(6, 10); break;
    case EnemyType::Elite:
        e.hp = e.maxHp = 260 + gameTime_ * 0.45f; e.radius = 27; e.speed = 118;
        e.xp = 40; e.gold = rint(22, 35); break;
    case EnemyType::Boss:
        e.hp = e.maxHp = NZ_BOSS_HP; e.radius = 60; e.speed = 72;
        e.xp = 0; e.gold = 0; e.spawnT = 0.15f;
        break;
    }
    e.angle = angleTo(e.pos.x, e.pos.y, player_.pos.x, player_.pos.y);
    enemies_.push_back(e);
}

void Game::spawnBullet(Vec2 pos, Vec2 dir, float speed, float dmg, bool enemy,
                       int pierce, float blast, Color c) {
    float len = std::max(0.001f, std::sqrt(dir.x * dir.x + dir.y * dir.y));
    Bullet b;
    b.pos = pos;
    b.vel = Vec2{dir.x / len * speed, dir.y / len * speed};
    b.damage = dmg; b.enemy = enemy; b.pierce = pierce; b.blast = blast; b.color = c;
    b.radius = enemy ? 6.0f : (blast > 0 ? 5.5f : 4.0f);
    b.life = enemy ? 4.0f : 2.0f;
    b.trail.push_back(pos);
    bullets_.push_back(b);
}

void Game::spawnParticles(Vec2 pos, Color c, int count, float speed, int kind, float size) {
    if (!particlesOn_ && kind != 2) count = count / 3;
    for (int i = 0; i < count; ++i) {
        if (particles_.size() > 1300) particles_.erase(particles_.begin());
        Particle p;
        float a = rrange(0.0f, 6.2831853f);
        float s = rrange(speed * 0.25f, speed);
        p.pos = pos;
        p.vel = Vec2{std::cos(a) * s, std::sin(a) * s};
        p.maxLife = p.life = rrange(0.25f, 0.75f);
        p.size = size * rrange(0.6f, 1.4f);
        p.color = c; p.kind = kind;
        p.spin = rrange(0.0f, 6.28f);
        if (kind == 2) { p.vel = Vec2{0, 0}; p.maxLife = p.life = 0.45f; }
        particles_.push_back(p);
    }
}

void Game::spawnDeathFX(const Enemy& e) {
    Color c = e.type == EnemyType::Elite ? Palette::orange :
              (e.type == EnemyType::Chaser ? Palette::pink : Palette::purple);
    spawnParticles(e.pos, c, e.type == EnemyType::Boss ? 90 : 14,
                   e.type == EnemyType::Boss ? 330 : 190, 0, 3.2f);
    spawnParticles(e.pos, c, 6, 70, 1, 5.0f);
    spawnParticles(e.pos, c, e.type == EnemyType::Boss ? 4 : 2, 0, 2,
                   e.type == EnemyType::Boss ? 140 : e.radius * 1.6f);
    if (e.type == EnemyType::Elite || e.type == EnemyType::Boss) {
        Audio::instance().play(Sfx::Explosion, e.type == EnemyType::Boss ? 1.0f : 0.55f);
        shake_ = std::max(shake_, e.type == EnemyType::Boss ? 20.0f : 8.0f);
    }
}

void Game::explode(Vec2 pos, float radius, float damage) {
    spawnParticles(pos, Palette::orange, 22, 260, 0, 3.5f);
    spawnParticles(pos, Palette::yellow, 2, 0, 2, radius);
    shake_ = std::max(shake_, 6.0f);
    Audio::instance().play(Sfx::Explosion, 0.45f);
    for (Enemy& e : enemies_) {
        if (e.dead || e.spawnT < 0.6f) continue;
        float d = distance(e.pos.x, e.pos.y, pos.x, pos.y);
        if (d < radius + e.radius) damageEnemy(e, damage * (1.0f - 0.4f * d / (radius + e.radius)), pos);
    }
}

// ============================================================
// 伤害与成长
// ============================================================
void Game::damageEnemy(Enemy& e, float dmg, Vec2 from) {
    if (e.dead || dmg <= 0) return;
    e.hp -= dmg;
    e.flash = 0.14f;
    float dx = e.pos.x - from.x, dy = e.pos.y - from.y;
    float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    float knock = e.type == EnemyType::Boss ? 0.0f : (e.type == EnemyType::Elite ? 25 : 90);
    e.vel.x += dx / len * knock; e.vel.y += dy / len * knock;

    wchar_t buf[32];
    swprintf(buf, 32, L"-%d", int(dmg));
    FloatText ft;
    ft.pos = e.pos; ft.life = ft.maxLife = 0.65f;
    wcsncpy(ft.text, buf, 31); ft.text[31] = 0;
    ft.color = Palette::yellow;
    texts_.push_back(ft);
    spawnParticles(e.pos, Palette::white, 3, 130, 0, 2.0f);
    if (randf() < 0.4f) Audio::instance().play(Sfx::Hit, 0.35f);

    if (player_.lifesteal > 0)
        player_.hp = std::min(player_.maxHp, player_.hp + dmg * player_.lifesteal);

    if (e.hp <= 0) {
        e.dead = true;
        player_.kills++;
        spawnDeathFX(e);
        if (e.type != EnemyType::Boss) {
            pickups_.push_back(Pickup{e.pos, Vec2{rrange(-60, 60), rrange(-60, 60)}, 0, float(e.xp), 20, rrange(0, 6.28f)});
            pickups_.push_back(Pickup{e.pos, Vec2{rrange(-70, 70), rrange(-70, 70)}, 1, float(e.gold), 20, rrange(0, 6.28f)});
            if (e.type == EnemyType::Elite || randf() < 0.08f)
                pickups_.push_back(Pickup{e.pos, Vec2{rrange(-60, 60), rrange(-60, 60)}, 2, 1, 20, rrange(0, 6.28f)});
        }
    }
}

void Game::damagePlayer(float dmg) {
#ifdef NZ_GODMODE   // 仅用于自动化测试：观察 Boss 流程
    (void)dmg;
    return;
#endif
    Player& p = player_;
    if (!p.alive || p.invuln > 0) return;
    float left = dmg;
    if (p.shield > 0) {
        float absorbed = std::min(p.shield, left);
        p.shield -= absorbed; left -= absorbed;
    }
    p.hp -= left;
    p.invuln = 0.48f; p.flash = 0.22f; p.regenDelay = 4.0f;
    shake_ = std::max(shake_, 9.0f);
    flash_ = 0.35f; flashColor_ = Palette::red;
    Audio::instance().play(Sfx::Hurt, 0.8f);
    spawnParticles(p.pos, Palette::cyan, 10, 170, 0, 2.6f);
    if (p.hp <= 0) {
        p.hp = 0; p.alive = false;
        state_ = State::GameOver;
        Audio::instance().play(Sfx::Defeat, 1.0f);
        shake_ = 16.0f;
    }
}

void Game::grantXP(int amount) {
    player_.xp += amount;
    checkLevelUp();
}

void Game::checkLevelUp() {
    if (player_.xp < player_.xpNeed) return;
    player_.xp -= player_.xpNeed;
    player_.level++;
    player_.xpNeed = int(28.0f * std::pow(1.27f, float(player_.level - 1)));
    newChoices();
    state_ = State::LevelUp;
    Audio::instance().play(Sfx::LevelUp, 0.9f);
    spawnParticles(player_.pos, Palette::cyan, 36, 230, 0, 3.0f);
    spawnParticles(player_.pos, Palette::cyan, 2, 0, 2, 90);
}

void Game::newChoices() {
    int pool[12];
    for (int i = 0; i < 12; ++i) pool[i] = i;
    for (int i = 11; i > 0; --i) {
        int j = rint(0, i);
        std::swap(pool[i], pool[j]);
    }
    for (int i = 0; i < 3; ++i) choices_[i] = pool[i];
}

void Game::applySkill(int id) {
    Player& p = player_;
    switch (id) {
    case 0: p.damage *= 1.20f; break;
    case 1: p.fireInterval = std::max(0.055f, p.fireInterval * 0.86f); break;
    case 2: p.speed = std::min(480.0f, p.speed * 1.12f); break;
    case 3: p.maxHp += 10; p.hp = std::min(p.maxHp, p.hp + 40); break;
    case 4: p.pierce += 1; break;
    case 5: p.blast += 26; break;
    case 6: p.maxShield += 25; p.shield = p.maxShield; break;
    case 7: p.bulletSpeed += 180; p.damage += 4; break;
    case 8: p.lifesteal += 0.04f; break;
    case 9: p.regen += 1.6f; break;
    case 10: p.multishot = std::min(5, p.multishot + 1); break;
    case 11: p.shield = p.maxShield; p.fireInterval = std::max(0.055f, p.fireInterval * 0.9f); break;
    }
    Audio::instance().play(Sfx::Click, 0.9f);
    spawnParticles(p.pos, Palette::green, 24, 200, 0, 2.8f);
    state_ = State::Playing;
    checkLevelUp();   // 连续升级
}

// ============================================================
// 更新系统
// ============================================================
void Game::updatePlayer(float dt) {
    Player& p = player_;
    p.fireCd = std::max(0.0f, p.fireCd - dt);
    p.invuln = std::max(0.0f, p.invuln - dt);
    p.flash = std::max(0.0f, p.flash - dt);
    p.regenDelay = std::max(0.0f, p.regenDelay - dt);

    int dx = (Input::down('D') || Input::down(VK_RIGHT) ? 1 : 0) -
             (Input::down('A') || Input::down(VK_LEFT) ? 1 : 0);
    int dy = (Input::down('S') || Input::down(VK_DOWN) ? 1 : 0) -
             (Input::down('W') || Input::down(VK_UP) ? 1 : 0);
    float len = std::max(1.0f, std::sqrt(float(dx * dx + dy * dy)));
    float targetX = dx / len * p.speed, targetY = dy / len * p.speed;
    float k = std::min(1.0f, dt * (dx || dy ? 13.0f : 18.0f));
    p.vel.x = lerpf(p.vel.x, targetX, k);
    p.vel.y = lerpf(p.vel.y, targetY, k);
    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;
    pushOutOfObstacles(p.pos, 16);

    float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
    if (speed > 12) p.walkPhase += dt * (5.0f + speed * 0.045f);

    Vec2 mouse = screenToWorld(Input::mx(), Input::my());
    p.angle = angleTo(p.pos.x, p.pos.y, mouse.x, mouse.y);

    if (Input::mouseDown(0) && p.fireCd <= 0) {
        p.fireCd = p.fireInterval;
        int n = p.multishot;
        for (int i = 0; i < n; ++i) {
            float off = (i - (n - 1) * 0.5f) * 0.135f;
            float a = p.angle + off;
            Vec2 dir{std::cos(a), std::sin(a)};
            Vec2 muzzle{p.pos.x + dir.x * 28, p.pos.y + dir.y * 28};
            spawnBullet(muzzle, dir, p.bulletSpeed, p.damage * (n > 1 ? 0.82f : 1.0f),
                        false, p.pierce, p.blast, p.blast > 0 ? Palette::orange : Palette::cyan);
        }
        spawnParticles(Vec2{p.pos.x + std::cos(p.angle) * 30, p.pos.y + std::sin(p.angle) * 30},
                       Palette::cyan, 3, 110, 0, 1.8f);
        shake_ = std::max(shake_, 1.6f);
        Audio::instance().play(Sfx::Shoot, 0.55f);
    }

    if (p.regenDelay <= 0 && p.regen > 0)
        p.hp = std::min(p.maxHp, p.hp + p.regen * dt);
    if (p.regenDelay <= 0 && p.shield < p.maxShield)
        p.shield = std::min(p.maxShield, p.shield + 11.0f * dt);
}

void Game::updateSpawner(float dt) {
    if (bossSpawned_) return;
    if (gameTime_ > BOSS_TIME - 18 && bossWarnShown_ < 0) {
        bossWarnShown_ = 0;
        bannerText_ = L"警告 // 机械核心信号接近";
        bannerColor_ = Palette::red;
        bannerTimer_ = 3.0f;
        Audio::instance().play(Sfx::BossAppear, 0.45f);
    }
    if (gameTime_ >= BOSS_TIME) {
        bossSpawned_ = true;
        float a = rrange(0.0f, 6.2831853f);
        spawnEnemy(EnemyType::Boss,
                   player_.pos.x + std::cos(a) * 420,
                   player_.pos.y + std::sin(a) * 420);
        bannerText_ = L"机械核心已降临 // 最终决战";
        bannerColor_ = Palette::pink;
        bannerTimer_ = 3.6f;
        shake_ = 24.0f;
        flash_ = 0.5f; flashColor_ = Palette::pink;
        Audio::instance().play(Sfx::BossAppear, 1.0f);
        return;
    }

    eliteTimer_ -= dt;
    if (eliteTimer_ <= 0) {
        eliteTimer_ = rrange(40.0f, 52.0f);
        float a = angleTo(player_.pos.x, player_.pos.y, cam_.x + r_.width(), cam_.y + r_.height() / 2.0f);
        spawnEnemy(EnemyType::Elite, player_.pos.x + std::cos(a) * 430, player_.pos.y + std::sin(a) * 430);
        bannerText_ = L"精英单位侦测到 // 保持移动";
        bannerColor_ = Palette::orange;
        bannerTimer_ = 2.2f;
    }

    spawnTimer_ -= dt;
    if (spawnTimer_ > 0) return;
    float interval = std::max(0.42f, 2.1f - gameTime_ * 0.0105f);
    spawnTimer_ = interval;
    size_t cap = size_t(std::min(72, 34 + int(gameTime_ / 8)));
    if (enemies_.size() >= cap) return;

    int count = 1;
    if (gameTime_ > 55) count = 2;
    if (gameTime_ > 120) count = 3;
    for (int i = 0; i < count && enemies_.size() < cap; ++i) {
        float roll = randf();
        EnemyType type;
        if (gameTime_ < 40) type = roll < 0.84f ? EnemyType::Grunt : EnemyType::Chaser;
        else if (gameTime_ < 90) type = roll < 0.52f ? EnemyType::Grunt :
                                          (roll < 0.84f ? EnemyType::Chaser : EnemyType::Turret);
        else type = roll < 0.38f ? EnemyType::Grunt :
                    (roll < 0.66f ? EnemyType::Chaser :
                     (roll < 0.90f ? EnemyType::Turret : EnemyType::Elite));
        float a = rrange(0.0f, 6.2831853f);
        float d = rrange(430.0f, 520.0f);
        Vec2 pos{player_.pos.x + std::cos(a) * d, player_.pos.y + std::sin(a) * d};
        for (int t = 0; t < 8 && hitObstacle(pos, 40); ++t)
            pos = Vec2{player_.pos.x + std::cos(a += 0.5f) * d, player_.pos.y + std::sin(a) * d};
        spawnEnemy(type, pos.x, pos.y);
    }
}

void Game::updateEnemies(float dt) {
    Player& p = player_;
    // Boss 召唤延后到遍历结束后统一生成：
    // 遍历中持有 Enemy& ，直接 push_back 可能因 vector 扩容使引用悬空。
    struct SummonReq { EnemyType type; float x, y; };
    std::vector<SummonReq> pendingSummons;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& e = enemies_[i];
        if (e.dead) continue;
        e.spawnT = std::min(1.0f, e.spawnT + dt * 1.5f);
        e.flash = std::max(0.0f, e.flash - dt * 7);
        e.phase += dt * (e.type == EnemyType::Chaser ? 9.0f : 4.0f);
        e.stun = std::max(0.0f, e.stun - dt);
        e.attackCd = std::max(0.0f, e.attackCd - dt);
        e.auxCd = std::max(0.0f, e.auxCd - dt);
        if (e.spawnT < 1.0f) continue;

        float dx = p.pos.x - e.pos.x, dy = p.pos.y - e.pos.y;
        float d = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        Vec2 dir{dx / d, dy / d};
        e.angle = std::atan2(dir.y, dir.x);

        if (e.type == EnemyType::Boss) {
            // 保持中距离环绕 + 三种攻击模式
            Vec2 move = d > 260 ? dir : (d < 170 ? Vec2{-dir.x, -dir.y} : Vec2{-dir.y, dir.x});
            e.vel.x = lerpf(e.vel.x, move.x * e.speed, std::min(1.0f, dt * 2.5f));
            e.vel.y = lerpf(e.vel.y, move.y * e.speed, std::min(1.0f, dt * 2.5f));

            if (e.attackCd <= 0) {
                e.attackCd = 3.4f;
                int n = 14;
                for (int k = 0; k < n; ++k) {
                    float a = 6.2831853f * k / n + e.phase * 0.3f;
                    spawnBullet(e.pos, Vec2{std::cos(a), std::sin(a)}, 245, 15, true, 0, 0, Palette::pink);
                }
                spawnParticles(e.pos, Palette::pink, 2, 0, 2, 120);
                Audio::instance().play(Sfx::EnemyShoot, 0.8f);
                shake_ = std::max(shake_, 5.0f);
            }
            if (e.auxCd <= 0) {
                e.auxCd = 1.7f;
                for (int k = -2; k <= 2; ++k) {
                    float a = e.angle + k * 0.16f;
                    spawnBullet(e.pos, Vec2{std::cos(a), std::sin(a)}, 330, 12, true, 0, 0, Palette::red);
                }
            }
            e.summonCd -= dt;
            if (e.summonCd <= 0) {
                e.summonCd = 9.0f;
                for (int k = 0; k < 3; ++k) {
                    float a = rrange(0.0f, 6.28f);
                    // 只记录请求，遍历结束后再生成，避免 e 引用失效
                    pendingSummons.push_back({k == 0 ? EnemyType::Chaser : EnemyType::Grunt,
                                              e.pos.x + std::cos(a) * 70,
                                              e.pos.y + std::sin(a) * 70});
                }
                bannerText_ = L"核心正在投放增援";
                bannerColor_ = Palette::orange;
                bannerTimer_ = 1.6f;
            }
        } else {
            Vec2 want = dir;
            if (e.type == EnemyType::Turret) {
                if (d < 270) want = Vec2{-dir.x, -dir.y};
                else if (d < 420) want = Vec2{-dir.y, dir.x};
            } else if (e.type == EnemyType::Chaser) {
                float weave = std::sin(e.phase) * 0.35f;
                want = Vec2{dir.x - dir.y * weave, dir.y + dir.x * weave};
            } else if (e.type == EnemyType::Elite && d < 150) {
                want = Vec2{-dir.y, dir.x};
            }
            float k = std::min(1.0f, dt * 4.5f);
            e.vel.x = lerpf(e.vel.x, want.x * e.speed, k);
            e.vel.y = lerpf(e.vel.y, want.y * e.speed, k);

            if (e.type == EnemyType::Turret && e.attackCd <= 0 && d < 560 && d > 60) {
                e.attackCd = 2.0f;
                spawnBullet(e.pos, dir, 315, 13, true, 0, 0, Palette::orange);
                Audio::instance().play(Sfx::EnemyShoot, 0.35f);
            }
        }

        if (e.stun > 0) { e.vel.x *= 0.3f; e.vel.y *= 0.3f; }
        e.pos.x += e.vel.x * dt;
        e.pos.y += e.vel.y * dt;
        e.vel.x *= (1.0f - std::min(0.9f, dt * 2.0f));
        e.vel.y *= (1.0f - std::min(0.9f, dt * 2.0f));
        pushOutOfObstacles(e.pos, e.radius);

        // 接触伤害
        float reach = e.radius + 16;
        if (d < reach && e.attackCd <= 0 && e.type != EnemyType::Turret) {
            e.attackCd = e.type == EnemyType::Boss ? 1.1f :
                         (e.type == EnemyType::Chaser ? 0.75f : 1.0f);
            damagePlayer(e.type == EnemyType::Boss ? 22 :
                         (e.type == EnemyType::Elite ? 18 : (e.type == EnemyType::Chaser ? 10 : 12)));
        }
    }

    // 统一生成 Boss 召唤的增援（此时不再持有 enemies_ 元素引用）
    for (const auto& s : pendingSummons) spawnEnemy(s.type, s.x, s.y);

    // 敌人互斥，避免堆叠成一坨
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& a = enemies_[i];
        if (a.dead || a.spawnT < 1.0f) continue;
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Enemy& b = enemies_[j];
            if (b.dead || b.spawnT < 1.0f) continue;
            float dx = b.pos.x - a.pos.x, dy = b.pos.y - a.pos.y;
            float minD = a.radius + b.radius;
            float d2 = dx * dx + dy * dy;
            if (d2 >= minD * minD || d2 < 0.01f) continue;
            float dd = std::sqrt(d2), push = (minD - dd) * 0.5f;
            float nx = dx / dd, ny = dy / dd;
            float wa = a.type == EnemyType::Boss ? 0.1f : 1.0f;
            float wb = b.type == EnemyType::Boss ? 0.1f : 1.0f;
            a.pos.x -= nx * push * wa; a.pos.y -= ny * push * wa;
            b.pos.x += nx * push * wb; b.pos.y += ny * push * wb;
        }
    }
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
                                  [](const Enemy& e) { return e.dead; }), enemies_.end());
}

void Game::updateBullets(float dt) {
    for (Bullet& b : bullets_) {
        if (b.life <= 0) continue;
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.life -= dt;
        b.trail.push_back(b.pos);
        if (b.trail.size() > 8) b.trail.erase(b.trail.begin());

        if (b.pos.x < -20 || b.pos.y < -20 || b.pos.x > WORLD_W + 20 || b.pos.y > WORLD_H + 20) {
            b.life = 0; continue;
        }
        if (hitObstacle(b.pos, b.radius)) {
            spawnParticles(b.pos, b.color, 4, 100, 0, 1.6f);
            b.life = 0; continue;
        }
        if (b.enemy) {
            if (dist2(b.pos.x, b.pos.y, player_.pos.x, player_.pos.y) <
                (b.radius + 15) * (b.radius + 15)) {
                damagePlayer(b.damage);
                spawnParticles(b.pos, Palette::red, 6, 120, 0, 2.0f);
                b.life = 0;
            }
        } else {
            for (Enemy& e : enemies_) {
                if (e.dead || e.spawnT < 0.55f) continue;
                float rr = b.radius + e.radius;
                if (dist2(b.pos.x, b.pos.y, e.pos.x, e.pos.y) >= rr * rr) continue;
                damageEnemy(e, b.damage, b.pos);
                if (b.blast > 0) explode(b.pos, b.blast, b.damage * 0.65f);
                if (b.pierce > 0) { --b.pierce; }
                else b.life = 0;
                break;
            }
        }
    }
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
                                  [](const Bullet& b) { return b.life <= 0; }), bullets_.end());
}

void Game::updatePickups(float dt) {
    for (size_t i = 0; i < pickups_.size(); ++i) {
        Pickup& p = pickups_[i];
        p.life -= dt;
        p.phase += dt;
        float d = distance(p.pos.x, p.pos.y, player_.pos.x, player_.pos.y);
        if (d < 145) {
            float k = std::min(1.0f, dt * 5.0f);
            p.vel.x = lerpf(p.vel.x, (player_.pos.x - p.pos.x) / std::max(1.0f, d) * 340, k);
            p.vel.y = lerpf(p.vel.y, (player_.pos.y - p.pos.y) / std::max(1.0f, d) * 340, k);
        } else {
            p.vel.x *= (1.0f - std::min(0.9f, dt * 3.0f));
            p.vel.y *= (1.0f - std::min(0.9f, dt * 3.0f));
        }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (d < 22) {
            if (p.kind == 0) grantXP(int(p.value));
            else if (p.kind == 1) { player_.gold += int(p.value); Audio::instance().play(Sfx::Coin, 0.45f); }
            else {
                player_.shield = std::min(player_.maxShield, player_.shield + 28);
                player_.hp = std::min(player_.maxHp, player_.hp + 12);
                Audio::instance().play(Sfx::Energy, 0.7f);
            }
            p.life = 0;
        }
    }
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
                                  [](const Pickup& p) { return p.life <= 0; }), pickups_.end());
}

void Game::updateParticles(float dt) {
    for (size_t i = 0; i < particles_.size(); ++i) {
        Particle& p = particles_[i];
        p.life -= dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= (1.0f - std::min(0.9f, dt * 2.2f));
        p.vel.y *= (1.0f - std::min(0.9f, dt * 2.2f));
        p.spin += dt * 3.0f;
        if (p.kind == 2) p.size += 135 * dt;
        if (p.kind == 1) p.size += 7 * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0; }), particles_.end());
    for (size_t i = 0; i < texts_.size(); ++i) {
        texts_[i].life -= dt;
        texts_[i].pos.y -= 38 * dt;
    }
    texts_.erase(std::remove_if(texts_.begin(), texts_.end(),
                                [](const FloatText& t) { return t.life <= 0; }), texts_.end());
}

void Game::updateWorld(float dt) {
    gameTime_ += dt;
    updatePlayer(dt);
    if (state_ != State::Playing) return;   // 玩家死亡时立即停止后续系统
    updateSpawner(dt);
    updateEnemies(dt);
    updateBullets(dt);
    updatePickups(dt);
    updateParticles(dt);

    // 胜利判定
    bool bossDead = bossSpawned_;
    if (bossDead) {
        bossDead = true;
        bool alive = false;
        for (const Enemy& e : enemies_) if (e.type == EnemyType::Boss) { alive = true; break; }
        bossDead = !alive;
    }
    if (bossDead) {
        state_ = State::Victory;
        Audio::instance().play(Sfx::Victory, 1.0f);
        for (int i = 0; i < 8; ++i)
            spawnParticles(Vec2{rrange(200.0f, float(r_.width() - 200)) + cam_.x,
                                rrange(150.0f, float(r_.height() - 150)) + cam_.y},
                           i % 2 ? Palette::cyan : Palette::pink, 22, 220, 0, 3.0f);
    }

    // 镜头跟随
    float tx = player_.pos.x - r_.width() * 0.5f;
    float ty = player_.pos.y - r_.height() * 0.5f;
    cam_.x = lerpf(cam_.x, tx, std::min(1.0f, dt * 6.0f));
    cam_.y = lerpf(cam_.y, ty, std::min(1.0f, dt * 6.0f));
    cam_.x = clampf(cam_.x, 0.0f, float(WORLD_W - r_.width()));
    cam_.y = clampf(cam_.y, 0.0f, float(WORLD_H - r_.height()));
}

// ============================================================
// 主循环入口
// ============================================================
void Game::frame(float dt) {
    dt = std::min(dt, 0.033f);
    time_ += dt;
    shake_ = std::max(0.0f, shake_ - dt * 34.0f);
    flash_ = std::max(0.0f, flash_ - dt * 2.6f);
    bannerTimer_ = std::max(0.0f, bannerTimer_ - dt);

    switch (state_) {
    case State::Playing:
        if (Input::pressed(VK_ESCAPE) || Input::pressed('P')) {
            state_ = State::Paused;
            Audio::instance().play(Sfx::Click, 0.7f);
        } else updateWorld(dt);
        break;
    case State::Menu:
        if (Input::pressed(VK_RETURN)) resetGame();
        updateParticles(dt);
        break;
    case State::HowTo:
        if (Input::pressed(VK_ESCAPE)) state_ = State::Menu;
        updateParticles(dt);
        break;
    case State::Settings:
        if (Input::pressed(VK_ESCAPE)) state_ = State::Menu;
        Audio::instance().setMusicVolume(musicVol_);
        Audio::instance().setSfxVolume(sfxVol_);
        Audio::instance().setMusicEnabled(true);
        updateParticles(dt);
        break;
    case State::LevelUp:
        if (Input::pressed('1')) applySkill(choices_[0]);
        else if (Input::pressed('2')) applySkill(choices_[1]);
        else if (Input::pressed('3')) applySkill(choices_[2]);
        updateParticles(dt);
        break;
    case State::Paused:
        if (Input::pressed(VK_ESCAPE) || Input::pressed('P')) state_ = State::Playing;
        break;
    case State::GameOver:
    case State::Victory:
        updateParticles(dt);
        break;
    }
    render();
}

// ============================================================
// 绘制：背景
// ============================================================
void Game::drawBackdrop(float camX, float camY, float t) {
    int w = r_.width(), h = r_.height();
    for (int y = 0; y < h; ++y) {
        float u = float(y) / h;
        int rr = int(5 + 9 * u), gg = int(6 + 5 * u), bb = int(22 + 18 * (1.0f - u));
        r_.fillRect(0, y, w, 1, color(uint8_t(rr), uint8_t(gg), uint8_t(bb)));
    }
    // 世界网格
    int startX = int(camX / 64) * 64, startY = int(camY / 64) * 64;
    for (float gx = startX; gx < camX + w + 64; gx += 64) {
        int sx = int(gx - camX);
        bool major = (int(gx) % 256) == 0;
        r_.line(float(sx), 0, float(sx), float(h),
                major ? color(50, 110, 180, 52) : color(45, 90, 150, 22), 1);
    }
    for (float gy = startY; gy < camY + h + 64; gy += 64) {
        int sy = int(gy - camY);
        bool major = (int(gy) % 256) == 0;
        r_.line(0, float(sy), float(w), float(sy),
                major ? color(50, 110, 180, 52) : color(45, 90, 150, 22), 1);
    }
    // 地面电路装饰
    int cx0 = int(camX / 256), cy0 = int(camY / 256);
    for (int gx = cx0; gx <= cx0 + w / 256 + 1; ++gx) {
        for (int gy = cy0; gy <= cy0 + h / 256 + 1; ++gy) {
            uint32_t hash = uint32_t(gx * 374761393 + gy * 668265263);
            hash = (hash ^ (hash >> 13)) * 1274126177u;
            float wx = gx * 256.0f + float(hash % 180);
            float wy = gy * 256.0f + float((hash >> 9) % 180);
            float sx = wx - camX, sy = wy - camY;
            Color c = (hash & 1) ? color(40, 180, 230, 40) : color(160, 60, 220, 35);
            r_.fillRect(int(sx), int(sy), 18, 2, c);
            r_.fillRect(int(sx), int(sy), 2, 10, c);
            if ((hash & 3) == 0) r_.circleAdd(sx, sy, 2.0f, color(c.r, c.g, c.b, 60), true);
        }
    }
    // 流动光带
    float sweep = std::fmod(t * 105.0f, float(w) + 360.0f) - 180.0f;
    for (int k = 0; k < 3; ++k) {
        float x = sweep + k * 190.0f;
        r_.lineAdd(x, 0, x + 130, float(h), color(70, 150, 255, 2), 18.0f);
        r_.lineAdd(x, 0, x + 130, float(h), color(120, 80, 255, 2), 6.0f);
    }
    // 上下边缘霓虹线
    r_.fillRect(0, 0, w, 2, color(60, 140, 240, 60));
    r_.fillRect(0, h - 2, w, 2, color(150, 60, 220, 60));
}

void Game::drawWorld(float camX, float camY) {
    drawBackdrop(camX, camY, time_);
    for (const Obstacle& o : obstacles_) drawObstacle(r_, o, camX, camY, time_);
    for (const Pickup& p : pickups_) drawPickup(r_, p, camX, camY, time_);
    for (const Bullet& b : bullets_) drawBullet(r_, b, camX, camY);
    for (const Enemy& e : enemies_) drawEnemy(r_, e, camX, camY, time_);
    if (player_.alive) drawPlayer(r_, player_, camX, camY, time_);
    for (const Particle& p : particles_) drawParticle(r_, p, camX, camY);
    for (const FloatText& ft : texts_) {
        float a = ft.life / ft.maxLife;
        addText(int(ft.pos.x - camX), int(ft.pos.y - camY), 16, ft.text,
                Color{ft.color.r, ft.color.g, ft.color.b, uint8_t(255 * a)}, 1, FW_BOLD);
    }
}

// ============================================================
// 绘制：HUD
// ============================================================
void Game::drawHUD() {
    int w = r_.width(), h = r_.height();
    Player& p = player_;

    // 左上：生命 / 护盾
    panel(18, 16, 306, 82, Palette::blue);
    addText(32, 24, 14, L"生命 / SHIELD", Palette::dim, 0, FW_BOLD);
    bar(32, 46, 274, 15, p.hp / p.maxHp, Palette::pink, color(220, 50, 100, 210));
    bar(32, 66, 274, 12, p.shield / std::max(1.0f, p.maxShield), Palette::cyan, color(40, 180, 230, 210));
    wchar_t buf[64];
    swprintf(buf, 64, L"%d / %d", int(p.hp), int(p.maxHp));
    addText(306, 43, 13, buf, Palette::white, 2);
    swprintf(buf, 64, L"%d / %d", int(p.shield), int(p.maxShield));
    addText(306, 64, 12, buf, Palette::white, 2);

    // 中上：等级 / 经验
    panel(w / 2 - 186, 16, 372, 62, Palette::purple);
    swprintf(buf, 64, L"LV.%d", p.level);
    addText(w / 2 - 172, 24, 22, buf, Palette::yellow, 0, FW_BOLD);
    addText(w / 2 - 172, 50, 11, L"经验值", Palette::dim);
    bar(w / 2 - 112, 32, 278, 16, float(p.xp) / p.xpNeed, Palette::green, color(40, 200, 140, 200));
    swprintf(buf, 64, L"%d / %d", p.xp, p.xpNeed);
    addText(w / 2 + 166, 31, 12, buf, Palette::white, 2);

    // 右上：击杀 / 金币
    panel(w - 246, 16, 228, 82, Palette::pink);
    swprintf(buf, 64, L"击杀  %d", p.kills);
    addText(w - 232, 26, 18, buf, Palette::white, 0, FW_BOLD);
    swprintf(buf, 64, L"金币  %d", p.gold);
    addText(w - 232, 54, 18, buf, Palette::yellow, 0, FW_BOLD);
    r_.glowCircle(w - 38, 62, 6, Palette::yellow, 1.8f);

    // Boss 血条
    const Enemy* boss = nullptr;
    for (const Enemy& e : enemies_) if (e.type == EnemyType::Boss) { boss = &e; break; }
    if (boss) {
        panel(w / 2 - 370, 90, 740, 62, Palette::red, color(28, 8, 28, 225));
        addText(w / 2, 96, 17, L"机械核心 // MECHANICAL CORE", Palette::red, 1, FW_BOLD);
        bar(w / 2 - 350, 121, 700, 19, boss->hp / boss->maxHp, Palette::red, color(210, 30, 60, 200));
        swprintf(buf, 64, L"%d / %d", int(boss->hp), int(boss->maxHp));
        addText(w / 2, 120, 12, buf, Palette::white, 1);
    }

    // 左下：武器 / 技能状态
    panel(18, h - 88, 452, 70, Palette::cyan);
    addText(30, h - 82, 13, L"武器 // 脉冲步枪 MK-II", Palette::cyan, 0, FW_BOLD);
    swprintf(buf, 64, L"伤害 %.0f   射速 %.1f/s   穿透 %d",
             p.damage, 1.0f / p.fireInterval, p.pierce);
    addText(30, h - 60, 13, buf, Palette::dim);
    swprintf(buf, 64, L"爆破 %.0f   多重 %d   移速 %.0f", p.blast, p.multishot, p.speed);
    addText(30, h - 40, 13, buf, Palette::dim);
    // 技能图标
    const Color icons[] = {Palette::pink, Palette::yellow, Palette::cyan, Palette::orange, Palette::green};
    for (int i = 0; i < 5; ++i) {
        int ix = 330 + i * 26, iy = h - 64;
        r_.strokeRect(ix, iy, 20, 20, icons[i], 1);
        r_.fillRect(ix + 4, iy + 4, 12, 12, Color{icons[i].r, icons[i].g, icons[i].b, 90});
    }

    // 右下：生存时间 / Boss 倒计时
    panel(w - 250, h - 88, 232, 70, Palette::purple);
    int sec = int(gameTime_);
    swprintf(buf, 64, L"存活时间  %02d:%02d", sec / 60, sec % 60);
    addText(w - 238, h - 80, 15, buf, Palette::white, 0, FW_BOLD);
    if (!bossSpawned_) {
        int left = std::max(0, int(BOSS_TIME - gameTime_));
        swprintf(buf, 64, L"核心降临倒计时  %02d:%02d", left / 60, left % 60);
        addText(w - 238, h - 52, 13, buf, left < 30 ? Palette::red : Palette::purple);
    } else {
        addText(w - 238, h - 52, 13, L"最终阶段 // 击败机械核心", Palette::pink);
    }

    // 中央横幅
    if (bannerTimer_ > 0) {
        float a = clampf(bannerTimer_, 0.0f, 1.0f);
        r_.fillRect(0, h / 2 - 44, w, 78, color(6, 4, 18, int(150 * a)));
        r_.fillRect(0, h / 2 - 44, w, 2, Color{bannerColor_.r, bannerColor_.g, bannerColor_.b, uint8_t(200 * a)});
        r_.fillRect(0, h / 2 + 32, w, 2, Color{bannerColor_.r, bannerColor_.g, bannerColor_.b, uint8_t(200 * a)});
        neonTitle(w / 2, h / 2 - 28, 30, bannerText_.c_str(), bannerColor_);
    }
}

// ============================================================
// 绘制：菜单 / 说明 / 设置
// ============================================================
void Game::drawMenu() {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(4, 5, 16, 125));
    // 侧边霓虹柱
    for (int i = 0; i < 5; ++i) {
        float y = 60 + i * 130 + std::sin(time_ * 1.2f + i) * 12;
        r_.glowLine(28, y, 28, y + 70, i % 2 ? Palette::pink : Palette::cyan, 1.2f);
        r_.glowLine(w - 28, y, w - 28, y + 70, i % 2 ? Palette::cyan : Palette::purple, 1.2f);
    }
    neonTitle(w / 2, 108, 76, L"霓虹禁区", Palette::cyan);
    neonTitle(w / 2, 194, 28, L"最后的幸存者  /  LAST SURVIVOR", Palette::pink);
    addText(w / 2, 244, 14, L"机械病毒已吞噬城市 · 你是唯一的猎人", Palette::dim, 1);

    int bx = w / 2 - 160;
    if (button(bx, 310, 320, 54, L"开始游戏", Palette::cyan, 22)) resetGame();
    if (button(bx, 376, 320, 54, L"游戏说明", Palette::purple, 22)) {
        state_ = State::HowTo; Audio::instance().play(Sfx::Click);
    }
    if (button(bx, 442, 320, 54, L"游戏设置", Palette::pink, 22)) {
        state_ = State::Settings; Audio::instance().play(Sfx::Click);
    }
    if (button(bx, 508, 320, 54, L"退出游戏", Palette::red, 22)) {
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    }
    addText(w / 2, h - 46, 13, L"WASD 移动 · 鼠标瞄准 · 左键射击 · ESC 暂停", Palette::dim, 1);
    addText(w - 14, h - 22, 11, L"v1.0 // NEON ZONE", Palette::dim, 2);
}

void Game::drawHowTo() {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(3, 4, 14, 205));
    int px = w / 2 - 440, py = 90;
    panel(px, py, 880, 540, Palette::cyan);
    addText(w / 2, py + 20, 34, L"游戏说明 // OPERATION MANUAL", Palette::cyan, 1, FW_BOLD);
    const wchar_t* lines[] = {
        L"W A S D / 方向键      移动角色",
        L"鼠标                  控制射击方向",
        L"鼠标左键              持续射击",
        L"ESC / P               暂停游戏，可继续或重新开始",
        L"",
        L"· 击杀敌人获得经验与金币，经验满后选择强化技能。",
        L"· 蓝色能量块可恢复护盾和生命，靠近后自动拾取。",
        L"· 敌人分为机械兵、追踪怪、远程炮台和精英怪。",
        L"· 存活 3 分钟后机械核心降临，击败它即可获胜。",
        L"· 掩体可以阻挡子弹，也挡住你自己的子弹，注意走位。",
        L"",
        L"提示：让敌人聚成一堆，用穿透和爆破弹一次性清场。"
    };
    int y = py + 82;
    for (int i = 0; i < 12; ++i) {
        addText(px + 48, y, i == 0 ? 17 : 16, lines[i],
                i >= 5 && i < 10 ? Palette::white : Palette::dim, 0,
                i == 0 ? FW_BOLD : FW_NORMAL);
        y += i == 0 ? 32 : 30;
    }
    if (button(w / 2 - 110, py + 470, 220, 50, L"返回主菜单", Palette::pink, 20))
        state_ = State::Menu, Audio::instance().play(Sfx::Click);
}

void Game::drawSettings() {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(3, 4, 14, 205));
    int px = w / 2 - 340, py = 110;
    panel(px, py, 680, 500, Palette::purple);
    addText(w / 2, py + 20, 32, L"游戏设置 // SETTINGS", Palette::purple, 1, FW_BOLD);

    int mx = Input::mx(), my = Input::my();
    bool held = Input::mouseDown(0);

    auto slider = [&](int id, int y, const wchar_t* label, float& value, Color accent) {
        int sx = px + 250, sw = 330;
        addText(px + 46, y - 3, 18, label, Palette::white);
        r_.fillRect(sx, y, sw, 10, color(10, 12, 30, 255));
        r_.fillRect(sx, y, int(sw * value), 10, Color{accent.r, accent.g, accent.b, 200});
        r_.strokeRect(sx, y, sw, 10, accent, 1);
        r_.glowCircle(float(sx + int(sw * value)), float(y + 5), 8, accent, 1.6f);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", int(value * 100));
        addText(sx + sw + 14, y - 3, 16, buf, accent);
        bool inside = mx >= sx - 8 && mx <= sx + sw + 8 && my >= y - 12 && my <= y + 22;
        if (Input::mousePressed(0) && inside) dragSlider_ = id;
        if (!held) dragSlider_ = -1;
        if (dragSlider_ == id && held)
            value = clampf(float(mx - sx) / sw, 0.0f, 1.0f);
    };
    slider(0, py + 96, L"背景音乐音量", musicVol_, Palette::cyan);
    slider(1, py + 156, L"音效音量", sfxVol_, Palette::pink);

    auto toggle = [&](int, int y, const wchar_t* label, bool& value, Color accent) {
        int bx = px + 250;
        addText(px + 46, y - 3, 18, label, Palette::white);
        r_.fillRect(bx, y, 54, 28, color(10, 12, 30, 255));
        r_.strokeRect(bx, y, 54, 28, accent, 2);
        if (value) {
            r_.fillRect(bx + 3, y + 3, 48, 22, Color{accent.r, accent.g, accent.b, 90});
            r_.glowLine(bx + 10, y + 14, bx + 22, y + 24, accent, 1.5f);
            r_.glowLine(bx + 22, y + 24, bx + 45, y + 5, accent, 1.5f);
        } else {
            r_.line(bx + 10, y + 8, bx + 44, y + 20, color(accent.r, accent.g, accent.b, 70), 2);
        }
        bool inside = mx >= bx && mx <= bx + 54 && my >= y && my <= y + 28;
        if (Input::mousePressed(0) && inside) {
            value = !value;
            Audio::instance().play(Sfx::Click);
        }
    };
    toggle(2, py + 226, L"屏幕震动", shakeOn_, Palette::orange);
    toggle(3, py + 286, L"扫描线效果", scanlinesOn_, Palette::green);
    toggle(4, py + 346, L"高粒子密度", particlesOn_, Palette::cyan);

    if (button(w / 2 - 110, py + 420, 220, 50, L"返回主菜单", Palette::pink, 20)) {
        state_ = State::Menu;
        Audio::instance().play(Sfx::Click);
    }
}

// ============================================================
// 绘制：升级 / 暂停 / 结算
// ============================================================
void Game::drawLevelUp() {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(4, 6, 20, 225));
    for (int i = 0; i < 3; ++i) {
        float y = (h * (i + 1)) / 4.0f;
        r_.lineAdd(0, y, w, y, color(80, 60, 200, 18), 12.0f);
    }
    panel(w / 2 - 480, 100, 960, 520, Palette::green, color(8, 18, 30, 240));
    neonTitle(w / 2, 122, 40, L"系统升级 // CHOOSE ONE", Palette::green);
    addText(w / 2, 176, 15, L"选择一个强化模块（也可按 1 / 2 / 3）", Palette::dim, 1);

    for (int i = 0; i < 3; ++i) {
        int cw = 280, ch = 248;
        int cx = w / 2 - 440 + i * 300, cy = 218;
        int mx = Input::mx(), my = Input::my();
        bool hover = mx >= cx && mx <= cx + cw && my >= cy && my <= cy + ch;
        Color accent = i == 0 ? Palette::cyan : (i == 1 ? Palette::pink : Palette::purple);
        r_.fillRect(cx, cy, cw, ch, hover ? Color{accent.r, accent.g, accent.b, 55} : color(12, 14, 38, 235));
        r_.strokeRect(cx, cy, cw, ch, hover ? Palette::white : accent, hover ? 3 : 2);
        r_.fillRect(cx, cy, cw, 4, accent);
        // 模块图标
        float icx = float(cx + cw / 2), icy = float(cy + 68);
        r_.glowRing(icx, icy, 27, 3, accent, 2.2f);
        r_.glowCircle(icx, icy, 8, accent, 2.4f);
        for (int k = 0; k < 6; ++k) {
            float a = time_ * 0.8f + k * 1.047197f;
            r_.lineAdd(icx + std::cos(a) * 33, icy + std::sin(a) * 33,
                       icx + std::cos(a) * 42, icy + std::sin(a) * 42,
                       Color{accent.r, accent.g, accent.b, 120}, 2);
        }
        const Skill& s = skills_[choices_[i]];
        addText(cx + cw / 2, cy + 112, 21, s.name, hover ? Palette::white : accent, 1, FW_BOLD);
        addText(cx + cw / 2, cy + 154, 15, s.desc, Palette::white, 1);
        wchar_t num[8];
        swprintf(num, 8, L"0%d", i + 1);
        addText(cx + 14, cy + ch - 34, 14, num, Color{accent.r, accent.g, accent.b, 180});
        addText(cx + cw - 14, cy + ch - 34, 13, L"选择", hover ? Palette::white : Palette::dim, 2);
        if (hover && Input::mousePressed(0)) applySkill(choices_[i]);
    }
}

void Game::drawPause() {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(4, 5, 18, 210));
    panel(w / 2 - 230, 150, 460, 430, Palette::cyan);
    neonTitle(w / 2, 178, 44, L"已暂停", Palette::cyan);
    addText(w / 2, 238, 14, L"系统挂起 // 战术暂停", Palette::dim, 1);
    if (button(w / 2 - 160, 282, 320, 52, L"继续游戏", Palette::green, 21)) {
        state_ = State::Playing; Audio::instance().play(Sfx::Click);
    }
    if (button(w / 2 - 160, 348, 320, 52, L"重新开始", Palette::cyan, 21)) {
        resetGame();
    }
    if (button(w / 2 - 160, 414, 320, 52, L"返回主菜单", Palette::pink, 21)) {
        state_ = State::Menu; Audio::instance().play(Sfx::Click);
    }
    addText(w / 2, 540, 12, L"按 ESC 继续", Palette::dim, 1);
}

void Game::drawEnd(bool victory) {
    int w = r_.width(), h = r_.height();
    r_.fillRect(0, 0, w, h, color(4, 4, 16, 220));
    int px = w / 2 - 330, py = 110;
    Color accent = victory ? Palette::green : Palette::red;
    panel(px, py, 660, 500, accent, color(10, 12, 32, 240));
    neonTitle(w / 2, py + 26, 48, victory ? L"任务完成" : L"任务失败", accent);
    addText(w / 2, py + 92, 15,
            victory ? L"机械核心已被摧毁 // 城市重获希望" : L"猎人信号丢失 // 禁区仍在扩张",
            Palette::dim, 1);

    int sec = int(gameTime_);
    wchar_t v1[64], v2[64], v3[64], v4[64];
    swprintf(v1, 64, L"%02d:%02d", sec / 60, sec % 60);
    swprintf(v2, 64, L"%d", player_.kills);
    swprintf(v3, 64, L"LV.%d", player_.level);
    swprintf(v4, 64, L"%d", player_.gold);
    const wchar_t* labels[4] = {L"存活时间", L"击杀数量", L"最终等级", L"获得金币"};
    wchar_t* values[4] = {v1, v2, v3, v4};
    for (int i = 0; i < 4; ++i) {
        int y = py + 142 + i * 52;
        r_.fillRect(px + 54, y, 552, 42, color(16, 20, 46, 220));
        r_.fillRect(px + 54, y, 3, 42, accent);
        addText(px + 74, y + 10, 17, labels[i], Palette::dim);
        addText(px + 582, y + 9, 20, values[i], i == 0 ? Palette::cyan : Palette::white, 2, FW_BOLD);
    }
    if (button(w / 2 - 240, py + 380, 230, 54, L"重新开始", Palette::cyan, 21)) resetGame();
    if (button(w / 2 + 10, py + 380, 230, 54, L"返回主菜单", Palette::pink, 21)) {
        state_ = State::Menu; Audio::instance().play(Sfx::Click);
    }
}

// ============================================================
// 渲染总入口
// ============================================================
void Game::render() {
    r_.beginFrame(color(4, 5, 16));
    bool inMenu = state_ == State::Menu || state_ == State::HowTo || state_ == State::Settings;
    if (inMenu) {
        drawBackdrop(time_ * 9.0f, time_ * 6.0f, time_);
        // 环境浮尘
        if (particles_.size() < 60 && randf() < 0.5f) {
            Particle p;
            p.pos = Vec2{rrange(0.0f, float(r_.width())), rrange(0.0f, float(r_.height()))};
            p.vel = Vec2{rrange(-12.0f, 12.0f), rrange(-25.0f, -6.0f)};
            p.life = p.maxLife = rrange(2.0f, 4.0f);
            p.size = rrange(1.0f, 2.4f);
            p.color = randf() < 0.5f ? Palette::cyan : Palette::pink;
            p.kind = 0;
            particles_.push_back(p);
        }
        for (const Particle& p : particles_) drawParticle(r_, p, 0, 0);
        if (state_ == State::Menu) drawMenu();
        else if (state_ == State::HowTo) drawHowTo();
        else drawSettings();
    } else {
        drawWorld(cam_.x, cam_.y);
        if (state_ == State::Playing) drawHUD();
        else if (state_ == State::LevelUp) { drawHUD(); drawLevelUp(); }
        else if (state_ == State::Paused) { drawHUD(); drawPause(); }
        else if (state_ == State::GameOver) drawEnd(false);
        else if (state_ == State::Victory) drawEnd(true);
    }

    float scan = scanlinesOn_ ? 0.10f : 0.0f;
    float flash = flash_;
    if (bossSpawned_ && state_ == State::Playing && bannerTimer_ > 0)
        flash = std::max(flash, 0.12f * (0.5f + 0.5f * std::sin(time_ * 14.0f)));
    r_.postProcess(0.90f, scan, flash, flashColor_, 0.58f);

    r_.flush();
    flushText();

    int sx = 0, sy = 0;
    if (shakeOn_ && shake_ > 0.2f) {
        sx = int(rrange(-shake_, shake_));
        sy = int(rrange(-shake_, shake_));
    }
    r_.present(hwnd_, sx, sy);
}

} // namespace nz
