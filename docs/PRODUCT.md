# Crosspoint for Windows —— 产品说明

> 本文回答两个问题：**这个产品是什么**，以及**它和原版固件（跑在真机上的那份）到底有什么区别**。
> 面向想用它做开发/调试/执行流程分析的人。原理细节见 [DEBUGGING.md](DEBUGGING.md)，进度交接见 [PROGRESS.md](PROGRESS.md)。

---

## 1. 这是什么

**在 Windows 上运行 Crosspoint 电子书固件，不需要真机、不需要烧录。**

一句话原理：**不是模拟 ESP32 芯片，而是把真固件用本机编译器重新编译成 Windows 程序，再用一套「模拟硬件层」替换硬件接口。**
所以跑的是**同一份固件 C++ 代码**，只是它底下面对的不是 SPI/I2C/GPIO，而是 SDL2 窗口、键盘鼠标、硬盘目录。

三个定位：

| 它是什么 | 它不是什么 |
|---|---|
| **源码级可调试的固件运行环境**：gdb 断点 / 单步 / 调用栈、函数级追踪 | ❌ 不是 CPU/指令级模拟器，没有 ESP32 寄存器与外设时序 |
| **UI 与业务逻辑的快速迭代工具**：改代码 → 秒级重编 → 立刻看到 | ❌ 不是周期精确的仿真，没有面板波形/刷新时序 |
| **执行流程分析工具**：不用刷真机读串口，就能看完整调用树 | ❌ 不是硬件验证平台，功耗/网络/触摸硬件必须上真机 |

---

## 2. 它解决了什么问题

原版工作流：改固件 → 烧录到 Xteink X4 → 串口看日志 → 猜。一次迭代几分钟到十几分钟，且只能看日志，看不到变量和调用栈。

本产品把这条链路变成：

| 场景 | 原版（真机） | 本产品（模拟器） |
|---|---|---|
| 看某个函数被谁调用 | 加 LOG → 烧录 → 读串口 | **gdb 断点** 或 **函数级追踪**，直接出调用树 |
| 看某次执行的变量 | 加 LOG → 烧录 | gdb p 变量，改完继续 |
| 复现某个交互 | 手动点屏幕 | 脚本化（CROSSPOINT_TOUCH_SCRIPT）可重复 |
| 改 UI 看效果 | 烧录 → 等面板刷新 | 重编 → 秒级看到 |
| 分析整体执行流程 | 只能靠日志拼 | build_trace.cmd + 调用树 |

---

## 3. 组成与版本

```
cross_point/
├─ crosspoint-win/            ← 本仓库（模拟器 + 构建脚本 + 调试工具）
│  ├─ sim/                    ← 模拟器内核（HAL 的 Windows 实现 + 假平台头文件）
│  ├─ Crosspoint/             ← 固件子模块（我们的 fork，指向 emulator-latest）
│  │  └─ freeink-sdk/         ← 二级子模块（板级 profile + HAL 驱动 + UI 库）
│  ├─ sdcard/                 ← 虚拟 SD 卡（普通目录，放书/缓存/设置）
│  ├─ scripts/                ← 触摸/截图/基准/追踪分析脚本
│  └─ docs/                   ← 文档
└─ tools/msys64/              ← 便携工具链（GCC 16.2 / CMake / Ninja / SDL2 / gdb）
```

| 项 | 值 |
|---|---|
| 分支 | latest-firmware |
| 上游固件 | **ca1a8c1（2026‑11‑21，上游主干 HEAD）** |
| 固件 fork | Dingjinjin-chihiro/crosspoint-reader，分支 emulator-latest |
| 模拟器内核来源 | 上游移植 PR jonmooreai/Crosspoint-Emulator#6（未合并）+ 我们对 HEAD 的适配 |
| 版本号 | 2.0.0-emu |
| 语言标准 | C++20 |
| 许可 | 模拟器与固件均 MIT（原始版权见 NOTICE） |

> main 分支是旧版（原版模拟器 + 固件 1.0.0），链路稳定但不更新。所有新工作都在 latest-firmware。

---

## 4. 工作原理

### 4.1 分层结构

```
┌──────────────────────────────────────────────────────────┐
│ Crosspoint/src/activities/**   界面与业务逻辑                │  ← 真固件代码
│ Crosspoint/lib/**  (Epub/GfxRenderer/Logging/LibraryIndex…) │    同一份源码
│   └─ 只依赖下面的 HAL 抽象，不直接碰硬件                     │
├──────────────────────────────────────────────────────────┤
│ Crosspoint/lib/hal/  ← 硬件抽象接口（10 个）                │  ← 固件与硬件的边界
│   HalDisplay / HalGPIO / HalStorage / HalClock /             │
│   HalPowerManager / HalFrontlight / HalHaptics /             │
│   HalMemory / HalSystem / HalTiltSensor                      │
├────────────────────────────┬─────────────────────────────┤
│ 真机：ESP32 驱动             │ 模拟器：sim/src/*.cpp          │
│ SPI 刷屏 / I2C 触摸 / SDMMC  │ SDL2 窗口 / 鼠标 / sdcard 目录   │
└────────────────────────────┴─────────────────────────────┘
```

