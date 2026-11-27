#pragma once
// ============================================================
// Audio.h - 程序化音频（winmm waveOut 实时混音，零音频资源）
// 所有音效由代码合成；如需替换为真实音频，只需改 Sfx 数据生成处
// ============================================================
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <windows.h>
#include <mmsystem.h>

namespace nz {

enum class Sfx {
    Shoot, Hit, Explosion, LevelUp, BossAppear, Victory, Defeat,
    Coin, Hurt, EnemyShoot, Click, Energy
};

class Audio {
public:
    static Audio& instance();

    bool init();
    void shutdown();
    void play(Sfx id, float volume = 1.0f);

    void setMusicVolume(float v);
    void setSfxVolume(float v);
    void setMusicEnabled(bool on);
    void setSfxEnabled(bool on);
    float musicVolume() const { return musicVol_.load(); }
    float sfxVolume() const { return sfxVol_.load(); }
    bool  musicEnabled() const { return musicOn_.load(); }
    bool  sfxEnabled() const { return sfxOn_.load(); }

private:
    Audio() = default;
    ~Audio() = default;
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    struct Voice {
        const std::vector<float>* data;
        size_t pos = 0;
        float vol = 1.0f;
        float pan = 0.0f;   // -1 左, +1 右
        uint64_t id = 0;    // 唯一 ID，用于混音后精确回写播放位置
    };

    void audioThread();
    void mix(int16_t* out, int frames);
    void buildSfx();
    static std::vector<float> makeSfx(Sfx id);

    HWAVEOUT hwo_ = nullptr;
    HANDLE hEvent_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};

    static const int kRate = 22050;
    static const int kBufFrames = 2048;
    static const int kBufCount = 4;
    WAVEHDR headers_[kBufCount]{};
    std::vector<int16_t> buffers_[kBufCount];

    std::mutex voiceMutex_;
    std::deque<Voice> voices_;
    std::vector<std::vector<float>> sfxData_;
    uint64_t nextVoiceId_ = 0;   // 仅主线程 play() 分配，回调线程不写

    uint64_t musicSample_ = 0;
    uint32_t noiseSeed_ = 12345u;
    std::atomic<float> musicVol_{0.55f};
    std::atomic<float> sfxVol_{0.8f};
    std::atomic<bool> musicOn_{true};
    std::atomic<bool> sfxOn_{true};
};

} // namespace nz
