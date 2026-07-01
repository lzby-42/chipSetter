# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# chipSetter — 固晶机控制系统

Qt 5.15.2 C++ 上位机，运行于固高 GNC-C610 工控机 (Win10, 192.168.1.2)。

## 最重要的事

1. **不要直接 include gts.h**：只有 `GncController.h` 才 include，其他文件通过 `GncController` 类访问硬件
2. **日志位置**：`logs/chipsetter_年月日_时分秒.log`（每次启动生成新文件，自动创建logs目录）
3. **电机参数持久化**：`motor_params.json`（exe同目录），启动自动加载，点击"应用"自动保存（写临时文件+rename防断电）
4. **轴参数适用性**：旋转轴/曲柄可取消导程勾选，气缸轴可取消软限位勾选，JSON持久化
5. **运动控制器 API**：使用 `.claude/skills/googol-motion-controller/` skill 查询 GTN_* API 用法、模式、触发器等

## 硬件架构

```
UI (widgets/) ──Signal/Slot──> Core (core/) ──GncController──> GTS SDK (googol/gts.dll)
                                │                                  └── GNC-C610 运动控制器
                                │                                      └── 19路DI (X0-X18, GPI1-19)
                                │                                      └── 4路DO (Y9-Y12, GPO10-13)
                                │                                      └── 16轴 (胶盘, 点胶, 取晶...)
                                ├── ProcessManager (9步状态机)
                                │     ├── DispensingPlatformController (轴2+3)
                                │     └── PickupPlatformController (轴13+14)
                                ├── AlarmLogger (报警触发/恢复/历史)
                                └── StatsCollector (产量/时长/节拍)
```

### IO 信号映射 (HardwareConfig.h, 以 CFG 为准)

DI_AXIS_MAP 格式: `{gpiNumber, axisId, signalType}` (0=HOME, 1=正限位, 2=负限位)

| GPI | 端口 | 信号名称 | →轴 | 信号类型 |
|-----|------|----------|-----|----------|
| 1 | X0 | 点胶平台X轴正限位 | 2 | 正限位 |
| 2 | X1 | 点胶平台X轴负限位 | 2 | 负限位 |
| 3 | X2 | 点胶平台X轴HOME | 2 | HOME |
| 4 | X3 | 点胶平台Y轴正限位 | 3 | 正限位 |
| 5 | X4 | 点胶平台Y轴负限位 | 3 | 负限位 |
| 6 | X5 | 点胶平台Y轴HOME | 3 | HOME |
| 7 | X6 | 取晶X轴正限位 | 7 | 正限位 |
| 8 | X7 | 取晶X轴负限位 | 7 | 负限位 |
| 9 | X8 | 取晶X轴HOME | 13 | HOME |
| 10 | X9 | 取晶Y轴负限位 | 14 | 负限位 |
| 11 | X10 | 取晶Y轴正限位 | 14 | 正限位 |
| 12 | X11 | 取晶臂转动 | 7 | 正限位 |
| 13 | X12 | 点胶臂转动 | 8 | HOME |
| 14 | X13 | 取晶平台W轴 | 15 | HOME |
| 15 | X14 | 吸嘴HOME | 16 | HOME |
| 16 | X15 | 上顶针 | 11 | 正限位 |
| 17 | X16 | 下顶针 | 12 | 正限位 |
| 18 | X17 | 点胶臂上下 | 9 | 正限位 |
| 19 | X18 | 取晶臂上下 | 10 | **负限位** |

| GPO | 端口 | 信号名称 |
|-----|------|----------|
| 10 | Y9 | 红灯 |
| 11 | Y10 | 黄灯 |
| 12 | Y11 | 绿灯 |
| 13 | Y12 | 蜂鸣器 |

DO_INDEX_BASE=10, 使用 `GTN_WriteDigitalOutputBit`(1-indexed, doIndex 10-13)。硬件低电平有效(0=开,1=关), `IoManager::setDO` 自动翻转。

## 编译

### 统一调度脚本 (推荐)

```bash
powershell -File scripts/workflow.ps1 <命令> [选项]

# 命令:
#   test     实机测试全流程: 编译 → 打包 → 部署 → 远端启动 (默认)
#   debug    F5调试: 同上 (走 gdbserver)
#   quick    跳过编译: 打包 → 部署 → 启动 (已编译过时用)
#   build    仅编译
#   package  仅打包
#   deploy   仅部署
#   start    仅远端启动 (直接启动, 无 gdbserver)
#   stop     仅远端停止
```

### 编译
```bash
powershell -File scripts/workflow.ps1 build
# 或: scripts\build_debug.bat (双击运行)
# 输出: debug/chipSetter.exe
```

### 部署到工控机
```bash
powershell -File scripts/workflow.ps1 test
# 完整链路: 编译 → 打包 → 部署 → 启动
```

### 测试
```bash
cd tests && qmake tests.pro && make -f Makefile.Debug -j4

# 运行全部测试:
./debug/test_hardware_config.exe -txt    # 9 个测试: 轴/DI/DO 计数、名称、ID范围、默认值
./debug/test_motor_axis.exe -txt         # 4 个测试: 默认构造、自定义ID、元类型注册、范围
./debug/test_process_manager.exe -txt    # 8 个测试: 初始状态、循环、暂停继续、收尾、急停、步骤定义
```

