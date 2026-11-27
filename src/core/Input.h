#pragma once
// ============================================================
// Input.h - 键盘 / 鼠标输入（由窗口过程喂入，每帧 update 翻转边沿）
// ============================================================
#include <windows.h>

namespace nz {

class Input {
public:
    static void onKey(UINT vk, bool down);
    static void onMouseMove(int x, int y);
    static void onMouseButton(int button, bool down);   // 0=左键 1=右键 2=中键
    static void onWheel(int delta);
    static void update();                                // 帧末调用
    static void clear();                                 // 失焦时清空

    static bool down(int vk);
    static bool pressed(int vk);
    static bool released(int vk);
    static bool mouseDown(int button);
    static bool mousePressed(int button);
    static int  mx();
    static int  my();
    static int  wheel();
};

} // namespace nz
