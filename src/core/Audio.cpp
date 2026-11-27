// ============================================================
// Audio.cpp - 程序化合成音效 + 实时 BGM 步进音序器
// 无外部音频文件；所有波形在内存中生成
// ============================================================
#include "Audio.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace nz {

static constexpr float PI2 = 6.28318530718f;
static float hz(int midi) { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); }

// ---------- 音效波形合成 ----------
std::vector<float> Audio::makeSfx(Sfx id) {
    std::mt19937 rng(9876u + unsigned(id) * 7919u);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    float dur = 0.2f;
    switch (id) {
        case Sfx::Shoot: dur = 0.11f; break;
        case Sfx::Hit: dur = 0.10f; break;
        case Sfx::Explosion: dur = 0.55f; break;
        case Sfx::LevelUp: dur = 0.70f; break;
        case Sfx::BossAppear: dur = 1.70f; break;
        case Sfx::Victory: dur = 1.50f; break;
        case Sfx::Defeat: dur = 1.25f; break;
        case Sfx::Coin: dur = 0.18f; break;
        case Sfx::Hurt: dur = 0.30f; break;
        case Sfx::EnemyShoot: dur = 0.18f; break;
        case Sfx::Click: dur = 0.06f; break;
        case Sfx::Energy: dur = 0.38f; break;
    }
    int n = int(kRate * dur);
    std::vector<float> out(n);
    float lp = 0.0f;

    for (int i = 0; i < n; ++i) {
        float t = float(i) / kRate, u = t / dur;
        float s = 0.0f;
        switch (id) {
        case Sfx::Shoot: {
            float f = 1250.0f - 780.0f * u;
            float env = std::exp(-t * 34.0f);
            s = (std::sin(PI2 * f * t) * 0.55f + noise(rng) * 0.35f) * env;
            break;
        }
        case Sfx::Hit: {
            float env = std::exp(-t * 42.0f);
            s = (noise(rng) * 0.7f + std::sin(PI2 * (900.0f - 2400.0f * u) * t) * 0.4f) * env;
            break;
        }
        case Sfx::Explosion: {
            lp = lp * 0.88f + noise(rng) * 0.12f;
            float f = 95.0f - 55.0f * u;
            s = (lp * 2.6f + std::sin(PI2 * f * t) * 0.7f) * std::exp(-t * 5.2f);
            break;
        }
        case Sfx::LevelUp: {
            static const int notes[] = {69, 72, 76, 81, 84};
            float seg = 0.13f;
            int k = std::min(4, int(t / seg));
            float local = t - k * seg;
            float f = hz(notes[k]);
            s = (std::sin(PI2 * f * local) + 0.35f * std::sin(PI2 * f * 2 * local))
                * std::exp(-local * 7.0f) * 0.5f;
            break;
        }
        case Sfx::BossAppear: {
            float rumble = std::sin(PI2 * (58.0f - 28.0f * u) * t) * std::exp(-t * 1.2f) * 0.6f;
            float beep = 0.0f;
            for (float start : {0.25f, 0.72f, 1.18f}) {
                float dt = t - start;
                if (dt > 0 && dt < 0.24f)
                    beep = std::sin(PI2 * 660.0f * dt) * std::exp(-dt * 12.0f) * 0.35f;
            }
            s = rumble + beep + noise(rng) * 0.06f * (1.0f - u);
            break;
        }
        case Sfx::Victory: {
            static const int notes[] = {72, 76, 79, 84, 88};
            float seg = 0.24f;
            int k = std::min(4, int(t / seg));
            float local = t - k * seg;
            float f = hz(notes[k]);
            s = (std::sin(PI2 * f * local) + 0.4f * std::sin(PI2 * f * 1.5f * local))
                * std::exp(-local * 3.2f) * 0.45f;
            break;
        }
        case Sfx::Defeat: {
            static const int notes[] = {64, 60, 57, 52};
            float seg = 0.30f;
            int k = std::min(3, int(t / seg));
            float local = t - k * seg;
            float f = hz(notes[k]);
            s = (std::sin(PI2 * f * local) * 0.7f + std::sin(PI2 * f * 0.5f * local) * 0.3f)
                * std::exp(-local * 2.6f) * 0.5f;
            break;
        }
        case Sfx::Coin:
            s = std::sin(PI2 * (t < 0.08f ? 1180.0f : 1760.0f) * t) * std::exp(-t * 12.0f) * 0.5f;
            break;
        case Sfx::Hurt: {
            float env = std::exp(-t * 9.0f);
            s = (noise(rng) * 0.55f + std::sin(PI2 * (320.0f - 220.0f * u) * t) * 0.5f) * env;
            break;
        }
        case Sfx::EnemyShoot: {
            float f = 520.0f - 300.0f * u;
            s = std::sin(PI2 * f * t) * std::exp(-t * 16.0f) * 0.42f;
            break;
        }
        case Sfx::Click:
            s = std::sin(PI2 * 920.0f * t) * std::exp(-t * 40.0f) * 0.5f;
            break;
        case Sfx::Energy: {
            float f = 420.0f + 1100.0f * u;
            s = (std::sin(PI2 * f * t) + 0.3f * std::sin(PI2 * f * 2.0f * t))
                * std::exp(-t * 3.5f) * 0.4f;
            break;
        }
        }
        out[i] = std::clamp(s, -1.0f, 1.0f);
    }
    return out;
}

