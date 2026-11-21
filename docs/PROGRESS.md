# 进度交接：Crosspoint for Windows

> 写给下一个会话。读完这一份就能直接继续，不需要翻聊天记录。
> 最后更新：2026-11-22 00:5x（本地时区 +08:00）

---

## 0. 一句话现状

Crosspoint 电子书固件（**上游主干 HEAD `ca1a8c1`，2026-11-21**）已经能在 Windows 上用 SDL2 模拟器跑起来，
**源码级可调试**（gdb 断点已实测命中），仓库在 `Dingjinjin-chihiro/crosspoint-win` 的 `latest-firmware` 分支，
CI 全绿。下一步是**界面美化 + 代码优化 + 调试能力增强 + 触摸输入**。

---

## 1. 仓库与远端

| 项目 | 位置 | 状态 |
|---|---|---|
| 主仓库 | <https://github.com/Dingjinjin-chihiro/crosspoint-win> | public, MIT |
| 分支 `main` | 旧版：原版模拟器 + 固件 `1.0.0`（2026-02-08） | 稳定，勿动 |
| 分支 `latest-firmware` | **当前工作分支**：移植版模拟器 + 固件 HEAD | 最新提交 `10587c6` |
| 固件 fork | <https://github.com/Dingjinjin-chihiro/crosspoint-reader> | 分支 `emulator-latest` → `ca1a8c1`；分支 `emulator-1.0.0` → `00e9b90` |
| 子模块 | `Crosspoint/` → 上面 fork 的 `emulator-latest`；其内还有二级子模块 `Crosspoint/freeink-sdk` | 必须 `--recursive` |
| CI | `.github/workflows/build.yml` | `windows-latest` + MSYS2/MinGW，push 到 main/latest-firmware 触发 |

**本地工作区**：`C:\Users\DINGJINJIN\Downloads\cross_point`

```
cross_point/
├─ crosspoint-win/          ← git 仓库工作副本（当前在 latest-firmware）
│  ├─ Crosspoint/           ← 固件子模块（ca1a8c1）+ freeink-sdk
│  ├─ build/                ← Release 产物（crosspoint_emulator.exe + SDL2.dll）
│  ├─ build-debug/          ← RelWithDebInfo 产物（带符号，gdb 用，约 144 MB exe）
│  ├─ sim/                  ← 模拟器内核（PR #6 移植版 + 我们的 HEAD 适配）
│  ├─ third_party/ArduinoJson/
│  ├─ sdcard/               ← 虚拟 SD 卡（含两本示例书）
│  ├─ scripts/              ← 示例书生成、按键注入、帧缓冲诊断
│  ├─ docs/DEBUGGING.md     ← 调试手册（gdb/日志/截图/自动化/局限）
│  ├─ docs/screenshots/
│  └─ setup_toolchain.cmd / prepare_firmware.cmd / build.cmd / run_emulator.cmd
├─ Crosspoint-Emulator/     ← 旧版（main 分支时代）工作副本，仅作参考
├─ tools/msys64/            ← 便携工具链（GCC 16.2 / CMake 4.5 / Ninja / SDL2 2.32.10 / Python / gdb 18.1）
├─ deps/                    ← PNGdec/JPEGDEC/AnimatedGIF 的源码（离线构建用，已打补丁）
└─ scripts/                 ← 工作区级辅助脚本（port_head.py、upgrade_nav.ps1 等）
```

**环境要点**
- 代理：`HTTP_PROXY=http://127.0.0.1:7897`、`HTTPS_PROXY=同`（访问 GitHub 必需；直连约 38 KB/s）
- GitHub 认证：`gh` 已登录 `Dingjinjin-chihiro`，token 存在 keyring，**已含 `workflow` scope**（能推 `.github/workflows`）
- 工具链不进仓库（`.gitignore` 有 `tools/`、`build*/`、`deps/`）
- 当前可能还有模拟器进程在跑（旧版 pid 15956）

---

## 2. 立即上手