## 关键文件

| 文件 | 职责 |
|------|------|
| `chipSetter.pro` | qmake 工程，固定 GTS SDK 编译 |
| `core/HardwareConfig.h` | 常量定义：AXIS_COUNT=16, DI_COUNT=19, DO_COUNT=4, DO_INDEX_BASE=10, DI_AXIS_MAP |
| `core/GncController.h/.cpp` | **GTS SDK 封装**：GT_Open/GT_GetDi/GT_SetDoBit 等全部硬件操作，SDK 错误追踪 |
| `core/IoManager.h/.cpp` | IO 管理器：50ms 轮询 DI/DO，变化检测，发出 diChanged/doChanged 信号 |
| `core/MotorManager.h/.cpp` | 16 轴状态管理，运动控制 |
| `core/ProcessManager.h/.cpp` | 9 步工艺状态机，持有 DispensingPlatform/PickupPlatform 引用 |
| `core/DispensingPlatformController.h/.cpp` | 点胶平台（轴2=X, 轴3=Y）：X→Y 回零序列 + 双轴并发点位移动，错误上报 AlarmLogger |
| `core/PickupPlatformController.h/.cpp` | 取晶平台（轴13=X, 轴14=Y）：X→Y 回零序列 + 双轴并发点位移动，错误上报 AlarmLogger |
| `core/AlarmLogger.h/.cpp` | 报警管理器：报警触发/恢复，历史记录，活跃计数，自增 ID |
| `core/StatsCollector.h/.cpp` | 生产统计：产量、运行时长（小时）、平均节拍（秒/件），1秒定时器更新 |
| `models/MotorAxis.h` | 轴数据模型：位置/速度/限位/回零(3模式)/导程/软限位/状态标志 |
| `models/IoSignal.h` | IO 信号数据模型（ID/类型/名称/值） |
| `models/AlarmRecord.h` | 报警记录数据模型（ID/时间戳/级别/来源/消息/已解决） |
| `widgets/MotorControlWidget.h/.cpp` | 调试模式核心：轴选择按钮组 + PTP运动 + 参数编辑 三合一（替代旧 MotorPtpWidget + MotorParamWidget） |
| `mainwindow.h/.cpp` | 主窗口：创建全部 8 个 Core 模块、10 个 Widget，信号连线总控 |
| `main.cpp` | 入口：文件日志初始化，QSS 加载 |
| `googol/` | GTS SDK 文件：gts.h, gts.lib, gts.dll, gt_rn.dll, core1_20260625.cfg |
| `scripts/workflow.ps1` | **统一调度脚本**：编译→打包→部署→启动/停止（test/debug/quick/build 等 7 个命令） |
| `scripts/build_debug.bat` | 一键编译（双击运行） |
| `tests/` | 3 个单元测试 |
| `.vscode/start_direct.ps1` | 直接启动 chipSetter.exe（无 gdbserver），用于实机测试（区别于 F5 调试） |

## 回零机制

3 种模式，由 `motor_params.json` 中 `homeMode` 控制：

### mode=10 — 限位回零 (GTN_GoHome)

适用：CFG 中配有限位开关的轴。控制器自动完成"往限位搜索→触发→反向退离→停止"。

CFG 中只有单侧限位的轴，`homeDir` 必须指向存在的那一侧：

| 轴 | 仅有的限位 | homeDir |
|----|-----------|---------|
| 7  | GPI12 正限位 | 1 |
| 9  | GPI18 正限位 | 1 |
| 10 | GPI19 负限位 | -1 |
| 12 | GPI17 正限位 | 1 |
| 14 | 双向都有 | 任意 |

`escapeStep`：触发限位后反向退离的 pulse 数。`homeEscapeStep=0` 时自动计算 `qMax(hv*10, 50)`，>0 时直接使用。注意 GPI19=轴10 **负限位**（CFG `limitNegativeIndex=19`，HardwareConfig.h DI_AXIS_MAP signalType=2）。

**重要**：`GTN_GoHome` 完成后不会自动清零位置寄存器——必须手动调用 `GTN_ZeroPos`。mode=10/20/35 均在轮询检测到回零成功时调用 `m_controller->zeroPosition()`。

### mode=20 — IO 回零 (Event-Task 两阶段)

适用：用任意 GPI 做 home 标记点的轴（轴8 GPI13、轴15 GPI14、轴16 GPI15）。

绕过 `GTN_SetTriggerEx` 的硬件模块约束（"Trigger/捕获源/编码器需同模块"→error 7），改用 Event-Task：

```
Event(WATCH_VAR_GPI + WATCH_CONDITION_UP/DOWN 边沿) → Task(TASK_STOP option=0 急停)
```

**三阶段：**
1. **Phase 0 快速搜索**：Jog `homeVelocity×100%` → GPI 边沿 → 硬件急停
2. **Phase 1 反向退离**：Trap 反向 200 pulse → 离开传感器
3. **Phase 2 慢速校验**：Jog `homeVelocity×10%` → GPI 边沿 → 急停 → ZeroPos

