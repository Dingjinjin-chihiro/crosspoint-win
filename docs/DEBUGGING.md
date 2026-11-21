# 调试这份模拟器

结论先说：**能调试，而且是在固件源码层面调试** —— 断点、单步、调用栈、变量、条件断点都能用；
固件自己的分级日志、面板级截图、性能计时也能直接看。但它不是硬件仿真器，网络/内存/波形这些是桩，
不能用来验证这些子系统的真实行为（详见文末"不能做什么"）。

下面前四条都在这台机器上实测过。

---

## 1. 源码级断点（gdb）

先编一个带符号的版本（Release 是 `-O3 -DNDEBUG`，没有行号信息）：

```bat
tools\msys64\usr\bin\bash.exe -lc "export PATH=/mingw64/bin:$PATH; export MSYSTEM=MINGW64; ^
  cd /c/path/to/crosspoint-win; cmake -S . -B build-debug -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCROSSPOINT_ROOT=$PWD/Crosspoint && ^
  cmake --build build-debug -j 8"
```

然后把 `build-debug\crosspoint_emulator.exe` 和 `SDL2.dll` 放到一个带 `sdcard\` 的目录里，用 gdb 起：

```
break ActivityManager::goToReader
run
```

实测输出（按 ENTER 打开 EPUB 时命中断点）：

```
Thread 1 hit Breakpoint 1, ActivityManager::goToReader
    (this=0x7ff6948eede0 <activityManager>, path="/The Quiet Shelf.epub",
     allowFastInitialRefresh=false)
    at .../Crosspoint/src/activities/ActivityManager.cpp:328
