# Crosspoint for Windows — 模拟器 + 固件（2.x 分支）

在 Windows 上用 SDL2 窗口运行 [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader)
电子书固件，不需要真机、不需要烧录。

> **这是 `latest-firmware` 分支**：固件升级到 **2026‑10‑27（`c9cb1f8d`，2.x 线）**，
> 用上游移植 PR #6 的新模拟器内核（C++20、直接复用固件 HAL、FreeInk SDK）。
> `main` 分支仍保留原版模拟器 + 1.0.0 固件，那套更老但链路完全稳定。

![首页](docs/screenshots/00-window.png)

## 30 秒跑起来

需要 Windows 10/11 x64、Python 3（工具链自带）、Git、网络（首次拉工具链和图片解码库）。

```bat
git clone --recursive -b latest-firmware https://github.com/Dingjinjin-chihiro/crosspoint-win.git
cd crosspoint-win

setup_toolchain.cmd   :: 首次：便携 MSYS2 + GCC / CMake / Ninja / SDL2 / Python（约 5 分钟）
build.cmd             :: 拉子模块（含 freeink-sdk）→ CMake 拉解码库 → 编译
run_emulator.cmd      :: 启动
```

首次 `build.cmd` 的 CMake 阶段会从 GitHub 取 PNGdec / JPEGDEC / AnimatedGIF（约 25 MB），
ArduinoJson 已内置在 `third_party/`，不需要下载。

## 按键

| 按键 | 功能 |
|------|------|
| ← ↑ → ↓ | 四向导航 |
| Enter | 确认 / 打开 |
| Backspace 或 Esc | 返回 |
| P | 电源键 |

## 放书

`.epub` / `.txt` / `.xtc` 放进 `sdcard/`，重启后在 **Browse Files** 里可见。
仓库自带两本示例书。

## 这个版本带来了什么

相比 `main` 分支的 1.0.0，2.x 固件在模拟器里已经能看到：

- 新首页（继续阅读块 + 带图标的 Browse Files / Library / File Transfer / Settings）
- 文件列表显示扩展名与图标
- 阅读菜单扩展：书签、摘录剪贴（Clipping）、夜间模式、自动翻页、截图、二维码分享、阅读方向、文本设置
- 进度显示更细（章节 × 页数 + 百分比）

网络相关（Wi‑Fi / OPDS / 插件 / OTA）在模拟器里是桩实现，按设计不工作。

## 与 main 分支的差异

| | `main` | `latest-firmware`（本分支） |
|---|---|---|
| 固件 | tag `1.0.0`（2026‑02‑08） | `c9cb1f8d`（2026‑10‑27，2.x） |
| 模拟器内核 | 原版（2026‑02‑11，自维护 sim HAL 头文件） | 上游 PR #6 的移植版（直接复用固件 HAL + FreeInk SDK） |
| 语言标准 | C++17 | C++20 |
| 固件补丁 | 2 个（cstdint、use‑after-free） | 不需要（上游已修复） |
| 额外依赖 | 无 | FreeInk SDK + PNGdec / JPEGDEC / AnimatedGIF |

`Crosspoint` 子模块指向我们自己的 fork 的分支 `emulator-latest`（内容等于上游 `c9cb1f8d`）。

## 已验证

首页 · 文件浏览 · **EPUB 阅读**（章节标题、两端对齐、1/2 22% 进度）· 翻页到下一章 · 阅读菜单 · 返回，
均在本机实测通过，无崩溃。

## 关于"最新"

上游主干现在（2026‑11‑21）比本分支固定的 `c9cb1f8d` 还新约 3 周。直接对 HEAD 编译会在 HAL 层失败：
`HalDisplay` 新增了 `displayBufferAsync` / `lendFrameBufferStorage` / `controllerIdle` /
`writeGrayscalePlaneStrip` 等接口，`EInkDisplay` 也加了 `getControllerName` 等方法，
模拟器的 `sim/src/sim_display.cpp` 需要跟着改一轮。这部分是后续工作，不影响本分支可用。

## 许可

模拟器（原版与移植版）与固件均为 MIT，FreeInk SDK 及其依赖见各自 LICENSE。原始版权见 `NOTICE`。
