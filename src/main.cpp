// ============================================================
// main.cpp - Win32 入口 / 窗口 / 主循环
// 项目零外部依赖：仅使用 Windows SDK + C++ 标准库
// ============================================================
#include "core/Renderer.h"
#include "core/Input.h"
#include "core/Audio.h"
#include "game/Game.h"

using namespace nz;

static const int VIEW_W = 1280;
static const int VIEW_H = 720;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE:
        Input::onMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        Input::onMouseButton(0, true);
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        Input::onMouseButton(0, false);
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        Input::onMouseButton(1, true);
        return 0;
    case WM_RBUTTONUP:
        Input::onMouseButton(1, false);
        return 0;
    case WM_MBUTTONDOWN:
        Input::onMouseButton(2, true);
        return 0;
    case WM_MBUTTONUP:
        Input::onMouseButton(2, false);
        return 0;
    case WM_MOUSEWHEEL:
        Input::onWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        Input::onKey(UINT(wParam), true);
        if (wParam == VK_SPACE || wParam == VK_UP || wParam == VK_DOWN) return 0;
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Input::onKey(UINT(wParam), false);
        return 0;
    case WM_KILLFOCUS:
        Input::clear();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    const wchar_t* className = L"NeonZoneWindowClass";
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    RECT rect{0, 0, VIEW_W, VIEW_H};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);
    int winW = rect.right - rect.left, winH = rect.bottom - rect.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
    int px = (screenW > winW) ? (screenW - winW) / 2 : 0;
    int py = (screenH > winH) ? (screenH - winH) / 2 : 0;

    HWND hwnd = CreateWindowExW(0, className, L"霓虹禁区：最后的幸存者  |  NEON ZONE",
                                style, px, py, winW, winH,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败", L"霓虹禁区", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    Renderer renderer(VIEW_W, VIEW_H);
    Audio::instance().init();       // 无音频设备时自动静默运行
    Game game(renderer, hwnd);

    LARGE_INTEGER freq{}, last{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    MSG msg{};
    bool running = true;

    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        float dt = float(double(now.QuadPart - last.QuadPart) / double(freq.QuadPart));
        last = now;
        if (dt < 0.0f) dt = 0.0f;

        game.frame(dt);
        Input::update();
    }

    Audio::instance().shutdown();
    DestroyWindow(hwnd);
    return 0;
}