Audio& Audio::instance() {
    static Audio a;
    return a;
}

void Audio::buildSfx() {
    sfxData_.resize(12);
    for (int i = 0; i < 12; ++i) sfxData_[i] = makeSfx(Sfx(i));
}

bool Audio::init() {
    if (running_) return true;
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = kRate * 4;
    fmt.cbSize = 0;

    hEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent_) return false;

    MMRESULT r = waveOutOpen(&hwo_, WAVE_MAPPER, &fmt, (DWORD_PTR)hEvent_, 0, CALLBACK_EVENT);
    if (r != MMSYSERR_NOERROR) {
        CloseHandle(hEvent_);
        hEvent_ = nullptr;
        return false;   // 无音频设备时游戏静默运行
    }

    buildSfx();
    for (int i = 0; i < kBufCount; ++i) {
        buffers_[i].resize(size_t(kBufFrames) * 2);
        ZeroMemory(&headers_[i], sizeof(WAVEHDR));
        headers_[i].lpData = reinterpret_cast<LPSTR>(buffers_[i].data());
        headers_[i].dwBufferLength = DWORD(buffers_[i].size() * sizeof(int16_t));
        headers_[i].dwFlags = 0;
        waveOutPrepareHeader(hwo_, &headers_[i], sizeof(WAVEHDR));
        mix(buffers_[i].data(), kBufFrames);
        waveOutWrite(hwo_, &headers_[i], sizeof(WAVEHDR));
    }
    running_ = true;
    thread_ = std::thread(&Audio::audioThread, this);
    return true;
}

void Audio::shutdown() {
    if (!running_) return;
    running_ = false;
    waveOutReset(hwo_);
    if (thread_.joinable()) thread_.join();
    for (int i = 0; i < kBufCount; ++i) {
        waveOutUnprepareHeader(hwo_, &headers_[i], sizeof(WAVEHDR));
    }
    waveOutClose(hwo_);
    hwo_ = nullptr;
    if (hEvent_) { CloseHandle(hEvent_); hEvent_ = nullptr; }
}

void Audio::audioThread() {
    while (running_) {
        WaitForSingleObject(hEvent_, 120);
        if (!running_) break;
        for (int i = 0; i < kBufCount; ++i) {
            if ((headers_[i].dwFlags & WHDR_DONE) == 0) continue;
            waveOutUnprepareHeader(hwo_, &headers_[i], sizeof(WAVEHDR));
            mix(buffers_[i].data(), kBufFrames);
            headers_[i].dwFlags = 0;
            waveOutPrepareHeader(hwo_, &headers_[i], sizeof(WAVEHDR));
            waveOutWrite(hwo_, &headers_[i], sizeof(WAVEHDR));
        }
    }
}

void Audio::play(Sfx id, float volume) {
    if (!running_.load() || !sfxOn_.load()) return;
    int idx = int(id);
    if (idx < 0 || idx >= int(sfxData_.size())) return;
    Voice v;
    v.data = &sfxData_[idx];
    v.vol = std::clamp(volume, 0.0f, 2.0f);
    v.pan = (rand() % 200 - 100) / 220.0f;
    v.id = ++nextVoiceId_;   // 主线程分配，唯一
    std::lock_guard<std::mutex> lk(voiceMutex_);
    if (voices_.size() > 28) voices_.pop_front();
    voices_.push_back(v);
}

