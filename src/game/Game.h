#pragma once
// ============================================================
// Game.h - 游戏主状态机 / 系统调度 / UI
// ============================================================
#include "../core/Renderer.h"
#include "../core/Input.h"
#include "../core/Audio.h"
#include "Entities.h"
#include <vector>
#include <string>

namespace nz {

enum class State { Menu, HowTo, Settings, Playing, LevelUp, Paused, GameOver, Victory };

class Game {
public:
    Game(Renderer& renderer, HWND hwnd);
    void frame(float dt);

private:
    // ---- 状态 ----
    State state_ = State::Menu;
    Renderer& r_;
    HWND hwnd_;
    float time_ = 0;          // 全局动画时间
    float gameTime_ = 0;      // 本局存活时间
    float shake_ = 0, flash_ = 0;
    Color flashColor_ = Palette::red;
    Vec2 cam_{0, 0};

    static const int WORLD_W = 2600, WORLD_H = 1900;
    static const float BOSS_TIME;

    Player player_;
    std::vector<Enemy> enemies_;
    std::vector<Bullet> bullets_;
    std::vector<Particle> particles_;
    std::vector<FloatText> texts_;
    std::vector<Pickup> pickups_;
    std::vector<Obstacle> obstacles_;

    float spawnTimer_ = 2.0f;
    float eliteTimer_ = 42.0f;
    bool bossSpawned_ = false;
    float bossWarnShown_ = -1;
    float bannerTimer_ = 0;
    std::wstring bannerText_;
    Color bannerColor_ = Palette::pink;
    Enemy* boss_ = nullptr;

    // ---- 技能 ----
    struct Skill { int id; const wchar_t* name; const wchar_t* desc; };
    static const Skill skills_[];
    int choices_[3]{0, 0, 0};
    int dragSlider_ = -1;

    // ---- 设置 ----
    float musicVol_ = 0.55f, sfxVol_ = 0.80f;
    bool shakeOn_ = true, scanlinesOn_ = true, particlesOn_ = true;

    // ---- UI 文本队列（GDI 文字统一在后处理之后绘制）----
    struct TextItem {
        int x, y, size, weight, align;
        std::wstring str;
        Color color;
        bool shadow;
    };
    std::vector<TextItem> textQueue_;

    // ---- 内部方法 ----
    void addText(int x, int y, int size, const wchar_t* s, Color c,
                 int align = 0, int weight = FW_NORMAL, bool shadow = false);
    void flushText();

    void resetGame();
    void updateWorld(float dt);
    void updatePlayer(float dt);
    void updateEnemies(float dt);
    void updateBullets(float dt);
    void updatePickups(float dt);
    void updateParticles(float dt);
    void updateSpawner(float dt);

    void spawnEnemy(EnemyType type, float x, float y);
    void spawnBullet(Vec2 pos, Vec2 dir, float speed, float dmg, bool enemy,
                     int pierce, float blast, Color c);
    void spawnParticles(Vec2 pos, Color c, int count, float speed, int kind, float size);
    void spawnDeathFX(const Enemy& e);
    void explode(Vec2 pos, float radius, float damage);
    void damageEnemy(Enemy& e, float dmg, Vec2 from);
    void damagePlayer(float dmg);
    void grantXP(int amount);
    void checkLevelUp();
    void applySkill(int id);
    void newChoices();

    bool hitObstacle(Vec2 p, float radius) const;
    void pushOutOfObstacles(Vec2& p, float radius) const;
    Vec2 screenToWorld(int sx, int sy) const;
    Vec2 clampWorld(Vec2 p) const;

    // ---- 绘制 ----
    void render();
    void drawBackdrop(float camX, float camY, float t);
    void drawWorld(float camX, float camY);
    void drawHUD();
    void drawMenu();
    void drawHowTo();
    void drawSettings();
    void drawLevelUp();
    void drawPause();
    void drawEnd(bool victory);

    bool button(int x, int y, int w, int h, const wchar_t* label,
                Color accent, int fontSize = 20);
    void panel(int x, int y, int w, int h, Color border = Palette::blue, Color fill = color(10, 12, 32, 220));
    void bar(int x, int y, int w, int h, float ratio, Color fill, Color back);
    void neonTitle(int x, int y, int size, const wchar_t* s, Color c);
};

} // namespace nz