```bat
cd C:\Users\DINGJINJIN\Downloads\cross_point\crosspoint-win

:: 已装好工具链，直接构建（约 1-2 分钟增量，全量 4 分钟）
build.cmd

:: 运行（从仓库根目录，sdcard/ 在这里）
run_emulator.cmd
```

`build.cmd` 会：`prepare_firmware.cmd`（递归拉子模块 + 应用 `patches/*.patch`，当前没有补丁）→ CMake + Ninja →
把 `SDL2.dll` 放到 exe 旁边。产物：`build\crosspoint_emulator.exe`。

**离线/快速构建**（避免 CMake 再下一次解码库）：`build/` 的 CMakeCache 已指向 `C:\...\deps\{pngdec,jpegdec,animatedgif}`；
如果换了目录，configure 时传：

```
-DFETCHCONTENT_SOURCE_DIR_PNGDEC=.../deps/pngdec
-DFETCHCONTENT_SOURCE_DIR_JPEGDEC=.../deps/jpegdec
-DFETCHCONTENT_SOURCE_DIR_ANIMATEDGIF=.../deps/animatedgif
```

**验证改动（当前唯一可信的端到端验证）**

```powershell
# 1) 看 CI 是否绿
gh run list  --repo Dingjinjin-chihiro/crosspoint-win --branch latest-firmware --limit 3
gh run watch <run-id> --repo Dingjinjin-chihiro/crosspoint-win --exit-status

# 2) 本地跑起来，抓窗口确认画面（脚本在 scripts/）
powershell -File C:\Users\DINGJINJIN\Downloads\cross_point\scripts\upgrade_nav.ps1 `
  -ProcessId <pid> -Keys "ENTER,DOWN,ENTER" -Out shot.png -WaitMs 6000

