---
name: io-hardware-integration
description: IO信号系统从Mock到真实GTS SDK硬件的完整对接过程、踩坑和最终架构
metadata:
  type: project
---

## 真实IO硬件对接 — 完整过程

### 目标
将 chipSetter 的IO系统从 MockGncController 假数据对接为真实 GTS SDK 硬件读取。

### 最终架构 (用户重构后)
- 单一 `GncController` 类，直接 `#include "googol/gts.h"`
- 不再有 IGncController/MockGncController/GncControllerImpl 分层
- 始终编译为 GTS SDK 模式，链接 googol/gts.lib，无Mock模式
- 新增 `DispensingPlatformController` (轴2+3) 和 `PickupPlatformController` (轴10+11)

### 关键踩坑

1. **MC_GPI 值错误 (最严重的bug)**
   - 错误: Mock模式下 MC_GPI=0，但GTS SDK中 MC_GPI=4
   - 后果: GT_GetDi(0, ...) 读取的是正限位寄存器而非GPI通用输入，IO信号永远不变
   - 修复: 直接 include gts.h，使用其中正确的 MC_GPI=4, MC_GPO=12

2. **结构体冲突**
   - gts.h 中的 TStandardHomePrm/TStandardHomeStatus 与旧Mock定义布局不同
   - 曾用本地 extern "C" 声明避开include gts.h，但导致了MC_GPI=0的bug
   - 最终方案: 去掉Mock层，直接include gts.h

3. **编译模式混乱**
   - 曾有两种模式互斥编译，Mock/Real容易混淆
   - 修复: 始终Real模式，简化.pro

4. **cfg文件路径**
   - GT_LoadConfig 需要正确的cfg路径，在工控机上路径可能不同
   - 修复: 多路径搜索 (直接路径 → googol/子目录 → 当前目录)

5. **报警系统接入**
   - GncController 通过 hardwareError 信号发射错误
   - MainWindow 连接 → AlarmLogger::raiseAlarm(WARNING)
   - gtsCall() 追踪连续错误: 第1次报警 → 第5次严重警告 → 恢复时通知

### IO信号流
IoManager::onPollTimer() [50ms] → GncController::readDI() → GT_GetDi(MC_GPI=4, &bitmask) → values[i] = (bitmask >> i) & 1 → detectChanges() → emit diChanged() → IoMonitorWidget/轴限位映射

### 关键文件
- core/GncController.h/.cpp — GTS SDK封装
- core/HardwareConfig.h — DI_COUNT=19, DO_COUNT=4, DO_INDEX_BASE=9
- core/IoManager.h/.cpp — IO轮询和变化检测
- chipSetter.pro — 固定GTS SDK编译，链接googol/gts.lib
- googol/ — gts.h, gts.lib, gts.dll, gt_rn.dll, core1_20261212.cfg
- scripts/workflow.ps1 — 统一调度脚本
- CLAUDE.md — 完整项目文档

**Why:** IO是机器控制的基础，错误的值会导致所有传感器读数不正确。
**How to apply:** 新增IO信号时修改HardwareConfig.h；所有硬件操作通过GncController，不直接调gts.h。