void Audio::setMusicVolume(float v) { musicVol_.store(std::clamp(v, 0.0f, 1.0f)); }
void Audio::setSfxVolume(float v) { sfxVol_.store(std::clamp(v, 0.0f, 1.0f)); }
void Audio::setMusicEnabled(bool on) { musicOn_.store(on); }
void Audio::setSfxEnabled(bool on) { sfxOn_.store(on); }

// ---------- 实时混音 ----------
void Audio::mix(int16_t* out, int frames) {
    // 原子量只读一次，避免与设置界面跨线程反复读写
    const bool musicOn = musicOn_.load();
    const float musicVol = musicVol_.load();
    const float sfxVol = sfxVol_.load();
    static const int chords[4][3] = {
        {57, 60, 64},   // Am
        {53, 57, 60},   // F
        {48, 52, 55},   // C
        {55, 59, 62}    // G
    };
    const float stepFrames = float(kRate) * 60.0f / (118.0f * 4.0f);

    // 取一份音效快照，避免在音频线程中长时间持锁。
    // 每个 Voice 带唯一 id，混音后用 id 精确回写，避免按位置错配。
    std::vector<Voice> local;
    {
        std::lock_guard<std::mutex> lk(voiceMutex_);
        local.assign(voices_.begin(), voices_.end());
    }

    for (int i = 0; i < frames; ++i) {
        float t = float(musicSample_) / kRate;
        int step = int(musicSample_ / stepFrames) & 63;
        float sp = float(musicSample_ % uint64_t(stepFrames)) / stepFrames;
        int ci = step / 16;
        const int* ch = chords[ci];

        float m = 0.0f;
        if (musicOn && musicVol > 0.001f) {
            // 低音：锯齿波，每拍强调
            int bassNote = ch[0] - 12 + ((step & 3) == 3 ? 7 : 0);
            float bf = hz(bassNote);
            float saw = 2.0f * (bf * t - std::floor(bf * t)) - 1.0f;
            m += saw * std::exp(-sp * 3.2f) * 0.20f;

            // 琶音：三角波 16 分音符
            int an = ch[(step + ci) % 3] + 12;
            float af = hz(an);
            float tri = 2.0f / PI2 * std::asin(std::sin(PI2 * af * t));
            m += tri * std::exp(-sp * 7.0f) * 0.11f;

            // 铺底和弦：两个轻微失谐的正弦
            float pad = std::sin(PI2 * hz(ch[0]) * t) * 0.5f +
                        std::sin(PI2 * hz(ch[2]) * 1.003f * t) * 0.35f;
            m += pad * (0.035f + 0.02f * std::sin(PI2 * 0.25f * t));

            // 底鼓
            if ((step & 3) == 0)
                m += std::sin(PI2 * (48.0f + 90.0f * std::exp(-sp * 22.0f)) * t)
                     * std::exp(-sp * 9.0f) * 0.30f;
            // 踩镲 / 军鼓
            noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
            float ns = float(int32_t(noiseSeed_)) / 2147483648.0f;
            if ((step & 1) == 0) m += ns * std::exp(-sp * 26.0f) * 0.055f;
            if ((step & 7) == 4) m += ns * std::exp(-sp * 13.0f) * 0.10f;
            m *= musicVol;
        }

        float l = m, r = m;
        for (auto& v : local) {
            if (v.pos >= v.data->size()) continue;
            float s = (*v.data)[v.pos++] * v.vol * sfxVol;
            l += s * (1.0f - std::max(0.0f, v.pan) * 0.55f);
            r += s * (1.0f + std::min(0.0f, v.pan) * 0.55f);
        }
        out[i * 2]     = int16_t(std::clamp(l, -1.0f, 1.0f) * 30000.0f);
        out[i * 2 + 1] = int16_t(std::clamp(r, -1.0f, 1.0f) * 30000.0f);
        ++musicSample_;
    }

    // 按 id 把快照的播放位置精确同步回共享队列：
    // - 快照中已播完的语音从共享队列删除
    // - 快照期间主线程新入队的语音（id 不在快照中）保持不动
    std::lock_guard<std::mutex> lk(voiceMutex_);
    for (auto it = voices_.begin(); it != voices_.end();) {
        auto found = std::find_if(local.begin(), local.end(),
                                  [&](const Voice& v) { return v.id == it->id; });
        if (found == local.end()) { ++it; continue; }   // 新入队，未参与本次混音
        if (found->pos >= found->data->size()) {
            it = voices_.erase(it);
        } else {
            it->pos = found->pos;
            ++it;
        }
    }
}

} // namespace nz
