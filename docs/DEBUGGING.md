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

运行中按 **Ctrl+S**（macOS 上 Cmd+S），会在**工作目录**下写 `screenshots/crosspoint-<时间戳>.bmp`：
它是渲染器输出原始尺寸，不带窗口边框、不受系统缩放影响。适合做 UI 回归对比。

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

## 5. 性能观测

`[GFX] Time = N ms from clearScreen to displayBuffer` 是每次刷新的耗时统计，可以直接看出
某个界面是不是变慢了。注意它包含宿主渲染时间，不等于真机波形时间。

---

## 不能做什么（别被"能跑"误导）

| 限制 | 影响 |
|---|---|
| **网络全是桩**：Wi‑Fi / TLS / OPDS / 插件下载 / OTA 都不连真网 | 这些路径只能看 UI 流程，验证不了协议、证书、重试逻辑 |
| **内存数字是假的**：`MEM` 日志来自固定的 8 MB/16 MB 桩 | 不能用来判断真机 OOM；真机 heap 是同一份代码，但数值不同 |
| **不是周期精确**：没有面板波形、刷新时序、SPI 时序 | 功耗、残影、刷新闪烁这类问题必须上真机 |
| **外设缺失**：无蓝牙、无 IMU、无前置光、无触摸 | 相关功能走的是空实现 |
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