精度 ≈ 快速模式的 10 倍（慢速急停过冲更小）。

### mode=35 — 当前位置清零

不运动，直接 `GTN_ZeroPos`。适用：轴1 胶盘。

### 轴回零参数 (MotorAxis)

| 字段 | JSON key | 说明 |
|------|----------|------|
| `homeMode` | `homeMode` | 10/20/35 |
| `homeDir` | `homeDir` | 搜索方向 1/-1 |
| `homeEdge` | `homeEdge` | 触发边沿 0=下降沿 1=上升沿 |
| `homeVelocity` | `homeVelocity` | 搜索速度 (mm/s 或 pulse/ms) |
| `homeOffset` | `homeOffset` | 回零偏移 (mm) |
| `triggerIndex` | `triggerIndex` | mode=20 时 GPI 编号 |
| `homeEscapeStep` | `homeEscapeStep` | mode=10 退离 pulse (0=自动) |

### applyRequested 保护

`mainwindow.cpp` lambda 在调 `updateAxisParams` 前合并当前轴状态，保护 Widget 不管理的字段（`homeDir/homeEdge/homeMode/triggerIndex/homeEscapeStep/isActive/jogStep`），防止点"应用"时用 MotorAxis 默认值覆盖 JSON 配置。

## Event-Task 并发限制

`GTN_EventOn(core, eventId, 1)` 和 `GTN_ClearEvent(core)` 是**全局操作**——同一 core 上同一时刻只能有一个 Event-Task 活跃。因此多个 mode=20(IO回零) 轴**不能并发回零**。

**正确做法**：使用 `GTN_DeleteEvent`/`GTN_DeleteTask`/`GTN_DeleteEventTaskLink` 按 ID 删除，避免全局 Clear 破坏其他轴的 Event-Task。每个轴存储独立的 `eventId`/`taskId`/`linkId`（`MotorManager::m_homeEventId/TaskId/LinkId[]`），完成时只删自己的。

**并发限制**：即使使用 per-ID Delete，`GTN_EventOn` 仍是全局的。因此同一初始化相位中只能有一个 mode=20 轴。当前 Phase2 中轴8和轴15都是 mode=20，它们通过 per-ID Delete 隔离资源但**仍然串行执行**（EventOn 全局互斥）。

## 初始化流程 (步骤①)

步骤①(初始化)已从定时器模拟改为**事件驱动状态机**。5 个相位，11 个子步骤：

```
Phase 0: 亮黄灯 (同步)
Phase 1: 胶盘(轴1,mode=35) + 下顶针(12) + 取晶臂(7) + 点胶平台(2,3) [并发]
Phase 2: 取晶平台XY(13,14) + 取晶W(15) + 点胶臂(8) + 取晶上下(10) [并发]
Phase 3: 点胶臂旋出 5450pulse (轴8)
Phase 4: 点胶上下回零 (轴9)
```

**关键设计**：
- `ProcessManager` 新增 `InitPhase` 枚举 + `m_initPendingMask` 位掩码跟踪并发完成
- `ProcessManager::setIoManager/setMotorManager` 注入硬件依赖
- 子步骤按轴拆分（11个），每个信号处理器标记对应子步骤 COMPLETED
- 单轴回零失败**只重试该轴 1 次**，仍失败→全部停止+红灯+报警(`initError` 信号)
- 超时保护：Phase1/2/4 60s, Phase3 30s

**灯控**（黄灯 GPO11=Y10, 红灯 GPO10=Y9, 绿灯 GPO12=Y11，低电平有效）：
- Phase0: 关全部 DO → 亮黄灯
- INIT 完成: `stepStateChanged(0,COMPLETED)` → 关黄/红 → 亮绿灯 (MainWindow lambda)
- 失败/急停: 关黄/绿 → 亮红灯

**已知限制**：ProcessManager 通过 `m_motorManager` 指针直接控制单轴回零和移动，DispensingPlatform/PickupPlatform 在 INIT 中仅用于 XY 序列回零。后续步骤(②~⑧)仍为定时器模拟，需逐步替换为硬件驱动。

## 平台控制器超时保护

`DispensingPlatformController` 和 `PickupPlatformController` 新增 `QTimer` 超时机制：

- `m_homeTimer`(单次,60s) — `home()` 启动, `homeFinished` 或 `stop()` 停止
- `m_moveTimer`(单次,30s) — `moveTo()` 启动, `moveFinished` 或 `stop()` 停止
- 超时触发 → `stop()` + `reportError()` + `emit homeFinished/moveFinished(false)`
- 宏定义: `HOME_TIMEOUT_MS=60000`, `MOVE_TIMEOUT_MS=30000` (HardwareConfig.h)

**其他防御性修复**：
- `pendingMoves`(int 计数器) → `pendingMask`(int 位掩码) — 防止重复信号导致提前归零
- `stopMove()` 同步清除 `m_homingActive` — 防止急停后"已在回零中"拒绝重试
- `atomicWrite()` 改用 `QSaveFile` — NTFS 上真正原子写入，防止崩溃丢配置

