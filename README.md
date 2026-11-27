# 霓虹禁区：最后的幸存者（NEON ZONE: LAST SURVIVOR）

一款用纯 C++17 + Win32 自研渲染器开发的 2D 俯视角霓虹动作射击游戏。**零第三方依赖**，不需要 SFML / SDL2 / 引擎，不需要下载 DLL，编译后是一个独立 EXE。

## 下载

不想自己编译？直接下载免安装版：

**[NeonZone.exe (v1.0.0)](https://github.com/edisonChu666/NeonZone-game/releases/latest)** — 静态链接，双击即玩，无需任何运行库。

## 快速开始

### 方式一：直接双击编译脚本
1. 双击 `build.bat`
2. 编译完成后运行生成的 `NeonZone.exe`

脚本会自动在 PATH 中查找 `g++`。也可设置环境变量 `W64DEVKIT` 或 `MINGW64` 指向工具链根目录，脚本会自动把其 `bin` 加入 PATH。若都没有，请先安装 w64devkit / MinGW-w64 并把 `bin` 目录加入 PATH。

### 方式二：CMake
```bash
cmake -S . -B build
cmake --build build
build\NeonZone.exe
```

### 方式三：手动编译
```bash
g++ -std=c++17 -O2 -municode -mwindows \
    -static-libgcc -static-libstdc++ \
    src/main.cpp src/core/*.cpp src/game/*.cpp \
    -o NeonZone.exe -lwinmm
```

## 操作

| 按键 | 功能 |
|---|---|
| `W A S D` / 方向键 | 移动 |
| 鼠标 | 瞄准 |
| 鼠标左键（按住） | 连续射击 |
| `ESC` / `P` | 暂停 / 继续 |
| 升级界面 `1` `2` `3` | 选择强化技能 |
| 主菜单 `Enter` | 快速开始 |

## 游戏流程

主菜单 → 开始游戏 → 波次进攻 → 击杀掉落经验/金币/能量 → 升级选技能 → 精英怪（约每 45 秒）→ 3 分钟预警 → 机械核心 Boss → 胜利结算。
生命归零则进入失败结算。两种结算均显示存活时间、击杀数、等级、金币，并可重新开始或返回主菜单。

## 敌人与技能

- **机械兵**：六边形装甲，红色独眼，数量型。
- **追踪怪**：粉色飞镖，高速绕行接近。
- **远程炮台**：八角基座，保持距离并发射橙色弹丸。
- **精英怪**：八向尖刺，双层护盾，橙红核心。
- **机械核心 Boss**：旋转装甲板 + 四条机械臂，环形弹幕、扇形齐射、召唤增援。

升级技能池共 12 项：伤害、射速、移速、治疗、穿透、范围爆炸、护盾、弹速、吸血、自愈、多重射击、应急充能。

## 画面与音效

- 自研 32 位 DIB 软件渲染器：加法混合霓虹光、1/4 分辨率 Bloom 泛光、扫描线、暗角、受击闪屏、屏幕震动。
- 地面：深蓝紫渐变 + 世界网格 + 程序化电路纹理 + 流动光带。
- 玩家/敌人/Boss 全部由多边形与发光线条程序化绘制，各有独立造型与动画；子弹带拖尾；命中与爆炸有火花、烟雾、冲击环、碎片粒子。
- 音频由 `winmm` 实时合成：118 BPM 合成波背景音乐（贝斯/琶音/铺底/鼓组）+ 12 种程序化音效。无音频设备时自动静默，不影响游戏。想换成真实音频，替换 `src/core/Audio.cpp` 中的 `makeSfx()` 与 `mix()` 即可。

## 项目结构

```
NeonZone/
├── src/
│   ├── main.cpp              Win32 入口、窗口、主循环
│   ├── core/
│   │   ├── Util.h            数学/颜色/随机数
│   │   ├── Renderer.h/.cpp   DIB 渲染器与后处理
│   │   ├── Input.h/.cpp      键鼠输入
│   │   └── Audio.h/.cpp      程序化音频与混音线程
│   └── game/
│       ├── Entities.h/.cpp   实体数据与程序化绘制
│       └── Game.h/.cpp       状态机、战斗系统、UI
├── build.bat                 一键编译
├── CMakeLists.txt            CMake 构建
└── README.md
```

## 自动化测试开关（仅测试用）

编译测试版时可缩短 Boss 时间、降低 Boss 血量、开启无敌：

```bash
g++ ... -DNZ_BOSS_TIME=10.f -DNZ_BOSS_HP=420.f -DNZ_GODMODE ... -o NeonZone_Test.exe
```

正式构建不定义这些宏，Boss 为 3 分钟登场、4600 血、玩家正常受伤。

## 已知说明

- 窗口固定 1280×720 客户区，UI 按此分辨率设计，避免缩放错位。
- 掩体同时阻挡玩家自己的子弹，属于设计内容而非缺陷。
- 屏幕震动、扫描线、粒子密度、音量均可在设置界面实时调整。