328  void ActivityManager::goToReader(std::string path, const bool allowFastInitialRefresh) {

#0  ActivityManager::goToReader (...)      ActivityManager.cpp:328
#1  Activity::onSelectBook (path="/The Quiet Shelf.epub")   Activity.cpp:15
#2  FileBrowserActivity::activateSelected (...)             FileBrowserActivity.cpp:380
#3  FileBrowserActivity::handleButtons (...)                FileBrowserActivity.cpp:561
#4  UiListActivity::loop (...)                              UiListActivity.cpp:175
#5  ActivityManager::loop (...)                             unique_ptr.h:192
#6  loop ()                                                 main.cpp:1173
#7  main (...)                                              main_sim.cpp:129
```

常用操作：

| 操作 | gdb 命令 |
|---|---|
| 按函数名下断点 | `break ReaderActivity::create` |
| 按文件:行下断点 | `break EpubReaderActivity.cpp:120` |
| 条件断点 | `break FileBrowserActivity.cpp:380 if path.find("epub") != std::string::npos` |
| 单步 | `next` / `step` / `finish` |
| 看变量 | `print path`、`info locals`、`info args` |
| 调用栈 | `bt 20` |
| 看线程 | `info threads`（固件有 FreeRTOS 任务，模拟器里是真线程） |

VS Code 也能用：`cppdbg` + `miDebuggerPath` 指向 `tools\msys64\mingw64\bin\gdb.exe`，
`program` 指向 `build-debug\crosspoint_emulator.exe`。

> 中文字体/字形相关问题只能在断点里看数据，屏幕上看到的是位图渲染结果，没有字形级调试视图。

## 2. 固件自己的日志（已修好缓冲问题）

CMake 里已定义 `ENABLE_SERIAL_LOG=1`、`LOG_LEVEL=2`，固件各处 `LOG_ERR/LOG_INF/LOG_DBG` 直接打到 stdout：

```
[160] [ERR] [GYR] SDK IMU not found
[166] [DBG] [UI] Using Lyra theme
[169] [DBG] [MAIN] Starting CrossPoint version 2.0.0-emu
[172] [DBG] [SDREG] Fonts directory not found: /.fonts
[191] [DBG] [ACT] Entering activity: Home
[6149] [DBG] [ACT] Exiting activity: Home
[6150] [DBG] [ACT] Entering activity: FileBrowser
[6150] [DBG] [GFX] Time = 0 ms from clearScreen to displayBuffer
[10005] [INF] [MEM] Free: 8388608 bytes, Total: 16777216 bytes ...
```

格式是 `[millis] [LEVEL] [MODULE] message`，直接把 stdout 重定向到文件就能 `grep [ERR]`。

> 我在 `sim/include/ArduinoStub.h` 里给 Serial 的每次写入加了 `fflush(stdout)`。不加的话，
> stdout 被重定向到文件时是块缓冲，而模拟器通常是被强杀的，日志会整段丢失（之前一直是空的）。

想调日志级别就改 CMake 里的 `LOG_LEVEL`：`0` 只留错误，`1` 加信息，`2` 全量。

## 3. 面板级截图

窗口现在是**可缩放**的：480×800 面板按**整数倍**放大、居中，其余区域留边框（最近邻放大，像素不糊）。
无论窗口拉多大，鼠标/触摸都按同一套「整数倍 + 信箱」变换映射回面板像素。

运行中按 **Ctrl+S**（macOS 上 Cmd+S），会在**工作目录**下写 `screenshots/crosspoint-<时间戳>.bmp`：
它通过渲染目标抓取，**永远是面板原始 480×800**，与窗口缩放、边框、系统 DPI 无关。适合做 UI 回归对比。
BMP 转 PNG（便于直接查看）：`powershell -NoProfile -ExecutionPolicy Bypass -File scripts\shots_to_png.ps1`。

**调试 HUD**：设 `CROSSPOINT_EMULATOR_HUD=1` 启动，面板左上角叠加一个 FPS 读数（每秒向 stdout
打一行 `[HUD] fps=N`）。它是纯模拟器侧叠加，固件看不到。注意这里的 FPS 是「屏幕重绘次数/秒」：
e-ink 静态画面不重绘，所以空闲时是 0，只有固件真正刷屏时才会涨——这本身也是有用的观测。

## 5.5 手势面板（操作面板）

启动时会在屏幕左上角开一个独立的 **Gesture Panel** 窗口，7 个按钮：

| 按钮 | 动作 | 时序（由模拟器保证，鼠标抖动不影响） |
|---|---|---|
| TAP | 点按 | 80 ms |
| DBL | 双击 | 两次 80 ms，间隔 150 ms |
| HOLD | 长按 | 700 ms（按下 500 ms 时触发长按事件） |
| < / > / ^ / v | 左/右/上/下滑 | 260 ms，8 段插值 |

用法：**把鼠标移到主窗口的目标位置（只悬停不点）**，再点面板上的按钮——手势就在那个位置、
按上面的固定时序触发。这跟 F1–F7 热键等价，但更直观；也是 `CROSSPOINT_TOUCH_SCRIPT`
之外又一条稳定时序的触发通道。按钮是「按下即触发」（SDL 按下时会 SetCapture，导致抬起事件
坐标漂到窗口外，所以不依赖抬起坐标）。

日志会打 `[PANEL] button N '标签' fired`，随后是 `[TOUCH] inject gesture #N at (x,y)` 与
`release ... held=...ms`。脚本化验证用 `scripts/emu_touch.ps1` 的 `panel:N` 动作（0=TAP…6=下滑）。

```
[SHOT] screenshots/crosspoint-20261122-004012.bmp (480x800)
```

## 4. 键盘自动化（脚本驱动）

`scripts/upgrade_nav.ps1` 通过 `PostMessage` 向窗口注入按键，不抢焦点、可脚本化：

```powershell
powershell -File scripts\upgrade_nav.ps1 -ProcessId 1234 -Keys "ENTER,DOWN,ENTER" -Out shot.png -WaitMs 6000
```

注意方向键必须带 **extended-key 标志**（lParam bit 24），否则 SDL 会把 ↑↓←→ 解析错 ——
这个坑我踩过一次，脚本里已经处理。

## 5. 触摸输入：鼠标、手势热键、脚本

模拟器把面板当作一个**电容触摸面**：`CMakeLists.txt` 里强制 `FREEINK_CAP_TOUCH=1`，
`sim/src/sim_gpio.cpp` 用和固件 InputManager 完全相同的阈值与状态机实现 `HalGPIO` 的触摸接口，
所以固件里所有 `#if FREEINK_CAP_TOUCH` 的 UI 路径（列表点击、阅读器滑动翻页、边缘手势）都能走。

**坐标**：脚本和热键用的是**逻辑竖屏像素**（x 0..479，y 0..799），和截图上的坐标一致。
内部会按显示旋转的逆变换换算成 800×480 面板坐标再归一化，`GfxRenderer::tapToLogical()` 再转回逻辑坐标。

### 5.1 直接用鼠标

移动鼠标对准目标，左键按下/抬起就是手指按下/抬起。阈值与 SDK 一致：点按 28 px、
滑动 60 px / 700 ms 内、长按 500 ms。

> 已知宿主侧限制：SDL 在按键按下时会 `SetCapture()`，Windows 随后补发一条
> 带**真实光标位置**的 `WM_MOUSEMOVE`。如果用 PostMessage 合成点击（光标不在窗口里），
> 这条补发会被当成一次拖动。用真鼠标没有这个问题，脚本化请走 5.2/5.3。

### 5.2 手势热键（坐标来自指针，时序由模拟器保证）

把鼠标移到目标上（**只悬停、不点击**），然后按：

| 键 | 动作 |
|---|---|
| F1 | tap（点按） |
| F2 | double tap（两次点按，间隔 150 ms） |
| F3 | long press（按住 700 ms，始终由模拟器计时） |
| F4 / F5 | swipe left / right（从指针处起，220 px） |
| F6 / F7 | swipe up / down |

热键的价值在于**时序不进宿主事件队列**：tap 永远是 80 ms 的点按、长按永远是 700 ms，
不会因为宿主卡顿变成拖动。日志里能看到 `[TOUCH] inject gesture #N at (x,y)`。

### 5.3 脚本化（可复现，用于验收/回归）

`CROSSPOINT_TOUCH_SCRIPT` 指定一串动作，启动后 400 ms 开始按绝对时间执行：

```bat
set CROSSPOINT_TOUCH_SCRIPT=wait 1200; shot; tap 232,421; wait 2600; tap 147,189; wait 6000; swipe 400,400 100,400 300
build\crosspoint_emulator.exe
```

| 指令 | 含义 |
|---|---|
| `wait MS` | 等待 |
| `shot` | 写一张 `screenshots/*.bmp`（和 Ctrl+S 同一路径），用于把截图对齐到脚本时序 |
| `tap X,Y` / `double X,Y` | 点按 / 两次点按 |
| `long X,Y[,MS]` | 长按（默认 700 ms） |
| `swipe X1,Y1 X2,Y2[,MS]` | 滑动（默认 260 ms） |

被注入的步骤走的是和真实鼠标同一个 `pointerEvent()` 入口，所以两条通道的分类结果完全一致。
**这是当前唯一可复现的触摸验收方式**：一次运行就能在日志里看到
`Home → Library → EpubReader` 的 activity 迁移、翻页后的章节标题变化，以及边缘手势打开的工具栏。

脚本化鼠标/热键的辅助脚本是 `scripts/emu_touch.ps1`：

```powershell
# 移动指针 → 按 F1（在指针处点一下）→ 再按 F3（在指针处长按）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\emu_touch.ps1 ^
  -Actions "move:232,421; wait:200; fkey:1; wait:2500; move:147,189; fkey:1" ^
  -Out shot.png -WaitMs 1500
```

## 6. 性能观测

`[GFX] Time = N ms from clearScreen to displayBuffer` 是每次刷新的耗时统计，可以直接看出
某个界面是不是变慢了。注意它包含宿主渲染时间，不等于真机波形时间。

阅读器有更细的 `[ERS] Page render (tiled async)` 分解，能直接区分「固件排版/栅格化」和
「模拟器上屏」：`bw_render` / `gray_render` 是模拟器的纹理上屏（0-2 ms），`gray_display`
才是占大头的一项（5-17 ms，主要是垂直同步的 present 等待，不是渲染本身）。

## 7. 性能基准

**可重复基准**（5.3 的第一步）：固定序列 + 大书，用 `CROSSPOINT_TOUCH_SCRIPT` 驱动，
`scripts/bench_gfx.py` 汇总日志。

```bat
:: 生成一本足够长、能翻很多页的基准书（派生产物，不入库）
python scripts\make_bench_book.py "sdcard\Benchmark Book.epub" 5 40

:: 跑固定序列（开书 + 连翻 8 页），stdout 落盘
set CROSSPOINT_TOUCH_SCRIPT=wait 1200; tap 232,421; wait 3200; tap 147,189; wait 8000; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300; wait 1200; swipe 400,400 100,400 300
build\crosspoint_emulator.exe > build\emu-bench.log 2>&1

:: 汇总
python scripts\bench_gfx.py build\emu-bench.log
```

**基线结论（大书 5 章 × 40 段）**

| 指标 | 数值 | 说明 |
|---|---|---|
| 模拟器纹理上屏 | `bw_render=0ms`、灰度上屏 0-1ms | **不是热点**（`render_bw_to_texture` 已做 256 项查表、无分支） |
| 翻页 | 10-23ms/页 | 大头是 `gray_display`（6-17ms，垂直同步 present 等待），不是渲染 |
| 图书馆首帧 | ~2ms | 索引 ~25ms、首帧渲染 ~2ms |
| 阅读器开书（进 activity → 首页） | ~51ms | EPUB 载入 ~16ms + 首页分页渲染 ~31ms |

**⚠️ 一个容易误读的坑**：`[GFX] Time` 的 `start_ms` 是全局的，跨 activity 时会把脚本等待、
EPUB 载入等间隙一并算进去（例如图书馆的 `GFX Time=1737ms` 其实是「Home 清屏 → 图书馆上屏」跨越了
脚本的 wait，不是渲染耗时）。**测渲染要用 `[ERS] Page render` 分解，或「activity 进入 → 首页」的时间差。**

结论：PROGRESS 里的 `render_bw_to_texture`「热点候选」被基准**证伪**并已关闭（做了无分支查表优化，
但它是亚毫秒级，无可测提速）；缩略图、首章分页也**不是**热点（开书 ~51ms）。模拟器当前没有值得
优化的热点，翻页延迟本质是 60Hz 垂直同步的 present 等待。

## 8. 断点调试与执行流程分析（替代刷真机看日志）

目标是「在模拟器里像调试器一样断点 + 分析执行流程」，不用再把固件刷到硬件上读串口。

### 8.1 断点调试

三条路，按顺手程度选：

1. **VS Code F5**：`.vscode/launch.json`（cppdbg + 便携 gdb），断点/单步/调用栈/变量都可用（见第 1 节）。
2. **gdb CLI**：`gdb --args build-debug/crosspoint_emulator.exe` → `break 函数名` → `run`。
3. **tracepoint 配方**（断点命中不暂停，自动打调用栈继续跑，收集所有命中点）：

```
(gdb) break ActivityManager::goToReader
(gdb) commands
> silent
> printf "[BP] goToReader hit\n"
> bt 8
> continue
> end
(gdb) run
```

### 8.2 函数级执行追踪（-finstrument-functions）

```bat
build_trace.cmd                                  & rem 追踪构建（-finstrument-functions + 符号）
set CROSSPOINT_TRACE=1                           & rem 可选 CROSSPOINT_TRACE_MAX=3000000 / _FILE=路径
set CROSSPOINT_TOUCH_SCRIPT=wait 1200; tap 232,421; wait 2500
build-trace\crosspoint_emulator.exe             & rem 跑一段交互，产出 trace.log（E/X 记录）

python scripts\trace_analyze.py trace.log build-trace\crosspoint_emulator.exe ^
    --top 20 --tree --from 500 --to 700 --dump flow.txt
```

输出：调用次数 top-N、自耗时 top-N、`--tree` 缩进调用树、`--dump` 解析后的时间线。
实测（开机 + 点开 Library，~3.4s）：**2,949,120 条记录、3175 个函数**，调用树能清楚看到
`loop → MappedInputManager::update → HalGPIO::update → readButtonState/touchUpdate → millis`。

**要了解的三个坑**（都已处理，别重踩）：

1. `-finstrument-functions` 不能放 `CMAKE_CXX_FLAGS`——会把 CMake 配置期的编译器测试也插桩，
   测试程序链接不到 hook 直接失败。要走 `CROSSPOINT_TRACE_BUILD` 选项、只加到 target。
2. MinGW 的 pthread.h 内联包装（`nanosleep` 等）被插桩后无法内联，变成强符号与
   `--whole-archive` 的 libwinpthread 冲突——追踪构建链接加 `-Wl,--allow-multiple-definition`，
   并用 `-O3 -g`（不是 RelWithDebInfo 默认的 `-O2`）。
3. exe 有 ASLR：trace 头部会写 `# anchor 0x...`（`sim_trace_anchor` 的运行时地址），分析器用它
   减去 nm 的文件地址得到加载偏移，再把所有地址调回来才交给 addr2line。

代价：全量插桩约慢 10x（开机 2.3s vs 0.2s），trace.log 很大（3M 条 ≈ 58MB），且 std::chrono/tuple
这类模板噪声多（用 `grep -v std::` 过滤）。追踪构建是独立目录 `build-trace/`，正常构建不受影响。

### 8.3 直接语义动作（绕过手势分类）

触摸/鼠标注入走的是「坐标 → 手势分类 → 命中判定」这条链。如果你的目的已经明确（"我要打开这本书"，
而不是"我要在这个坐标点一下"），没必要让分类器去猜——直接用**语义动作**调用固件自己的 API：

```bat
set CROSSPOINT_EMULATOR_ACTIONS=wait 1200; library; wait 1500; open /The Quiet Shelf.epub; wait 2500; turn; wait 1200; home
build\crosspoint_emulator.exe
```

实测日志（**全程没有鼠标/触摸事件**）：

```
[ACTION] 5 action(s) scheduled
[ACTION] library              -> [ACT] Entering activity: Library
[ACTION] open /The Quiet Shelf.epub -> [ACT] Entering activity: EpubReader
[ACTION] turn                 -> accepted -> [ERS] Rendered page
[ACTION] turn                 -> accepted -> [ERS] Rendered page
[ACTION] home                 -> [ACT] Entering activity: Home
```

支持的动作（全部直接调 `ActivityManager` / reader 的 API）：

| 动作 | 调用 | 说明 |
|---|---|---|
| `home` / `library` / `settings` / `plugins` / `usb` / `join` | `goHome` / `goToLibrary` / `goToSettings` / `goToPlugins` / `goToUsbDrive` / `goToJoinNetwork` | 这些只**排队一个 pendingAction**，绝不会被输入层吞掉 |
| `browser [路径]` | `goToFileBrowser` | |
| `open <书籍路径>` | `goToReader` | 等价于"在列表里选中这本书并按确认"的结果 |
| `turn [-]` | `remoteTurn(forward,false)` | 阅读器翻页；`-` 表示反向 |
| `chapter [-]` | `remoteTurn(forward,true)` | 整章跳过 |
| `menu` | `remoteShortcut(true)` | 打开阅读菜单 |
| `pop` | `popActivity` | 返回上一屏 |
| `refresh` | `handleForcedRefresh` | 强制整屏刷新 |
| `msg <文本>` | `goToFullScreenMessage` | 全屏提示（调试打点用） |
| `sleep` / `boot` | `goToSleep` / `goToBoot` | |
| `wait <ms>` | — | 时序间隔 |

为什么不直接调 `injectRemoteClick`（语义按键）？因为 `MappedInputManager::update()` 每帧开头会
`remoteInput.reset()`，从循环外注入的按键会被下一帧清掉；而上面这些导航/阅读器 API 要么排队
pendingAction、要么直接调 reader 方法，**不会丢**。`turn`/`chapter`/`menu` 的返回值会打印
`accepted / rejected`，rejected 说明当时不在已渲染的阅读器里（等一帧再试即可）。

### 8.4 真单步调试（真正的 step in/over/out）

**模拟器上本来就是真单步**——它是本机 x86-64 原生代码，gdb 的 `step`/`next`/`stepi`/`finish`/
`until`/`watch` 全部可用，VS Code F5 同样。唯一要注意的是固件是 `while(true){ loop(); }` 结构，
用断点 + `finish` 跳过循环体，别硬 `step` 过整个主循环。

**真机上要真单步，必须走 JTAG**（不是串口）。好消息是 **Xteink X4 用的 ESP32-C3 自带 USB
Serial/JTAG**，一根 USB-C 线就能硬件调试，不需要额外的 JTAG 探针：

```ini
; Crosspoint/platformio.ini（或 platformio.local.ini，避免污染上游）
[env:debug]
extends = env:default
build_type = debug          ; -Og -g3，行号准确
debug_tool = esp-builtin    ; ESP32-C3/S3 内置 USB-JTAG
debug_init_break = tbreak setup
```

```bat
pio run -e debug              :: 编 debug 版
pio debug -e debug            :: VSCode 里启动调试（断点/单步/变量/调用栈）
:: 或纯命令行：
pio debug -e debug --interface=gdb -- -ex "target extended-remote :3333"
```

要点与坑：

- **必须 `-Og`**（`build_type = debug` 会带）。`-O2/-Os` 下函数被内联/重排，`next` 会跳得莫名其妙。
- **不要用串口调试**：串口只能看日志。JTAG 会**暂停 CPU**，此时面板/看门狗都停——单步慢是正常的。
- **硬件断点数量有限**（RISC-V 的触发单元少），必要时用软件断点（gdb 默认会往 flash 里插）。
- **FreeRTOS 多任务**：`info threads` 能看到任务；单步只停当前任务，其它任务在你停住时不跑（JTAG halt 整个核）。
- **USB 口被 OpenOCD 占用**时串口日志看不了；需要日志就先用 `monitor` 或改回普通 USB 模式。
- **设备上做"直接语义动作"**：没有模拟器可以 call 进去，两条路——
  1. 加一个**调试串口命令**（`Serial` 收到 `open /book.epub\n` → 调 `activityManager.goToReader`），
     用 `#if CROSSPOINT_DEBUG_ACTIONS` 包起来，只在调试构建启用（改固件要走 fork 分支，别混进 `main`）；
  2. 或者 JTAG + gdb `call`（C++ 方法带 `std::string` 参数时很难写，推荐第 1 种）。

---

## 不能做什么（别被"能跑"误导）

| 限制 | 影响 |
|---|---|
| **网络全是桩**：Wi‑Fi / TLS / OPDS / 插件下载 / OTA 都不连真网 | 这些路径只能看 UI 流程，验证不了协议、证书、重试逻辑 |
| **内存数字是假的**：`MEM` 日志来自固定的 8 MB/16 MB 桩 | 不能用来判断真机 OOM；真机 heap 是同一份代码，但数值不同 |
| **不是周期精确**：没有面板波形、刷新时序、SPI 时序 | 功耗、残影、刷新闪烁这类问题必须上真机 |
| **外设缺失**：无蓝牙、无 IMU、无前置光 | 相关功能走的是空实现（触摸是例外：鼠标/热键/脚本三条通道已可用，见第 5 节） |
| **无字形/排版可视化**：只有断点里的数据 | 字体度量、断行问题要自己写打印或看截图 |

需要验证上面这些，还是得上真机（或至少真机 + 串口日志）。模拟器的定位是**UI 与业务逻辑的快速迭代**，
它把这个层面做到了源码级可调试。

---

## 附：一次完整调试流程

```bat
:: 1) 编带符号版本
build.cmd                                  & rem 或按第 1 节编 build-debug

:: 2) 带日志启动（stdout 重定向）
cd C:\path\to\run-dir                      & rem 目录里要有 sdcard\
set CROSSPOINT_LOG=1
C:\path\build-debug\crosspoint_emulator.exe > emu.log 2>&1

:: 3) 另开窗口看日志
powershell -Command "Get-Content emu.log -Wait | Select-String 'ERR|ACT'"

:: 4) 需要停住时，gdb attach 到进程
tools\msys64\mingw64\bin\gdb.exe -p <pid>
(gdb) break EpubReaderActivity::render
(gdb) continue
```