## IO 信号流

```
IoManager::onPollTimer()  [每50ms]
  └→ GncController::readDI(core=1, diType=MC_GPI=4, diIndex=1, values, count=19)
       └→ GT_GetDi(4, &bitmask)          // GTS SDK: 读 GPI 32位掩码
            └→ values[i] = (bitmask >> i) & 1   // 逐位提取
  └→ IoManager::detectChanges()
       └→ emit diChanged(id, value)      // 信号通知 UI
            └→ IoMonitorWidget::onDiChanged(id, value)   // 调试面板
            └→ MainWindow lambda: DI→轴限位映射            // 生产面板
```

## UI 交互架构

### 窗口布局 (1024×710 + 标题栏 ≈ 768)

```
┌────────────────────────────────────────────────────────────┐
│ StatusBarWidget (26px, 双模式共享)                          │
│  固晶机控制系统 V1.0 | [模式灯(可点击)] | 产量 | 节拍 | 时长 │
├────────────────────────────────────────────────────────────┤
│ QStackedWidget                                              │
│ ┌[0] 生产模式 ─────────────────────────────────────────┐   │
│ │ ProductionWidget                                      │   │
│ │  ┌─ 工艺流程 ────────────────────────────────────┐   │   │
│ │  │ FlowStepBar: 9步按钮 + 循环框 + 回路箭头       │   │   │
│ │  │ ①(外) → [②→③→④→⑤→⑥→⑦→⑧](绿框) → ⑨(外)     │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │  ┌─ 步骤详情 (点击流程块切换) ───────────────────┐   │   │
│ │  │ StepDetailPanel: 子步骤 | 参数(可编辑) | 实时   │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │  ┌─ 设备状态 ────────────────────────────────────┐   │   │
│ │  │ DeviceStatusWidget: 16轴灯 ● + 急停 + GNC连接  │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │                                      ┌─ 报警列表 ─┐   │   │
│ │                                      │ 右栏        │   │   │
│ └──────────────────────────────────────┴─────────────┘   │   │
│ ┌[1] 调试模式 ─────────────────────────────────────────┐   │   │
│ │ 左(420px): MotorControlWidget (PTP+参数合一)          │   │
│ │ 中(stretch): IoMonitorWidget + StatsWidget            │   │
│ │ 右(220px): AlarmListWidget + 视觉预留                  │   │
│ └───────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────┤
│ BottomBarWidget (34px, 双模式共享)                          │
│  ▶启动 | ⏸暂停 | 收尾 | 立即收尾 | ↺复位 | [急停] | ⇄切换 │
└────────────────────────────────────────────────────────────┘
```

### 模式切换

| 入口 | 机制 |
|------|------|
| 底部操作栏 `⇄ 调试/生产` 按钮 | `BottomBarWidget::modeSwitchClicked` → QStackedWidget::setCurrentIndex |
| 状态栏模式灯(可点击) | `StatusBarWidget::modeClicked` → 同上（注意：已改为 QPushButton，不是 QLabel） |
| 生产界面内 "调试" 按钮 | `ProductionWidget::switchToDebugMode` → 切换到 index 1 |

两种模式共享 StatusBar + BottomBar + 底层 Core 数据，仅 QStackedWidget 内容切换。

### 生产模式 — 9 步工艺流程

```
①初始化 → ┌[循环框: ②上料→③定位→④顶晶→⑤拾取→⑥点胶→⑦贴装→⑧下料]──(⤴回路箭头)──┐ → ⑨收尾
          └ 绿色边框 #2e7d32, 背景 #0d1a0d ───────────────────────────────────────┘
```

**FlowStepBar 视觉规范:**
- 9 个 QPushButton，固定 72×60px，clickable
- 步骤 1-7 包裹在绿色 QFrame (`#LoopFrame`) 内，框内底部有 ⤴ 回路箭头(黄色 `#ffd740`)
- 步骤 0(初始化) 和 8(收尾) 在循环框**外部**
- 步骤按钮颜色状态:
  - PENDING: `bg:#2a2a3a, fg:#666, border:#333`
  - RUNNING: `bg:#1565c0, fg:#fff, border:#64b5f6` (bold + glow)
  - COMPLETED: `bg:#1b5e20, fg:#81c784, border:#388e3c`
  - FAILED: `bg:#b71c1c, fg:#ef9a9a, border:#f44336`

**StepDetailPanel 三栏:**
| 栏 | 内容 | 控件类型 | 是否可编辑 |
|----|------|----------|-----------|
| 左 | 子步骤列表 (○/◉/✓/✕ + 名称) | QLabel | 否 |
| 中 | 步骤参数 (名值对) | QLabel(名) + **QLineEdit(值)** | **是** |
| 右 | 实时状态 (位置/压力/耗时等) | QLabel(名+值) | 否 |

- 参数值编辑完成触发 `paramEdited(stepIndex, name, value)` → ProcessManager 更新
- 存储变量名为 `m_paramValues` (QPair<QLabel*, QLineEdit*>)，不是老的 `m_paramLabels`

