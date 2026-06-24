# 平台控制使用说明

## 概述

固晶机有两个 XY 平台，各自通过独立的 PlatformController 控制:

| 平台 | 类 | 轴 | 能力 |
|------|-----|-----|------|
| 点胶平台 | `DispensingPlatformController` | X=2, Y=3 | 回零 + 点位移动 |
| 取晶平台 | `PickupPlatformController` | X=10, Y=11 | 回零 + 点位移动 |

平台控制器通过 `MotorManager` 间接发送运动指令，通过 `AlarmLogger` 上报错误。

## 快速开始

```cpp
// 创建 (MainWindow 中完成)
auto* dispensing = new DispensingPlatformController(motorManager, alarmLogger, this);
auto* pickup     = new PickupPlatformController(motorManager, alarmLogger, this);

// 初始化 (连接信号)
dispensing->initialize();
pickup->initialize();

// 注入 ProcessManager
processManager->setDispensingPlatform(dispensing);
processManager->setPickupPlatform(pickup);
```

## 回零流程

每个平台回零按 X→Y 顺序执行:

```cpp
// 发起回零 (异步)
dispensing->home();

// 监听完成
connect(dispensing, &DispensingPlatformController::homeFinished,
        this, [](bool success) {
    if (success) {
        qDebug() << "点胶平台回零完成";
    } else {
        qWarning() << "点胶平台回零失败 — 查看报警栏";
    }
});
```

**回零方式**: 通过限位开关触发（非独立 HOME 传感器）。轴向限位方向移动，触发限位=找到零点。

**状态机**:
```
IDLE → HOMING_X → HOMING_Y → DONE
         ↓失败        ↓失败
        IDLE         IDLE (emit homeFinished(false))
```

**约束**:
- 回零中调用 `moveTo()` 被忽略
- 移动中调用 `home()` 被忽略
- 失败后再次调用 `home()` 从头重试

## 点位移动

双轴同时发令，分别到达，不插补:

```cpp
// 移动到绝对坐标 (异步, 双轴并发)
dispensing->moveTo(150.0, 200.0);  // 单位: mm

connect(dispensing, &DispensingPlatformController::moveFinished,
        this, [](bool success) {
    if (success) {
        qDebug() << "到位";
    }
});
```

**完成判断**: 两根轴都完成 (MotorManager emit moveFinished) 才 emit moveFinished。

**失败处理**: 任一根轴失败 (success=false)，emit moveFinished(false)，同时上报报警栏。

**速度/加速度**: 从 `MotorManager::axisState(axisId)` 读取当前轴参数，不在 PlatformController 中存储。修改速度需通过 MotorManager:
```cpp
MotorAxis params = motorManager->axisState(AXIS_DISPENSE_X);
params.velocity = 80.0;
motorManager->updateAxisParams(AXIS_DISPENSE_X, params);
```

## 错误处理

所有运动异常通过 `AlarmLogger::raiseAlarm()` 上报:

| 场景 | 级别 | source | message 示例 |
|------|------|--------|-------------|
| X轴回零失败 | WARNING | "点胶平台" | "X轴回零: home error, stage=5" |
| Y轴回零失败 | WARNING | "取晶平台" | "Y轴回零: home error, stage=3" |
| 双轴移动有一失败 | WARNING | "点胶平台" | "点位移动: X/Y轴移动异常" |

WARNING 级别不阻塞流程，由 ProcessManager 根据 `homeFinished(false)` / `moveFinished(false)` 决定后续动作。

## 多点移动示例 (ProcessManager 中)

```cpp
// ProcessManager 维护点位列表
QVector<QPointF> m_waferPositions;  // 晶圆坐标列表
int m_currentWafer = 0;

void ProcessManager::onPickupMoveFinished(bool success)
{
    if (!success) {
        pauseCycle();
        return;
    }

    // 执行拾取动作 (IO + 臂控制)...

    m_currentWafer++;
    if (m_currentWafer < m_waferPositions.size()) {
        // 移动到下一个晶圆
        QPointF p = m_waferPositions[m_currentWafer];
        m_pickupPlatform->moveTo(p.x(), p.y());
    } else {
        // 本步骤完成, 推进到下一步
        completeCurrentStep();
    }
}
```

## 跨平台协调

两个平台的操作通过 ProcessManager 步骤天然串行化:

```
Step ② 上料:   点胶平台.home()         → homeFinished →
               点胶平台.moveTo(取料位)   → moveFinished →
Step ③ 定位:   取晶平台.home()          → homeFinished →
               取晶平台.moveTo(视觉点)   → moveFinished →
Step ⑤ 拾取:   取晶平台.moveTo(晶圆1)   → moveFinished → 拾取臂执行
...
```

## stop() 行为

```cpp
dispensing->stop();  // 立即停两个轴 + 重置状态机
```

- 对两个轴分别调用 `MotorManager::stopMove()` (axisOff + emit moveFinished)
- m_homeState 回到 IDLE
- m_pendingMoves 清零
- 可在任何状态调用
