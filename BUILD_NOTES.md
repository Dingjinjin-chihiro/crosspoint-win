# Crosspoint Emulator — 本机构建与运行说明

把 [jonmooreai/Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator) 和它模拟的
[Crosspoint 电子书固件](https://github.com/crosspoint-reader/crosspoint-reader) 在本机（Windows 11）上跑起来了。
本文件记录**为什么这样配、改了哪些地方、怎么重建**。

当前状态：已编译成功并实机验证（首页 → 文件浏览 → TXT/EPUB 阅读 → 翻页 → 阅读菜单 → 返回），没有崩溃。

---

## 1. 目录结构

```
cross_point/
├─ Crosspoint-Emulator/        # 模拟器（SDL2 窗口 480x800 + 虚拟 SD 卡 ./sdcard）
│  ├─ build/                   # 构建产物 crosspoint_emulator.exe + 运行期 DLL
│  └─ sdcard/                  # 虚拟 SD 卡根目录（放书、缓存、阅读进度）
├─ Crosspoint/                 # Crosspoint 固件源码（被模拟的对象），固定到 tag 1.0.0
├─ tools/msys64/               # 便携工具链：MinGW-w64 GCC 16.2 / CMake 4.5 / Ninja / SDL2 2.32.10
├─ scripts/                    # 辅助脚本（生成示例书、驱动模拟器、帧缓冲诊断）
├─ docs/screenshots/           # 实测截图
├─ run_emulator.cmd            # 启动模拟器
└─ build.cmd                   # 重新编译
```

## 2. 运行

双击或在命令行执行：

```
run_emulator.cmd
```

按键：

| 按键 | 功能 |
|------|------|
| ← ↑ → ↓ | 四向导航 |
| Enter | 确认 / 选择 |
| Backspace 或 Esc | 返回 |
| P | 电源键（设置里长按可休眠） |

退出：关闭窗口或 Alt+F4。

## 3. 放书

把 `.epub` / `.txt` / `.xtc` 放进 `Crosspoint-Emulator\sdcard\`（可建子目录），重启模拟器即可在
“Browse Files”里看到。当前已带两本示例：

- `A Short Walk.txt`
- `The Quiet Shelf.epub`（由 `scripts/make_sample_epub.py` 生成）

生成新示例书：

```
python scripts\make_sample_epub.py "Crosspoint-Emulator\sdcard"
```

## 4. 重新编译

```
build.cmd
```

等价命令（在 MSYS2 的 mingw64 环境里）：

```bash
cmake -S Crosspoint-Emulator -B Crosspoint-Emulator/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCROSSPOINT_ROOT=<绝对路径>/Crosspoint
cmake --build Crosspoint-Emulator/build -j 8
```

编译完成后需要把运行期 DLL 放到 exe 旁边（首次已做）：

`SDL2.dll`、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`（来自 `tools/msys64/mingw64/bin`）。

---

## 5. 为什么要固定 Crosspoint 到 1.0.0

模拟器最后一次提交是 **2026-02-11**，而 Crosspoint 主干此后已大改：

- HAL 重构（`SdMan` → 全局 `HalStorage Storage`，`openFileForRead` 等接口变化）
- 新增 `freeink-sdk` 子模块、多个新库、C++20、新的 GfxRenderer 方向系统

直接对最新主干编译会在 `ContentProtection.h`、`Storage` 未声明等地方失败（仓库 issue #1、#6 记录的就是这件事）。
因此选择与模拟器同期的 **Crosspoint tag 1.0.0（2026-02-09）**，其 HAL 接口与模拟器 `sim/include` 中的
`SDCardManager`/`HalStorage` 存根完全对应。

当前 `Crosspoint` 工作区就是 `1.0.0` 提交加上下面第 6 节的两处修复。

## 6. 为跑起来做的修改

工具链本身用 MSYS2（TUNA 镜像）+ MinGW-w64 GCC，因为固件依赖 GCC 扩展和 POSIX 头文件
（`unistd.h`、`dirent.h`），MSVC 编不过；机器上也没有安装 VS C++ 工作负载。

### 6.1 模拟器侧（Crosspoint-Emulator）

| 文件 | 修改 | 原因 |
|------|------|------|
| `sim/src/sim_storage.cpp` | 增加 `sim_mkdir()` / `sim_realpath()` 兼容层（`_mkdir` / `_fullpath`） | MinGW 的 `mkdir` 只有一个参数、没有 `realpath` |
| `CMakeLists.txt` | 对 C++ 强制 `-include sim_std_prelude.h` | 固件头文件依赖 Arduino 核心的“隐式包含”，宿主编译需要补上 `<functional>`/`<cstdint>`/`<cassert>` 等 |
| `sim/include/sim_std_prelude.h` | 新增，集中包含标准头 + `using std::min/max` | 同上；Arduino 里 `min/max` 是全局可用的 |
| `sim/include/ArduinoStub.h` | 补一批标准头 | 同上 |
| `sim/include/WString.h` | 增加 `String(long long)` / `String(unsigned long long)` | 64 位下 `size_t` 构造 `String` 有重载歧义 |
| `sim/src/sim_display.cpp` | 实现 `saveFrameBufferAsPBM()`，并加 `CROSSPOINT_DUMP_FB` / `CROSSPOINT_DUMP_TEX` 调试开关 | 把 800×480 面板帧缓冲和交给 SDL 的 480×800 纹理落盘，方便核对“固件画的对不对”和“显示的对不对” |
| `Crosspoint/lib/Epub` 等 | 无改动 | — |

> 调试开关默认关闭，不影响正常运行。

### 6.2 固件侧（Crosspoint 1.0.0）—— 一个真实 bug

| 文件 | 修改 | 原因 |
|------|------|------|
| `src/main.cpp` `onGoToReader()` / `onGoToMyLibraryWithPath()` | 在 `exitActivity()` **之前**把传入的 `const std::string&` 拷贝一份再用 | **use-after-free**：回调参数通常指向当前 Activity 自己持有的字符串（如 `HomeActivity::recentBooks[i].path`），`exitActivity()` 会销毁该 Activity，之后再用这个引用构造 `ReaderActivity` 就是悬垂引用。实测在 Windows/MinGW 下进入阅读器时稳定触发 `0xC0000005` 访问冲突（gdb 栈：`onGoToReader` → `std::string` 构造 → `memmove`） |
| `src/CrossPointState.h` | 增加 `#include <cstdint>` | 头文件里用了 `uint8_t` 却没包含对应头（设备端靠 Arduino.h 兜底） |

修复后：从首页/文件列表打开 TXT 与 EPUB、翻页、返回、阅读菜单，均不再崩溃。

## 7. 已验证的功能

- 首页（近期书籍封面、Browse Files / Recents / File Transfer / Settings、状态栏、按键提示）
- 文件浏览（SD 卡目录、选中高亮、Up/Down/Open/Home）
- TXT 阅读（自动换行、2 页、页码/百分比状态栏）
- EPUB 阅读（章节标题、两端对齐、章节 × 页数统计、翻页到下一章）
- 阅读菜单（Go to Chapter / Reading Orientation / Go to % / Go Home / Sync Progress / Delete Book Cache）
- 返回导航、窗口尺寸 480×800 与 800×480 面板帧缓冲的旋转映射

未涉及：Wi-Fi/OTA/WebDAV（固件侧在模拟器里是桩）、DRM 书籍（需要 wolfSSL）。

## 8. 排查用的小工具

```powershell
# 以帧缓冲转储方式启动（生成 fb.pbm，800x480 原始面板数据）
$env:CROSSPOINT_DUMP_FB="C:\path\fb.pbm"
$env:CROSSPOINT_DUMP_TEX="C:\path\tex.ppm"
Crosspoint-Emulator\build\crosspoint_emulator.exe

# 把面板帧缓冲转成 PNG 并给出内容范围
python scripts\pbm_info.py fb.pbm panel.png
```

`scripts/emu_nav.ps1` 可向模拟器窗口发送按键（PostMessage，方向键带 extended 标志位，否则 SDL 会
把方向键解析错），用于无人工干预的回归测试。