**DeviceStatusWidget:**
- 16 个轴指示灯 ● (灰色未使能/绿色就绪/蓝色运动中/红色报警)
- 急停状态标签 + IO 活跃信号摘要(最多3个) + GNC 连接状态

### 底部操作栏 6 按钮

| 按钮 | 样式 | 触发的 ProcessManager 方法 | 说明 |
|------|------|--------------------------|------|
| ▶ 启动 | 绿底 | `startCycle()` | 重置全部，从①初始化开始 |
| ⏸ 暂停 | 灰色 | `pauseCycle()` | 完成当前步骤后停止 |
| 收尾 | 橙色 | `requestFinish()` | **出循环关机**: 设置 pendingFinish 标志，当前循环跑完后自动进⑨ |
| 立即收尾 | 深橙 | `finishCycle()` | **立刻跳**⑨收尾，不等循环结束 |
| ↺ 复位 | 浅灰 | `emergencyStop()` + clearAlarms + resetStats | 全部重置 |
| ⏻ 急停 | 红色(右侧独立) | `emergencyStop()` + 报警 | 立刻全部重置灰色 |

### ProcessManager 行为细节

| 方法 | 行为 | 步骤复位 | 子步骤复位 |
|------|------|---------|-----------|
| startCycle() | m_currentStepIdx<0 时重置全部步骤，从①开始 | 全部 PENDING | 全部 PENDING |
| pauseCycle() | 停止定时器，m_paused=true | — | — |
| resumeCycle() | m_currentStepIdx>=0 时重新启动定时器；<0 时从循环起点开始 | — | — |
| finishCycle() | 立刻跳⑨收尾，完成后全部重置 | 全部 PENDING | **必须同步重置** |
| requestFinish() | 设 m_pendingFinish=true | (循环终点触发) | — |
| emergencyStop() | 定时器停，全部重置，m_currentStepIdx=-1 | 全部 PENDING | ⚠️ **未重置子步骤** (仅设 m_substepProgress=0) |

**循环逻辑 (completeCurrentStep 中):**
- ⑧下料完成 → 如 m_pendingFinish=true → 进⑨收尾
- ⑧下料完成 → 如 m_paused=true → 停止，m_currentStepIdx=-1
- ⑧下料完成 → 正常 → 循环步骤 ②~⑧ 重置 PENDING（含子步骤！），m_cycleCount++，回到②
- ⑨收尾完成 → 全部 9 步重置 PENDING（含子步骤！），m_running=false，emit allFinished()

### 核心信号链路 (MainWindow::connectSignals)

```
BottomBar 按钮 → ProcessManager 控制槽
  startClicked → startCycle()
  pauseClicked → pauseCycle()
  softFinishClicked → requestFinish()
  immediateFinishClicked → finishCycle()
  resetClicked → emergencyStop() + clearAll()
  emergencyStopClicked → emergencyStop() + alarm

ProcessManager 状态 → UI 更新
  stepStateChanged(int,int) → FlowStepBar::setStepState(int,int)
  currentStepChanged(int) → FlowStepBar::setCurrentStep(int)
  substepStateChanged(int,int,int) → StepDetailPanel::updateSubstepState(int,int,int)
  realtimeDataUpdated(int,QVariantMap) → lambda → StepDetailPanel::updateRealtimeData
  cycleCompleted(int) → lambda → StatsCollector::incrementCount()

平台控制器 (ProcessManager 内部调用)
  DispensingPlatformController → MotorManager (轴2+3 home/moveTo/stop)
  PickupPlatformController     → MotorManager (轴13+14 home/moveTo/stop)
  两者均通过 AlarmLogger::raiseAlarm() 上报运动错误

AlarmLogger 状态 → UI 更新
  newAlarm(AlarmRecord)    → AlarmListWidget 追加报警行
  alarmResolved(int)       → AlarmListWidget 标记已恢复
  activeCountChanged(int)  → StatusBarWidget 报警计数

StatsCollector → UI 更新
  statsUpdated(count,hours,cycleTime) → StatusBarWidget 刷新产量/节拍/时长

FlowStepBar 用户交互
  stepClicked(int) → lambda(查询 ProcessManager) → StepDetailPanel::showStepDetail(...)

StepDetailPanel 参数编辑
  paramEdited(int,QString,double) → ProcessManager 更新参数

StatusBar 模式切换
  modeClicked() → QStackedWidget 切换 → switchToProductionMode/DebugMode
```

### UI 组件注意事项 (易错点)

