#include "Input.h"
#include <cstring>
#include <algorithm>

namespace nz {

namespace {
    bool keys_[256]{}, prevKeys_[256]{};
    bool mb_[3]{}, prevMb_[3]{};
    int mx_ = 0, my_ = 0, wheel_ = 0;
}

void Input::onKey(UINT vk, bool down) {
    if (vk < 256) keys_[vk] = down;
}
void Input::onMouseMove(int x, int y) { mx_ = x; my_ = y; }
void Input::onMouseButton(int b, bool down) {
    if (b >= 0 && b < 3) mb_[b] = down;
}
void Input::onWheel(int delta) { wheel_ = delta; }

void Input::update() {
    std::memcpy(prevKeys_, keys_, sizeof(keys_));
    std::memcpy(prevMb_, mb_, sizeof(mb_));
    wheel_ = 0;
}

void Input::clear() {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(prevKeys_, 0, sizeof(prevKeys_));
    std::memset(mb_, 0, sizeof(mb_));
    std::memset(prevMb_, 0, sizeof(prevMb_));
}

bool Input::down(int vk)     { return vk >= 0 && vk < 256 && keys_[vk]; }
bool Input::pressed(int vk)  { return vk >= 0 && vk < 256 && keys_[vk] && !prevKeys_[vk]; }
bool Input::released(int vk) { return vk >= 0 && vk < 256 && !keys_[vk] && prevKeys_[vk]; }
bool Input::mouseDown(int b)     { return b >= 0 && b < 3 && mb_[b]; }
bool Input::mousePressed(int b)  { return b >= 0 && b < 3 && mb_[b] && !prevMb_[b]; }
int  Input::mx() { return mx_; }
int  Input::my() { return my_; }
int  Input::wheel() { return wheel_; }

} // namespace nz
