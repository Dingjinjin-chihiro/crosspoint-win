# 构建与运行记录（`latest-firmware` 分支）

本分支把固件升级到 **2.x（2026‑10‑27 `c9cb1f8d`）**，并换用上游移植 PR #6 的模拟器内核。
`main` 分支是另一套（原版模拟器 + 1.0.0 固件），记录见该分支的 `BUILD_NOTES.md`。

---

## 1. 结构

```
.
├─ CMakeLists.txt / cmake/ / sim/   模拟器（上游 PR #6 的移植版）
├─ Crosspoint/                      固件 submodule → 我们的 fork，分支 emulator-latest（= 上游 c9cb1f8d）
│  └─ freeink-sdk/                  固件自己的 submodule（也要 init）
├─ third_party/ArduinoJson/         ArduinoJson 7.4.2 单头文件（免得再下一次）
├─ patches/                         本版本不需要补丁（见其中的 README）
├─ sdcard/                          虚拟 SD 卡
├─ scripts/                         示例书生成、按键注入、帧缓冲诊断
├─ docs/screenshots/
├─ setup_toolchain.cmd              便携工具链
├─ prepare_firmware.cmd             递归初始化子模块 + 应用 patches/（幂等）
├─ build.cmd / run_emulator.cmd
└─ tools/msys64/                    工具链（gitignore）
```

## 2. 与 main 分支相比的构建差异

| 项目 | main | 本分支 |
|---|---|---|
| C++ 标准 | C++17 | C++20 |
| 固件 HAL | 模拟器自带 sim 版 HAL 头文件 | 直接用固件 `lib/hal`，模拟器只实现平台相关部分 |
| 额外库 | 无 | FreeInk SDK（font/ui/network/catalog/book 等）+ PNGdec 1.1.6 + JPEGDEC + AnimatedGIF |
| 依赖获取 | 无（ArduinoJson 内置） | CMake `FetchContent` 拉 PNGdec / JPEGDEC / AnimatedGIF；ArduinoJson 用 `third_party/` |
| 固件补丁 | 2 个 | 0 个（上游已修复 `cstdint`、`goToReader` 按值传参） |
| 链接方式 | 动态运行库 | `-static-libgcc -static-libstdc++`，运行期只需要 `SDL2.dll` |

## 3. 三个容易踩的坑（已处理）

1. **JPEGDEC 补丁的换行符**
   固件自带的 `scripts/jpegdec_patches/*.patch` 是 CRLF，目标源码是 LF，`git apply` 会报
   `patch does not apply`。本机验证时先把补丁转 LF 再应用。若你手动构建遇到这个问题，
   用 `sed -i 's/\r$//'` 处理补丁即可。CI 里走 `FetchContent` 的 `PATCH_COMMAND`
   时由 CMake 传目录、`cmake/apply_patches.cmake` 用 `git apply`——同样会受此影响，
   所以 CI 使用 `core.autocrlf=false` 检出。

2. **freeink-sdk 是二级子模块**
   `Crosspoint/freeink-sdk` 没有 init 时会缺 `libs/`，CMake 直接找不到源码。
   `prepare_firmware.cmd` 用 `--recursive` 一次拉齐。

3. **首次 configure 会联网**
   PNGdec / JPEGDEC / AnimatedGIF 通过 `FetchContent` 下载（约 25 MB）。
   国内网络可以配代理，或预先 `git clone` 到 `build/_deps/*-src` 后再 configure。

## 4. 上游 HEAD 的情况（尚未跟进）

2026‑11‑21 的主干（`ca1a8c1`）比本分支新约 3 周，直接编译会在 HAL 层失败：

```
lib/hal/HalDisplay.h:19  'const class EInkDisplay' has no member named 'getControllerName'
lib/hal/HalDisplay.h:21  'const class EInkDisplay' has no member named 'getPanelName'
lib/hal/HalDisplay.h:23  'class EInkDisplay' has no member named 'setReaderRefresh'
...
sim/src/sim_display.cpp  no declaration matches 'void HalDisplay::drawImageTransparent(...)'
sim/src/net_stubs.cpp    redefinition of 'void freeink::SecureClient::reserveFullRecord()'
```

需要给 `sim/include/EInkDisplay.h` 补齐 `getControllerName` / `getPanelName` / `setReaderRefresh` /
`setOverlayFullTone` / `displayRetainedGrayscaleOverlay`，并按新 `HalDisplay` 头重写
`sim/src/sim_display.cpp` 的 `HalDisplay` 部分（`displayBufferAsync`、`waitRefreshComplete`、
`lendFrameBufferStorage`、`controllerIdle`、`writeGrayscalePlaneStrip` 等），再修 `net_stubs.cpp`
的重复定义。属于下一轮工作。

## 5. 实测记录

- 构建：271 / 295 个目标链接成功，产物 `crosspoint_emulator.exe`（约 10 MB）+ `SDL2.dll`
- 运行：首页 → Browse Files（TXT/EPUB 列表）→ 打开 EPUB → 第 1 页（1/2 22%）
  → 翻页到 Chapter 2（1/1 74%）→ 阅读菜单（书签/剪贴/夜间模式/截图/二维码/同步/文本设置）
- 进程稳定，未崩溃