1. **StatusBarWidget 的 modeLabel 已改为 QPushButton**：原来是 QLabel，现为 QPushButton，点击发射 `modeClicked()`。`setMode()` 需同时设置按钮文字和 stylesheet。
2. **StepDetailPanel 参数可编辑**：参数值为 QLineEdit，不要当成 QLabel 来用。清理时用 `m_paramValues` (QLabel*, QLineEdit* 对)，不是老的 `m_paramLabels`。
3. **子步骤状态必须同步重置**：循环重置和收尾重置时，除了 `m_stepStates[i] = PENDING`，还必须循环重置 `m_substepStates[i][j] = PENDING`。
4. **ProcessManager 信号用 int 不是 StepState 枚举**：避免 Widget 层依赖 Core 枚举类型。发射时用 `static_cast<int>(state)`。
5. **FlowStepBar::setSteps 可被多次调用**：ProductionWidget 构造和 MainWindow::initSystem 各调用一次，内部会 deleteLater 旧按钮并重建布局。
6. **窗口 resize(1024, 710)**：终端用户看到的实际窗口 1024×768，客户端 1024×710（标题栏占 ~58px）。
7. **BottomBar 的 setMode(0) 对应调试、setMode(1) 对应生产**：模式按钮文字是"接下来切到什么模式"，不是"当前模式"。
8. **颜色常量集中**：灰色 PENDING #2a2a3a/#666，蓝色 RUNNING #1565c0，绿色 COMPLETED #1b5e20，红色 FAILED #b71c1c，循环框 #0d1a0d/#2e7d32，回路箭头 #ffd740。

## 开发注意事项

### 添加新 IO 信号
1. 修改 `core/HardwareConfig.h`：更新 DI_COUNT/DO_COUNT，添加宏定义，更新 DI_NAMES/DI_AXIS_MAP
2. 如改变 DI 网格布局，更新 `widgets/IoMonitorWidget.cpp`

### MC_GPI/MC_GPO
- MC_GPI=4, MC_GPO=12 (来自 gts.h)，读写时直接使用
- **全部使用 GTN_* 网络版 API** (带 core 参数, 匹配 GNC-C610)

### GTS IO API 速查

| 操作 | API | 参数要点 |
|------|-----|---------|
| 读DI | `GTN_GetDi(core, MC_GPI, &bitmask)` | 返回32位掩码, bit0=GPI1=X0 |
| 写DO | `GTN_WriteDigitalOutputBit(core, &doBit, NULL)` | TDigitalOutputBit: mode=0, doType=12, **doIndex 10-13→Y9-Y12** |
| 回零 | `GTN_GoHome(core, axis, &homePrm)` | THomePrm (mode/moveDir/edge/...) |
| 轴使能 | `GTN_AxisOn(core, axis)` | |
| 点位运动 | `GTN_PrfTrap→GTN_SetTrapPrm→GTN_SetVel→GTN_SetPos→GTN_Update` | mask=1L<<(axis-1) |
| 状态 | `GTN_GetSts` / `GTN_GetPrfPos` / `GTN_GetHomeStatus` | |

### DO 控制细节
- `DO_INDEX_BASE=10`: 逻辑 DO1→doIndex10→Y9红灯, DO2→11→Y10黄灯, DO3→12→Y11绿灯, DO4→13→Y12蜂鸣器
- **硬件逻辑翻转**: doValue 0=开(亮), 1=关(灭) — `IoManager::setDO` 自动翻转
- **调试面板**: IoMonitorWidget 的 DO 标签直接可点击切换, 悬停变手型 `Qt::PointingHandCursor`

### 调试
- 所有 qDebug() 输出写入 `chipsetter_debug.log`
- 启动时日志路径、MC_GPI 值都会记录
- 前 3 次 IO 轮询结果详细记录（包括 bitmask 和提取的 values）
- GTS SDK 调用错误记录在 qWarning 中

### 已知问题
- Qt 5.15.2 存在 QButtonGroup::buttonClicked 弃用警告（预存问题）
- 软限位设置暂未实现运行时修改（需通过 GT_SetMcConfig）
- ~~**emergencyStop() 未重置子步骤状态**~~ — **已修复**：`emergencyStop()` 现在同步循环重置 `m_substepStates[i][j] = PENDING`，与 `completeCurrentStep()` 收尾/循环重置路径一致

### 远程调试 (VS Code)

详细踩坑记录见 `docs/remote-debug-guide.md`（15条）。以下为必知摘要：