### 4.2 关键机关：假头文件

固件写的是 #include <Arduino.h>、<WiFi.h>、<SdFat.h>、<Wire.h>……
模拟器把 sim/include/ 放在头文件搜索路径前面，于是 include 到的是模拟实现：

Arduino.h / ESP.h / WiFi.h / WiFiClientSecure.h / SdFat.h / FS.h /
FreeRTOSStub.h / Wire.h / SPI.h / WebServer.h / esp_*.h / nvs.h /
Preferences.h / MD5Builder.h / WolfsslCrypto.h …

对应实现：arduino_stub.cpp、esp_stub.cpp、freertos_stub.cpp（FreeRTOS→pthread）、
wifi_stub.cpp、net_stubs.cpp、battery_stub.cpp 等。

### 4.3 一个动作的完整链路

「鼠标点一下图书馆里的书」：

```
SDL mouse event
  → sim_display_pump_events()     按 windowID 路由 + 整数倍/信箱坐标反算
  → sim_gpio_on_pointer()         归一化成面板坐标，喂进触摸状态机
  → g_touch 状态机                分类 tap / swipe / long-press（阈值抄 SDK InputManager）
  → HalGPIO::wasTouchTap(nx,ny)   固件读到"手指点了"
  → MappedInputManager::wasScreenTapped()   renderer.tapToLogical() 转逻辑坐标
  → LibraryListActivity::rowTouch()        命中第几行
  → activateSelected() → ActivityManager::goToReader() → EpubReaderActivity
```

**除了最底下两行（SDL/触摸），上面全是真固件代码。** 这就是 gdb 能断在 goToReader、
追踪能录到完整调用树的原因——它们是本机原生函数，不是"模拟"出来的。

---

## 5. 与原版固件的差异（核心）

> "原版固件" = 烧进真机 Xteink X4 的那份固件。它和模拟器跑的是**同一份源码**，
> 差异全部发生在 **HAL 之下** 和 **编译选择** 上。

### 5.1 版本关系：源码一行不改