# 3) 固件日志（见 DEBUGGING.md）
#    运行时的 stdout 就是 Serial 日志，格式 [millis] [LEVEL] [MODULE] message
```

---

## 3. 已完成的工作（按阶段）

### 阶段 A：让 1.0.0 跑起来（`main` 分支）
- 无系统 C++ 编译器 → 用 TUNA 镜像装便携 MSYS2（GitHub 太慢），装 MinGW GCC/CMake/Ninja/SDL2/Python
- 固件钉到 `1.0.0`（模拟器只兼容这个 HAL 代际；直接编主干会死在 `ContentProtection.h`、`Storage` 未声明）
- 修了 6 类编译问题（`mkdir`/`realpath` 兼容层、隐式包含、`String` 64 位重载歧义等）
- **修了一个真实 use-after-free**：`onGoToReader()` 在 `exitActivity()` 之后还用传入的 `std::string&`，
  该字符串通常属于被销毁的 Activity → Windows/MinGW 下稳定 `0xC0000005`（gdb 栈：`memmove`）。
  改成先拷贝。补丁在 `main` 的 `patches/0002-*.patch`
- 加了 CI（抓到两个只在 CI 出现的问题：`core.autocrlf` 破坏补丁、缺 Python 导致 HTML 头文件生成被吞）

### 阶段 B：升到 2.x（`latest-firmware` 分支第一次提交）
- 采用上游移植 PR（jonmooreai/Crosspoint-Emulator#6，未合并）：C++20、直接用固件 HAL 头、FreeInk SDK、
  CMake `FetchContent` 拉 PNGdec/JPEGDEC/AnimatedGIF
- 固件先钉 `c9cb1f8d`（2026-10-27）
- 修：ArduinoJson 改内置（`third_party/`），避免 PlatformIO 依赖；`.gitignore` 里的 `*.cmake` 把
  `cmake/apply_patches.cmake` 一起忽略了（CI 发现，文件根本没提交）

### 阶段 C：追到主干 HEAD（当前状态）
固件 `ca1a8c1`（2026-11-21）。这轮 HAL 变化导致的适配（**已全部完成并验证**）：

| 文件 | 改动 |
|---|---|
| `sim/src/sim_display.cpp` | `HalDisplay` 接口对齐 HEAD：删 `drawImageTransparent`/`refreshDisplay`/`copyGrayscaleBuffers`/`supportsStripGrayscale`/`supportsAsyncGrayscaleBase`/`combinesGrayscaleBase`/`toggleInverted`，加 `controllerIdle()`；`getController()` 返回 `BoardConfig::DisplayController::SSD1677` |
| `sim/include/EInkDisplay.h` | 加 `getControllerName`/`getPanelName`/`setReaderRefresh`/`setOverlayFullTone`/`displayRetainedGrayscaleOverlay` |
| `sim/include/SDCardManager.h`、`sim/src/sim_storage.cpp` | 加 `sdTotalBytes()`（32 GB 假容量）、`sdUsedBytes()`（递归统计 sdcard） |
| `sim/src/sim_hal.cpp` | 加 `HalPowerManager::readBatteryPercentage()` |
| `sim/src/net_stubs.cpp` | 删 `SecureClient::reserveFullRecord()`（SDK 已内联）；加 `freeink::hostResolves()`（恒 false） |
| `sim/include/mbedtls/sha256.h` | 换成真正 SHA-256（书签 sync id 依赖稳定哈希；原来是滚动哈希） |
| `sim/include/ArduinoStub.h` | Serial 每次写入 `fflush(stdout)` —— 否则重定向到文件时日志全丢 |

### 阶段 D：调试能力（最新提交 `10587c6`）
- 文档 `docs/DEBUGGING.md`
- 实测：`build-debug`（RelWithDebInfo）下 `break ActivityManager::goToReader` 命中，`bt` 出
  `FileBrowserActivity::activateSelected → Activity::onSelectBook → goToReader → main`，参数
  `path="/The Quiet Shelf.epub"` 可见
- 日志级别、Ctrl+S 截图、PostMessage 按键注入（方向键必须带 extended 位，脚本已处理）

---

## 4. 调试与观测能力（现状）

| 能力 | 怎么做 | 状态 |
|---|---|---|
| 断点/单步/调用栈 | `build-debug` + `tools\msys64\mingw64\bin\gdb.exe`；VS Code 的 `cppdbg` 指向同一 gdb | ✅ 实测 |
| 固件日志 | stdout，`[ms] [LEVEL] [MODULE] msg`；`LOG_LEVEL` 在 `CMakeLists.txt` | ✅ |
| 截图 | 运行中 `Ctrl+S` → `screenshots/*.bmp`（480×800） | ✅ |
| 键盘自动化 | `scripts/upgrade_nav.ps1 -Keys "ENTER,DOWN"` | ✅ |
| 性能 | 日志里 `[GFX] Time = N ms` | ✅ 但含宿主渲染时间 |
| 内存 | 日志里 `[MEM]` | ⚠️ **假数据**（固定 16 MB 桩） |
| 网络 | Wi-Fi/TLS/OPDS/OTA 全是桩 | ❌ 只能看 UI 流程 |
| 面板波形/功耗 | 无 | ❌ 必须真机 |

---

## 5. 下一阶段任务（用户明确要求）

### 5.1 触摸输入（优先级最高，落点已探明）

**好消息**：固件已经有完整的触摸抽象，模拟器只需要把鼠标喂进去，不用改固件 UI。

**编译开关**
- `FREEINK_CAP_TOUCH` 由所选设备决定：`BoardConfig.h:219-224`，可用 `-DFREEINK_CAP_TOUCH=1` 强制打开
- 触摸驱动按设备选：`FREEINK_TOUCH_GT911` / `FT5X06` / `CHSC6X` / `FT6336U` / `GSLX680` / `CST816S` / `TMA525C`
- 我们当前只选 `FREEINK_DEVICE_X4=1`（**X4 无触摸**）。两条路：
  1. 加 `-DFREEINK_CAP_TOUCH=1`，让 `InputManager::hasTouch()` 走我们自己的实现（最省事）
  2. 或把设备换成有触摸的 profile（`FREEINK_DEVICE_X4PRO=1`，X4 Pro 用 GT911），
     并在 sim 里 `BoardConfig::selectDevice(Board::...X4Pro)`（`ACTIVE` 是 `BoardConfig.h:2263` 的 inline 变量，
     `selectDevice()` 在 2267 行，可在运行时切）
- 当前 CMake 只定义 `CROSSPOINT_EMULATED=1`，**没有定义 `SIMULATOR`**；`MappedInputManager.h:62` 有 `#ifdef SIMULATOR` 分支，需要时一并加

**要在 sim 里实现的接口**（现在全是 return false 的桩，`sim/src/sim_gpio.cpp:113-129`）
```cpp
bool hasTouch() const;
bool wasTouchTap(float& nx, float& ny) const;         // 归一化 0..1
bool wasTouchDown(float& nx, float& ny) const;
bool wasTouchReleased() const;
bool isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const;
bool isTouchHeldAt(float& nx, float& ny) const;
bool wasTouchLongPress(float& nx, float& ny) const;
void suppressTouchContact();
unsigned long lastTouchHeldMs() const;
bool wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const;
bool wasTouchActivity() const;
bool hasHomeKey() const;  bool wasHomeKeyPressed() const;  bool wasHomeKeyTapped() const;
```

**实现思路**
1. `sim_gpio.cpp` 再加一条输入通道：SDL 鼠标 `MOUSEBUTTONDOWN/UP`、`MOUSEMOTION`，把窗口坐标归一化
2. 用时间阈值分类：按下→抬起且移动小 = Tap；按下后移动超阈值 = Swipe；按住超时 = LongPress
3. 维护 `lastState/prevState` 的边沿语义（与现有按钮实现一致，参考 `sim_gpio.cpp:36-95`）
4. 屏幕坐标：窗口是 480×800 竖屏，固件触摸配置用面板坐标（多数 profile 是 799×479 横向），
   要在 `HalGPIO` 层做一次归一化，让 `MappedInputManager` 的 `decodeSwipe`/`rowTouch` 拿到正确方向
5. 验证：`LibraryListActivity` 里点一行 → 打开书；`EpubReaderActivity` 左滑翻页；顶部下滑开控制中心

**验收**：对着 `MappedInputManager` 的 `rowTouch()`/`wasSwipe()` 写 3 个手动场景，截图 + 日志确认，
然后让 CI 编过（触摸代码路径要进 CI 编译）。

### 5.2 界面美化
- 固件 UI 在 `Crosspoint/src/activities/**` + `freeink-sdk/libs/ui/FreeInkUI`
- 主题：日志里能看到 `[UI] Using Lyra theme`；`UITheme::getInstance()`
- 建议先做**模拟器侧**的观感改进（窗口缩放、整数倍缩放、抗锯齿/灰度映射），再做**固件主题**（颜色/边距/图标）
- 注意：改固件 UI 属于"我们的 fork"，要在 `emulator-latest` 上另开分支并回推到 fork，别混进 `main`

### 5.3 代码优化
- 现有可测点：日志 `[GFX] Time`、`[MEM]`（假）、`--build-debug` 下 perf 采样
- 优化前先补**可重复的基准**：固定 sdcard 内容 + 固定按键序列（用 `upgrade_nav.ps1`），记录 `[GFX]` 时间序列
- 热点候选：`render_bw_to_texture`（逐像素，可 SIMD/批量）、EPUB 分页缓存、缩略图预生成
- 红线：不要为了优化改坏 `sim_display` 的旋转映射（`rotX/rotY` 一旦错位，画面会旋转 90°，很难一眼看出）

### 5.4 调试能力增强（用户也提到"能进行调试"）
- 加 `.vscode/launch.json` + `.vscode/tasks.json`（F5 直接断点），gdb 路径 `tools\msys64\mingw64\bin\gdb.exe`
- 加 `build_debug.cmd`，别让用户手敲 cmake
- 可选：模拟器内叠加调试 HUD（FPS/当前 Activity/heap 假值），只在 `CROSSPOINT_EMULATOR_HUD=1` 时显示
- 可选：把按键/触摸注入做成"脚本回放"（录制 SDL 事件 → 重放），便于 UI 回归

---

## 6. 坑与红线（都踩过，别再踩）

1. **不要 `git add -A` 前不检查 `.gitignore`**：`*.cmake` 曾吞掉 `cmake/apply_patches.cmake`，CI 才暴露
2. **Windows CI 必须 `core.autocrlf=false`**：否则固件源码变 CRLF，`git apply` 与 `patch_animatedgif.py` 全失败
3. **固件自带的 JPEGDEC 补丁是 CRLF**：直接 `git apply` 会报 `patch does not apply`，要先转 LF
4. **`freeink-sdk` 是二级子模块**：`submodule update --init --recursive`，否则缺 `libs/`
5. **`.pio` 目录不入库**：ArduinoJson 必须走 `third_party/`（已在 CMake 里做回退）
6. **改固件不要在子模块里直接提交**：先在我们的 fork 上建分支推上去，再改 gitlink；否则 CI checkout 不到
7. **推 `.github/workflows/**` 需要 `workflow` scope**：已授权；换账号要重新 `gh auth refresh -h github.com -s workflows`
8. **别用 `SendKeys`**：SDL 收不到；必须 `PostMessage`，方向键 lParam 要带 `1<<24`（extended）
9. **日志要看得到必须 flush**：已修；如果以后又"没有日志"，先怀疑缓冲
10. **`build-debug` 的 exe 有 144 MB**：别提交、别打包进 artifact

---

## 7. 新会话开工检查清单

```
[ ] cd C:\Users\DINGJINJIN\Downloads\cross_point\crosspoint-win
[ ] git status && git branch --show-current      # 应该在 latest-firmware，工作区干净
[ ] git log --oneline -3                          # 最新应是 10587c6（或之后的提交）
[ ] git submodule status                          # Crosspoint 应为 ca1a8c1...（前面无 "-"）
[ ] build.cmd                                     # 增量构建应通过
[ ] run_emulator.cmd                              # 窗口能起来，能打开 sdcard 里的 EPUB
[ ] gh run list --repo Dingjinjin-chihiro/crosspoint-win --branch latest-firmware --limit 1   # CI 绿
```

如果子模块显示 `-` 开头（未初始化）：`git submodule update --init --recursive`。

---

## 8. 关键文件索引

| 想改什么 | 去哪 |
|---|---|
| 显示/旋转/截图 | `sim/src/sim_display.cpp` |
| 按键/（未来的）触摸 | `sim/src/sim_gpio.cpp`、`sim/include/EInkDisplay.h` |
| SD 卡模拟 | `sim/src/sim_storage.cpp`、`sim/include/SDCardManager.h` |
| 固件 HAL 之外的空实现 | `sim/src/sim_hal.cpp`、`sim/src/net_stubs.cpp` |
| 编译开关/依赖 | `CMakeLists.txt`（`target_compile_definitions` 里有 `FREEINK_DEVICE_X4`） |
| 固件 UI/主题 | `Crosspoint/src/activities/**`、`Crosspoint/freeink-sdk/libs/ui/FreeInkUI` |
| 触摸抽象 | `Crosspoint/lib/hal/HalGPIO.h:96-128`、`Crosspoint/src/MappedInputManager.h:36-200` |
| 设备/触摸配置 | `Crosspoint/freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h`（219-260 触摸开关，512 控制器枚举，2263 `ACTIVE`） |
| CI | `.github/workflows/build.yml` |
| 调试手册 | `docs/DEBUGGING.md` |

---

## 9. 给下一个会话的建议顺序

1. 跑一遍第 7 节检查清单（确认基线可复现）
2. **先做触摸**（5.1）：它是"能不能交互"的分水岭，而且落点最清晰
3. 触摸通了再谈美化（5.2）——否则每改一次 UI 都要用键盘验证，效率低
4. 优化（5.3）放最后，且先补基准
5. 每完成一块：本地截图验证 → 提交 → 推 → 等 CI 绿 → 再进下一块