**架构**: 开发机(192.168.1.100) → GDB(MSYS2 32-bit) → TCP :1234 → GNC(192.168.1.2) gdbserver → chipSetter.exe
**部署**: robocopy via SMB `\\192.168.1.2\share` → GNC 本地 `C:\Users\googol\Desktop\share\chipSetter\`
**远程管理**: WinRM (5985-5986) + schtasks /it (投递交互会话, 解决 Session 0 无 GUI)

---

### .vscode 工具链全景 (7个文件)

#### 文件清单

| 文件 | 职责 |
|------|------|
| `launch.json` | 3 种调试启动配置 (远程F5/远程仅连接/本地) |
| `tasks.json` | 8 个 Task, 含复合链路 `编译并部署 Debug 到目标机` |
| `package_debug.ps1` | windeployqt + googol/gts.dll + gdbserver + MinGW DLL → `debug_deploy/` |
| `deploy_to_target.ps1` | net use SMB 预建连接 → robocopy /E → GNC `\\192.168.1.2\share\chipSetter` |
| `start_gdbserver.ps1` | WinRM → PsExec → 手动 三级降级, 远端启动 gdbserver `--once 0.0.0.0:1234` |
| `start_direct.ps1` | 直接启动 chipSetter.exe (无 gdbserver), 用于实机测试 (非调试) |
| `stop_gdbserver.ps1` | WinRM 远端杀 chipSetter + gdbserver 进程, 删除计划任务 |

#### launch.json: 3 种调试配置

| 配置名 | 用途 | preLaunchTask | postDebugTask |
|--------|------|---------------|---------------|
| **远程调试 (GNC工控机)** | F5 一键: 编译→打包→部署→启动gdbserver→连接 | `编译并部署 Debug 到目标机` | `远端结束 gdbserver` |
| **远程调试 (仅连接,不编译)** | 仅连已有 gdbserver, 跳过编译部署 | `远端启动 gdbserver` | `远端结束 gdbserver` |
| **本地调试 (GDB)** | Mock 模式本地调试, 无硬件 | `编译 Debug 版本` | 无 |

**gdb setupCommands (防踩坑):**
- `set sysroot` — 清空 sysroot, 阻止 GDB 16.2 从远程读 exe 导致栈溢出
- `set auto-solib-add off` — 禁止自动加载远程 DLL

#### tasks.json: Task 依赖链

```
编译并部署 Debug 到目标机 (复合入口)
  ├── ① 远端结束 gdbserver    → stop_gdbserver.ps1 (WinRM)
  ├── ② 编译 Debug 版本
  │       └── make Debug (MSYS2)  → bash -lc "make -f Makefile.Debug -j4"
  │            (env: MSYSTEM=MINGW32  ← 关键! 写在env块不是命令里)
  ├── ③ 打包 Debug 部署包       → package_debug.ps1
  ├── ④ 部署到目标机            → deploy_to_target.ps1
  └── ⑤ 远端启动 gdbserver     → start_gdbserver.ps1
```

**独立 Task:**

| Task 名 | 类型 | 说明 |
|---------|------|------|
| `qmake Debug` | qmake | Mock 模式 qmake (无 `CONFIG+=real_gnc`) |
| `make Debug (MSYS2)` | bash | 增量编译, env 中设 `MSYSTEM=MINGW32` |
| `编译 Debug 版本` | 复合 | 仅编译 (依赖 `make Debug (MSYS2)`) |
| `打包 Debug 部署包` | powershell | package_debug.ps1 |
| `部署到目标机` | powershell | deploy_to_target.ps1 |
| `远端结束 gdbserver` | powershell | stop_gdbserver.ps1 |
| `远端启动 gdbserver` | powershell | start_gdbserver.ps1 |

**注意**: 新增 `.cpp/.h` 或修改 `.pro` 后需手动运行 `qmake Debug` task。

#### package_debug.ps1: 打包流程

```
① 检查 debug/chipSetter.exe 存在
② 清空并重建 debug_deploy/
③ 复制 debug/chipSetter.exe
④ windeployqt --no-translations --no-compiler-runtime (Qt DLL)
⑤ 复制 gdbserver.exe + libgcc_s_dw2-1.dll + libstdc++-6.dll + libwinpthread-1.dll
⑥ 复制 googol/ (gts.dll, gts.lib, gt_rn.dll, config.h, core1_20260625.cfg)
⑦ 复制 Qt plugins/ (platforms/qwindows.dll 等)
输出: debug_deploy/ (约 90 文件, 118MB)
```

#### deploy_to_target.ps1: 部署机制

```
① 从 ~\.chipsetter_cred.xml 读取加密凭证
② net use \\192.168.1.2\share /user:googol <password> (预建 SMB, 解决 1326 错误)
③ robocopy debug_deploy/ → \\192.168.1.2\share\chipSetter /E /R:3 /W:5
```

- 目标路径: `C:\Users\googol\Desktop\share\chipSetter\` (GNC 本地)
- robocopy 返回值: 0-7 正常, ≥8 失败

#### start_gdbserver.ps1: 三级降级启动

```
Method 1 (WinRM): New-PSSession → Invoke-Command 远端创建 .bat → schtasks /create /it 投递交互会话
Method 2 (PsExec): PsExec \\192.168.1.2 -s -d cmd /c "gdbserver.exe --once 0.0.0.0:1234 chipSetter.exe"
Method 3 (手动):   打印说明让用户 RDP 到 GNC 手动运行
```

**关键参数**: `gdbserver.exe --once 0.0.0.0:1234 chipSetter.exe`
- `--once`: gdb 断开后自动退出 (配合 postDebugTask 清理)
- `0.0.0.0:1234`: 必须绑定 IPv4 全地址, `:1234` 只绑 IPv6

#### stop_gdbserver.ps1: 远端清理

```
WinRM → Invoke-Command:
  Stop-Process chipSetter -Force
  Stop-Process gdbserver -Force
  schtasks /delete /tn "chipSetter-gdbserver" /f
