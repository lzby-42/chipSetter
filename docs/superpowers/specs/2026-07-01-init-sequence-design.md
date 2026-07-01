# 上电初始化流程设计

## 概述

将步骤①(初始化)从定时器模拟改为真实硬件驱动的事件驱动序列。

## 设计原则

- **前后端独立**: ProcessManager 只管调度，UI 只管展示，硬件操作委托给 MotorManager/IoManager/平台控制器
- **干净清爽**: 不新增文件，修改集中在 ProcessManager + MainWindow，新增代码 ~200 行

## 初始化序列 (5 阶段, 11 子步骤)

```
Phase 0: 亮黄灯 (同步)
  ├─ [0] 亮黄灯 — setDO(YELLOW,1)

Phase 1: 第一组回零 (组内并发, mask=bit12|bit7|bit0)
  ├─ [1] 胶盘回零 — axis1 mode=35 同步零位
  ├─ [2] 下顶针回零 — homeRequest(12)
  ├─ [3] 取晶臂转动回零 — homeRequest(7)
  └─ [4] 点胶平台回零 — DispensingPlatform::home() (2→3)

Phase 2: 第二组回零 (组内并发, mask=bit15|bit8|bit10|bit1)
  ├─ [5] 取晶平台XY回零 — PickupPlatform::home() (13→14)
  ├─ [6] 取晶平台W回零 — homeRequest(15)
  ├─ [7] 点胶臂转动回零 — homeRequest(8)
  └─ [8] 取晶臂上下回零 — homeRequest(10)

Phase 3: 点胶臂旋出 (单轴, mask=bit8)
  └─ [9] 点胶臂旋出5450pulse — moveRequest(8, 5450)

Phase 4: 点胶上下回零 (单轴, mask=bit9)
  └─ [10] 点胶上下回零 — homeRequest(9)
```

## 状态机

```
startInitPhase(phase):
  ├─ 设置 m_initPhase
  ├─ 设置 m_initPendingMask
  ├─ 启动超时定时器
  ├─ 更新子步骤状态为 RUNNING
  └─ 发射硬件操作

信号到达 → onInitXxxFinished:
  ├─ 检查 m_currentStepIdx==STEP_INIT && mask位有效
  ├─ 成功: 清除mask位, mask==0 → advanceInitPhase()
  └─ 失败: retryFailedAxis() → 重试1次 → 仍失败 → abortInit()

advanceInitPhase():
  ├─ 标记当前子步骤 COMPLETED
  ├─ 下一Phase存在 → startInitPhase(nextPhase)
  └─ 全部完成 → completeCurrentStep()
```

## 失败处理

- **单轴失败**: 只重试该轴 1 次
- **重试仍失败**: 停止全部轴, 黄灯灭→红灯亮, AlarmLogger FATAL 报警, 步骤标记 FAILED
- **超时**: Phase1/2/4 60s, Phase3 30s → 等同重试失败

## ProcessManager 变更

| 新增 | 说明 |
|------|------|
| `setIoManager(IoManager*)` | 注入, 控制黄灯/红灯 |
| `setMotorManager(MotorManager*)` | 注入, 连接 homeFinished/moveFinished |
| `enum InitPhase` | 6 状态: PHASE0~PHASE4 + INIT_FAILED |
| `int m_initPendingMask` | 位掩码跟踪并发完成 |
| `QMap<int,int> m_initRetryCount` | 单轴重试计数 |
| `QTimer* m_initPhaseTimer` | 单次超时 (60s/30s) |

**关键变更点**:
- `executeStep(STEP_INIT)` → 调 `startInitPhase(PHASE0)` 不启动 m_stepTimer
- `emergencyStop()` → 增加 `stopAllInitAxes()`
- `setDispensingPlatform/setPickupPlatform` → 增加 homeFinished 信号连接

## MainWindow 变更

- `initSystem()`: 注入 `m_ioManager` 和 `m_motorManager` 到 ProcessManager
- `connectSignals()`: 连接 `initError` → AlarmLogger + 步骤完成时灯色切换

## 不涉及的文件

- 平台控制器 (DispensingPlatform/PickupPlatform) — 接口不变
- MotorManager/IoManager — 接口不变
- 全部 Widget — 只通过现有 signal/slot 接收状态更新
- HardwareConfig.h — 常量已就位

## 位掩码约定

位掩码使用 `(1 << axisId)` 直接映射轴ID:

| 掩码 | 轴 | Phase | 信号源 |
|------|-----|-------|--------|
| bit(0) | — | Phase1 | DispensingPlatform::homeFinished |
| bit(1) | — | Phase2 | PickupPlatform::homeFinished |
| bit(7) | 取晶臂转动 | Phase1 | MotorManager::homeFinished(7) |
| bit(8) | 点胶臂转动 | Phase2,3 | homeFinished(8) / moveFinished(8) |
| bit(9) | 点胶上下 | Phase4 | homeFinished(9) |
| bit(10) | 取晶上下 | Phase2 | homeFinished(10) |
| bit(12) | 下顶针 | Phase1 | homeFinished(12) |
| bit(15) | 取晶平台W | Phase2 | homeFinished(15) |

平台控制器使用 bit(0)/bit(1) 虚拟位, 不与真实轴ID冲突。

## 验证

1. 编译: `powershell -File scripts/workflow.ps1 build`
2. 单元测试: 修改 test_process_manager 跳过 INIT 硬件依赖, 或 Mock MotorManager
3. 实机: 启动→观察黄灯→各组回零→点胶臂到位→绿灯→进入上料步骤