| 事实 | 证据 |
|---|---|
| 固件 app 层（Crosspoint/src/**）**没有任何模拟器分支** | 整个 src/ 里 CROSSPOINT_EMULATED 出现 **0 次** |
| 只有 2 个 HAL 头文件带模拟分支 | HalGPIO.h 2 处（真机才有的 InputManager 成员）、HalHaptics.h 3 处（模拟时马达 no-op） |
| 固件版本与上游一致 | 子模块指向 fork 的 emulator-latest = 上游 ca1a8c1 |

**结论：你在模拟器里调试/追踪到的业务逻辑，就是真机上跑的逻辑。**

### 5.2 编译差异：哪些真、哪些假、哪些没有

| 类别 | 内容 |
|---|---|
| **原样编译的真固件** | Crosspoint/src/** 全部（所有 Activity、ActivityManager、MappedInputManager、CrossPointSettings…）；Crosspoint/lib/** 除 HAL 外（Epub、GfxRenderer、Logging、I18n、LibraryIndex、ZipFile、xmltok、miniz、uzlib、OtShaper、MiniBidi、各种 *ToBmpConverter、KOReaderSync、TrustedTime…）；FreeInk SDK 的 8 个纯逻辑库（ui/FreeInkUI、network/JsonSax、network/CatalogList、network/Opds、book/ContentProtection、hardware/MemoryManager、font/FreeInkFont 含 FreeType）；5 个 HAL 实现原样跑（HalStorage、HalClock、HalMemory、HalFrontlight、HalTiltSensor） |
| **被模拟器替换的 HAL** | HalDisplay.cpp → sim_display.cpp（SDL 窗口）；HalGPIO.cpp → sim_gpio.cpp（键盘/鼠标/手势）；HalPowerManager.cpp → sim_hal.cpp（假电量）；HalSystem.cpp → sim_hal.cpp（空实现）；HalHaptics → 头文件内联 no-op |
| **被排除 + 桩掉的真固件** | network/CrossPointWebServer.cpp、WebDAVHandler.cpp → crosspoint_web_server_stub.cpp（服务器永不启动）；network/OtaUpdater.cpp → ota_updater_stub.cpp（OTA 不存在）；lib/Epub/BookKey.cpp（DRM 需 wolfSSL）→ net_stubs.cpp 桩 |
| **假头文件** | sim/include/ 整套 Arduino / ESP-IDF / FreeRTOS / WiFi / SdFat / Wire / SPI / nvs / Preferences |
| **编译开关** | CROSSPOINT_EMULATED=1、FREEINK_DEVICE_X4=1、FREEINK_CAP_TOUCH=1、LOG_LEVEL=2、EINK_DISPLAY_SINGLE_BUFFER_MODE=1、CROSSPOINT_VERSION="2.0.0-emu" |

### 5.3 运行行为差异（逐模块）

| 模块 | 真机 | 模拟器 | 你能信什么 |
|---|---|---|---|
| **显示** | SSD1677，SPI 刷屏，有波形/刷新时序/残影/局部刷新 | SDL2 纹理，立即呈现；帧缓冲 800×480 1-bit 旋转成 480×800 RGB24 | ✅ 画面内容/布局/排版逻辑；❌ 刷新耗时、闪烁、残影、功耗 |
| **输入（按键）** | ADC 电阻梯 / 数字按键（X4） | SDL 键盘（←↑→↓ / Enter / Backspace·Esc / P） | ✅ 按键→动作映射逻辑 |
| **输入（触摸）** | GT911 I2C，真实坐标/压力/多指 | 鼠标 + 手势注入（tap/swipe/long-press），坐标归一化后送固件 | ✅ 手势分类/命中判定逻辑（阈值抄 SDK）；❌ 真实触摸硬件行为 |
| **输入（Home 键/滑盖）** | X4 Pro 有电容 Home 键 | hasHomeKey()=false（X4 body 无 Home 键） | 相关功能走"无 Home 键"分支 |
| **存储** | SD 卡 SDMMC/FAT | 主机目录 sdcard/ | ✅ 调用流程/API 语义/持久化；❌ 速度、块级读取、卡拔出/读错误、文件系统差异 |
| **电源** | BQ27220 电量计、深睡、唤醒 | 固定 100%，深睡是个空函数 | ❌ 电量/休眠/唤醒逻辑 |
| **时钟** | DS3231 RTC | 系统时间 | ⚠️ 时间相关逻辑用真实系统时间，但没有 RTC 芯片行为 |
| **前光 / IMU / 马达** | 真实外设或桩 | no-op 桩 | ❌ 相关功能不可用 |
| **网络** | WiFi / TLS / HTTP / OPDS / Calibre / OTA | 全桩（DNS 预检恒 false、连接必失败） | ✅ UI 流程与错误处理分支；❌ 真实协议/证书/重试 |
| **NVS / Preferences** | Flash 持久 | **纯内存**（每次启动重置） | ⚠️ 设置走文件持久化（sdcard/.crosspoint/），但存 NVS 的会重置 |
| **USB 大容量存储** | 真 USB MSC | 桩 | ❌ 不可用 |
| **内存数字** | 真实 heap | 固定 8MB/16MB 桩 | ❌ 不能用来判断真机 OOM |

### 5.4 模拟器独有的能力（原版固件没有）

这些是**本产品在模拟器侧新加的东西**，真机上不存在：

| 能力 | 用法 | 说明 |
|---|---|---|
| **鼠标触摸** | 移动 + 左键 = 手指 | 真机只有电容屏 |
| **手势热键** | F1 tap / F2 double / F3 long / F4–F7 四向滑 | 鼠标只负责坐标，时序由模拟器保证（tap 恒 80ms、滑动恒 260ms） |
| **手势面板** | 独立窗口 7 个按钮 | 点击即按固定时序触发；按下即触发（规避 SDL SetCapture 漂移） |
| **脚本化交互** | CROSSPOINT_TOUCH_SCRIPT="wait 1200; tap 232,421; swipe ..." | 可重复的交互序列，坐标是逻辑竖屏像素 |
| **函数级执行追踪** | build_trace.cmd + CROSSPOINT_TRACE=1 + scripts/trace_analyze.py | -finstrument-functions 记录每个函数进出，解析成调用树/自耗时。实测 3.4s 交互 294 万条记录、3175 个函数 |
| **gdb / VS Code 断点** | F5（.vscode/launch.json）或 gdb --args build-debug/... | 断点/单步/调用栈/变量；含 tracepoint 配方（自动打栈继续跑） |
| **调试 HUD** | CROSSPOINT_EMULATOR_HUD=1 | 面板左上角 FPS 读数 + 每秒 [HUD] fps=N |
| **可缩放窗口** | 拖动窗口边缘 | 480×800 面板整数倍放大、居中、信箱；最近邻不糊 |
| **面板精确截图** | Ctrl+S / 脚本 shot | 永远输出 480×800，与窗口缩放/DPI 无关 |
| **PNG 转换** | scripts/shots_to_png.ps1 | BMP → PNG 便于查看 |
| **可重复基准** | scripts/bench_gfx.py + make_bench_book.py | [GFX]/[ERS] 时间序列统计 |
| **三套构建** | build.cmd（Release）/ build_debug.cmd（带符号）/ build_trace.cmd（追踪） | 都进独立目录，互不影响 |

### 5.5 原版有、模拟器没有的

| 缺失 | 影响 |
|---|---|
| 面板波形 / 刷新时序 / 残影 / 局部刷新 | 功耗、闪烁、残影类问题**必须真机** |
| 真实 WiFi / TLS / HTTP / OPDS / OTA | 协议、证书、下载、升级全部无法验证 |
| 真实电量 / 深睡 / 唤醒 / 电源管理 | 电池与休眠行为无法验证 |
| USB 大容量存储（USB Drive 模式） | 不可用 |
| GT911 触摸真实坐标/多指/压力 | 只能用模拟坐标 |
| 前光 / IMU / 马达 | 相关功能不可用 |
| SD 卡失败模式（拔出/错误/写保护/卡满） | 无法验证异常处理 |
| CPU 周期 / 中断 / FreeRTOS 任务优先级调度 | 时序与并发问题只能在真机观察 |

---

## 6. 快速上手

### 6.1 构建与运行

```bat
git clone --recursive -b latest-firmware https://github.com/Dingjinjin-chihiro/crosspoint-win.git
cd crosspoint-win

setup_toolchain.cmd   :: 首次：便携 MSYS2 + GCC/CMake/Ninja/SDL2/Python/gdb（约 5 分钟）
build.cmd             :: Release 构建（-j 12）
run_emulator.cmd      :: 启动
```

### 6.2 调试

```bat
build_debug.cmd       :: RelWithDebInfo + 符号，产物 build-debug/
:: VS Code 按 F5，或：
gdb --args build-debug\crosspoint_emulator.exe
(gdb) break ActivityManager::goToReader
(gdb) run
```

### 6.3 执行流程追踪

```bat
build_trace.cmd
set CROSSPOINT_TRACE=1
set CROSSPOINT_TOUCH_SCRIPT=wait 1200; tap 232,421; wait 2500
build-trace\crosspoint_emulator.exe            :: 产出 trace.log
python scripts\trace_analyze.py trace.log build-trace\crosspoint_emulator.exe --top 20 --tree
```

### 6.4 脚本化交互

```bat
:: 直接喂时序
set CROSSPOINT_TOUCH_SCRIPT=wait 1200; tap 232,421; wait 2600; swipe 400,400 100,400 300

:: 或用脚本驱动面板/热键
powershell -File scripts\emu_touch.ps1 -Actions "move:232,421; panel:0; wait:2500" -Out shot.png
```

---

## 7. 已知限制（一句话版）

- **网络全是桩**：Wi‑Fi/TLS/OPDS/插件/OTA 只能看 UI 流程。
- **内存数字是假的**：不能判断真机 OOM。
- **不是周期精确**：没有面板波形/SPI 时序，功耗与残影必须真机。
- **没有 Home 键 / 前光 / IMU / 马达 / USB MSC**。
- **NVS 纯内存**：每次启动重置（文件持久化的设置不受影响）。
- **TRACE 构建慢约 10×、trace.log 很大**（3M 条 ≈ 58MB），仅用于分析。

---

## 8. 文件索引

| 想看什么 | 去哪 |
|---|---|
| 模拟器入口 | sim/src/main_sim.cpp |
| 显示/旋转/截图/缩放 | sim/src/sim_display.cpp |
| 输入（按键/鼠标/触摸/手势注入） | sim/src/sim_gpio.cpp |
| 手势面板 | sim/src/sim_panel.cpp |
| SD 卡模拟 | sim/src/sim_storage.cpp + sim/include/SdFat.h、FS.h |
| 电源/系统/RTC/IMU/前光桩 | sim/src/sim_hal.cpp |
| 函数追踪 hook | sim/src/sim_trace.cpp |
| 假平台头文件 | sim/include/** |
| 构建选择与开关 | CMakeLists.txt |
| 固件 HAL 接口 | Crosspoint/lib/hal/** |
| 板级 profile | Crosspoint/freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h |
| 调试手册（含坑与限制） | docs/DEBUGGING.md |
| 进度与后续计划 | docs/PROGRESS.md |
| 脚本（触摸/截图/基准/追踪） | scripts/ |