```

---

#### F5 一键调试链路

```
①远端结束 gdbserver (杀旧进程, 解锁exe) → ②make -f Makefile.Debug -j4 (增量编译)
→ ③打包 (windeployqt + googol/gts.dll + gdbserver + MinGW DLL)
→ ④部署 (net use SMB → robocopy /E) → ⑤schtasks /it 远端启动 gdbserver
→ gdb 连接 → Shift+F5 (postDebugTask 自动远端杀进程)
```

#### 10 条铁律 (踩坑教训)

1. **gdb 配 `set sysroot`** — GDB 16.2 从远程读 exe 会栈溢出, 清空 sysroot 阻止
2. **不配 `sourceFileMap`** — debug info 已是 `D:\...` 格式, 映射反而导致断点 PENDING
3. **`stopAtEntry: false`** — 用 VS Code UI 断点, 避免停在 CRT 入口
4. **`cwd` 填本地路径** — 这是给本地 gdb 的, 不是远程的
5. **gdbserver 必须 `0.0.0.0:1234`** — `:1234` 只绑 IPv6, IPv4 连不上
6. **gdbserver 加 `--once`** — gdb 断开自动退出, 配合 postDebugTask 清理
7. **bash task 的 `MSYSTEM=MINGW32` 写在 `env` 块, 不写在命令里** — 登录 shell 的 /etc/profile 先于 -c 执行, 命令里 export 晚了 → 64位 g++ 编 32位 Qt → 链接静默失败 → exe 永不更新!
8. **qmake 不自动跑** — 每次跑重置 Makefile 时间戳, 破坏增量编译。**只有加新文件/改 .pro 才手动 qmake**
9. **package_debug.ps1 要跑 windeployqt + 复制 googol/** — 否则远程缺 Qt5*.dll 和 gts.dll
10. **deploy_to_target.ps1 用 `net use` 预建 SMB 连接** — VS Code 任务进程没有 GUI 凭证缓存, robocopy 报 1326

#### 日常操作

| 场景 | 操作 |
|------|------|
| 改代码调试 | 直接 F5 (增量编译自动部署) |
| 实机测试 | `powershell -File scripts/workflow.ps1 test` (全链路) |
| 快速部署 | `powershell -File scripts/workflow.ps1 quick` (已编译过) |
| 仅远端启动 | `powershell -File scripts/workflow.ps1 start` |
| 仅远端停止 | `powershell -File scripts/workflow.ps1 stop` |
| 新增 .cpp/.h | 先跑 `qmake Debug` task 再编译 |
| 改 .pro | 先跑 `qmake Debug` task 再编译 |
| GNC 端 | 需保持登录 (RDP/本地), 否则 schtasks /it 无交互会话可投递 |

#### 脚本速查

| 脚本 | 用途 | 关键参数 |
|------|------|----------|
| `scripts/workflow.ps1` | **统一调度** (编译→打包→部署→启动) | `test`/`quick`/`build`/`start`/`stop` |
| `scripts/build_debug.bat` | 一键编译 (双击运行) | `clean` 清旧文件 |
| `.vscode/package_debug.ps1` | windeployqt + googol/ + MinGW DLL → debug_deploy | 无参数, 全自动 |
| `.vscode/deploy_to_target.ps1` | net use SMB → robocopy /E → GNC | `-TargetShare` |
| `.vscode/start_gdbserver.ps1` | WinRM → PsExec → 手动 三级降级 | `-TargetIp -Port` |
| `.vscode/start_direct.ps1` | 直接启动 (无 gdbserver)，实机测试用 | `-TargetIp` |
| `.vscode/stop_gdbserver.ps1` | WinRM 远端杀 gdbserver + chipSetter | 需要凭证文件 |

凭证: `~\.chipsetter_cred.xml` (Export-Clixml 加密, 仅本机本用户可解密)
GNC 用户: `googol`, 需在 `Remote Management Users` 组
防火墙: TCP 1234 + 5985-5986 已放行

#### gdbserver 挂起陷阱

**问题**: gdbserver 启动目标进程后会将其**挂起**，等待 gdb 连接后发送 `continue`。如果不连 gdb，进程永远停在入口点——`MainWindowHandle=0`，看不到 GUI。

**区分**:
- **F5 调试**: 用 `start_gdbserver.ps1` → gdbserver 挂起进程 → gdb 连接 → continue → 窗口出现 ✅
- **实机测试**: 用 `start_direct.ps1` → 直接启动 chipSetter.exe → 无需 gdb → 窗口立即可见 ✅

#### Session 0 陷阱

**问题**: `schtasks /create /it` + `schtasks /run` 会把进程丢到 Session 0（无 GUI），即使有用户登录。

**根因**: `schtasks /run` 忽略 `/it` 标志，直接在非交互会话中执行。

**修复**: 不用 `/run`，调度时间设 5 秒后，等调度器自然触发。调度器会尊重 `/it` 并将进程放入用户会话 (Session 1+)。

```powershell
# 错误: schtasks /run → Session 0, 无 GUI
schtasks /create /tn "task" /tr "..." /sc once /st $time /it /f
schtasks /run /tn "task"   # ← 这行破坏了 /it

# 正确: 等调度器触发 → Session 1, 有 GUI
schtasks /create /tn "task" /tr "..." /sc once /st $time /it /f
Start-Sleep -Seconds 10    # 等调度器在用户会话中执行
```
