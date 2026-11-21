# Crosspoint for Windows — 模拟器 + 固件

在 Windows 上用 SDL2 窗口运行 [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader)
电子书固件，不需要真机、不需要烧录。本仓库把我们跑通它所需的**固件版本、编译修正、真实 bug 修复和工具链**
一起固化下来，克隆即可复现。

- 模拟器内核来自 [jonmooreai/Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator)（MIT）
- 固件来自 [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)（MIT），
  以 submodule 指向我们的 fork [Dingjinjin-chihiro/crosspoint-reader](https://github.com/Dingjinjin-chihiro/crosspoint-reader) 分支 `emulator-1.0.0`
  （上游 tag `1.0.0` + 两处修复）；同一修复也以补丁形式放在 `patches/`

[![build](https://github.com/Dingjinjin-chihiro/crosspoint-win/actions/workflows/build.yml/badge.svg)](https://github.com/Dingjinjin-chihiro/crosspoint-win/actions/workflows/build.yml)

![首页](docs/screenshots/00-window.png)

## 30 秒跑起来

需要 Windows 10/11 x64、Python 3（生成示例书用）、网络（首次拉工具链）。

```bat
git clone --recursive https://github.com/Dingjinjin-chihiro/crosspoint-win.git
cd crosspoint-win

setup_toolchain.cmd   :: 首次：下载便携 MSYS2 + MinGW / CMake / Ninja / SDL2（约 5 分钟）
build.cmd             :: 取固件 submodule + 打补丁 + 编译（约 3–5 分钟）
run_emulator.cmd      :: 启动
```

已经装过工具链就只需要后两条。

## 按键

| 按键 | 功能 |
|------|------|
| ← ↑ → ↓ | 四向导航 |
| Enter | 确认 / 打开 |
| Backspace 或 Esc | 返回 |
| P | 电源键（设置里长按可休眠） |

关闭窗口即退出。

## 放书

把 `.epub` / `.txt` / `.xtc` 放进 `sdcard/`（可以建子目录），重启模拟器后在 **Browse Files** 里就能看到。
仓库自带两本示例书：`A Short Walk.txt`、`The Quiet Shelf.epub`。

生成新的示例 EPUB：

```bat
python scripts\make_sample_epub.py sdcard
```

## 直接下载构建好的程序

不想装工具链的话，可以在 [Actions](https://github.com/Dingjinjin-chihiro/crosspoint-win/actions/workflows/build.yml) 里挑一次成功的 run，下载 `crosspoint-emulator-windows` artifact（exe + 4 个运行期 DLL）。
解压后把 exe 和 DLL 放在同一目录，并在同级建一个 `sdcard/` 放书即可运行。


## 目录结构

```
.
├─ CMakeLists.txt / sim/     模拟器源码（SDL2 窗口、虚拟 SD 卡、键盘映射）
├─ Crosspoint/               固件（git submodule，我们自己的 fork，基于上游 1.0.0）
├─ patches/                  我们对固件的两处修复（构建前自动 git apply）
├─ third_party/ArduinoJson/  ArduinoJson 7.4.2 单头文件（免装 PlatformIO）
├─ sdcard/                   虚拟 SD 卡根目录
├─ scripts/                  示例书生成、按键注入、帧缓冲诊断
├─ docs/screenshots/         实测截图
├─ BUILD_NOTES.md            完整构建记录、全部改动与原因
├─ setup_toolchain.cmd       安装便携工具链
├─ prepare_firmware.cmd      拉 submodule + 应用补丁（幂等）
├─ build.cmd / run_emulator.cmd
└─ tools/msys64/             工具链落点（已 gitignore）
```

> 只想更新固件到某个上游提交时：`git -C Crosspoint fetch origin <sha> && git -C Crosspoint checkout <sha>`，
> 然后重新跑 `build.cmd` 看补丁是否还能 `git apply`（不能的话按行号手工改一下，补丁就在 `patches/`）。

## 为什么固定固件版本

模拟器最后一次提交是 2026‑02‑11，固件主干此后经历了 HAL 重构（`SdMan` → 全局 `HalStorage Storage`）、
新增 `freeink-sdk` 子模块、C++20 等，直接对主干编译会在 `ContentProtection.h`、`Storage` 未声明处失败。
我们固定到同期的 tag `1.0.0`（2026‑02‑09），其 HAL 与模拟器的 `sim/include` 存根完全对应。

固件在此基础上打了两个补丁（`patches/`，由 `prepare_firmware.cmd` 自动应用）：

1. **修复 use‑after‑free（真实 bug）**：`onGoToReader()` / `onGoToMyLibraryWithPath()` 在
   `exitActivity()` 之后才使用传入的 `const std::string&`，而该字符串通常由即将被销毁的 Activity 持有。
   在 Windows/MinGW 下稳定触发 `0xC0000005`。改为先拷贝再销毁。
2. `CrossPointState.h` 补 `#include <cstdint>`（设备端靠 Arduino.h 兜底，宿主构建会报错）。

`prepare_firmware.cmd` 与 CI 都会先判断补丁是否已应用，重复执行安全。补丁带完整提交信息，也可以 `git am patches/*.patch` 直接打到上游分支。

## 已验证

首页（最近书籍封面 / 菜单 / 状态栏）· 文件浏览 · TXT 阅读（自动换行、页码百分比）·
EPUB 阅读（章节标题、两端对齐、翻页到下一章）· 阅读菜单 · 返回导航，均无崩溃。

未覆盖：Wi‑Fi / OTA / WebDAV（模拟器里为桩实现）、DRM 书籍（需要 wolfSSL）。

## 许可

模拟器与固件均为 MIT。本仓库的分支/修改同样以 MIT 发布，原始版权见 `NOTICE`。



