# patches/

这个分支固定的固件是 **2026-10-27（c9cb1f8d，对应 2.x）**，不需要任何补丁。

`main` 分支上针对 1.0.0 的两个修复，在这个版本里已经由上游自己解决了：

| 旧补丁（main 分支，1.0.0） | 新固件里的状态 |
|---|---|
| `0001` `CrossPointState.h` 补 `<cstdint>` | 已包含 `#include <cstdint>` |
| `0002` `onGoToReader()` 先拷贝再 `exitActivity()`（use-after-free） | 改成 `ActivityManager::goToReader(std::string path)` 按值传参，不再有悬垂引用 |

`prepare_firmware.cmd` 仍会遍历本目录下的 `*.patch`（幂等：已应用则跳过）。以后如果发现新固件
在这套模拟器下需要打补丁，直接把 `git format-patch` 的产物放进来即可。
