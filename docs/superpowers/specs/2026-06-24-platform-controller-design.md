# Platform Controller 设计规格

**日期**: 2026-06-24
**状态**: 已确认
**范围**: 新增 `DispensingPlatformController` 和 `PickupPlatformController`

---

## 1. 概述

为固晶机的两个 XY 平台创建独立的控制接口，封装自动回零序列和多点移动，将结果上报报警栏。

### 两个平台

| 平台 | 类名 | 轴 | Home 顺序 |
|------|------|-----|-----------|
| 点胶平台 | `DispensingPlatformController` | X=2, Y=3 | X → Y |
| 取晶平台 | `PickupPlatformController` | X=10, Y=11 | X → Y |

两个平台目前都不管 W/旋转轴。

---

## 2. 架构位置

```
ProcessManager → PlatformController → MotorManager → IGncController → GTS SDK
   (新增注入)        (新增类)            (已有)          (已有)
                        ↓
                   AlarmLogger
                    (报错栏)
```

- **PlatformController** 通过 `MotorManager*` 间接控制，复用已有的单轴运动、单位换算、完成检测
- **ProcessManager** 注入 PlatformController，在流程步骤中调用 `home()` / `moveTo()`，监听 `homeFinished` / `moveFinished` 推进子步骤
- **AlarmLogger** 注入 PlatformController，运动异常时写入报错栏

### 为什么不直接操作 IGncController

MotorManager 已有的能力不应重复实现：
- `moveRequest` 包含参数校验（速度/加速度/软限位）、`mmToPulse` 单位换算
- 轮询检测移动完成（状态位 0x400）、回零完成（`homeSts.run==0 && homeSts.error==0 && homeSts.stage==9`）
- `homeFinished` / `moveFinished` 信号已就绪

PlatformController 的职责是**序列编排**和**平台错误上报**，不是重新实现单轴控制。

---

## 3. 接口定义

### 3.1 DispensingPlatformController（点胶平台）

```cpp
class DispensingPlatformController : public QObject {
    Q_OBJECT
public:
    explicit DispensingPlatformController(MotorManager *motor,
                                          AlarmLogger *alarm,
                                          QObject *parent = nullptr);

    // 初始化：连接 MotorManager 信号
    void initialize();

    // --- 动作 ---
    void home();                          // 自动回零：X → Y
    void moveTo(double xMm, double yMm); // XY 移动到绝对坐标（并发，非插补）
    void stop();                          // 停止当前动作（急停两个轴）

    // --- 状态查询 ---
    bool isHomed() const;
    bool isMoving() const;
    double currentX() const;
    double currentY() const;

signals:
    void homeFinished(bool success);
    void moveFinished(bool success);
    void positionUpdated(double x, double y);

private slots:
    void onMotorHomeFinished(int axisId, bool success, int homeStage);
    void onMotorMoveFinished(int axisId, bool success);

private:
    enum HomeState { IDLE, HOMING_X, HOMING_Y, DONE };
    HomeState m_homeState = IDLE;

    int m_pendingMoves = 0;  // moveTo 等待完成的轴数
    bool m_moveFailed = false;

    void reportError(const QString &action, const QString &detail);
};
```

**MotorManager 信号连接：**
- `moveRequest(int axisId, double pos, double vel, double acc, double dec)` — 槽
- `homeRequest(int axisId)` — 槽
- `moveFinished(int axisId, bool success)` → `onMotorMoveFinished`
- `homeFinished(int axisId, bool success)` → `onMotorHomeFinished`
- `positionUpdated(int axisId, double pos)` → 聚合 X/Y，emit `positionUpdated(x, y)`

### 3.2 PickupPlatformController（取晶平台）

接口与 `DispensingPlatformController` 完全对称，仅轴 ID 不同（X=10, Y=11）。

### 3.3 双轴移动详细逻辑

```
moveTo(x, y)
  │
  ├→ m_pendingMoves = 2
  ├→ m_moveFailed = false
  ├→ motor->moveRequest(AXIS_X, x, vel, acc, dec)
  ├→ motor->moveRequest(AXIS_Y, y, vel, acc, dec)
  │
  └→ [等待] onMotorMoveFinished:
        if (!success) m_moveFailed = true;
        m_pendingMoves--;
        if (m_pendingMoves == 0)
            if (m_moveFailed)
                reportError("点位移动", "X/Y轴移动失败");
                emit moveFinished(false);
            else
                emit moveFinished(true);
```

两个轴同时发令、分别到达，不走插补。任一根轴失败即整体失败。

### 3.4 回零序列详细逻辑

```
home()
  │
  ├→ m_homeState = HOMING_X
  ├→ motor->homeRequest(AXIS_X)
  │
  └→ onMotorHomeFinished(AXIS_X, success, homeStage):
        if (!success)
            reportError("点胶平台回零", "X轴回零失败");
            emit homeFinished(false);
            m_homeState = IDLE;
            return;
        m_homeState = HOMING_Y;
        motor->homeRequest(AXIS_Y);
      │
      └→ onMotorHomeFinished(AXIS_Y, success, homeStage):
           if (!success)
               reportError("点胶平台回零", "Y轴回零失败");
               emit homeFinished(false);
               m_homeState = IDLE;
               return;
           m_homeState = DONE;
           emit homeFinished(true);
```

### 3.5 stop() 逻辑

```
stop()
  ├→ motor->stopMove(AXIS_X)   // axisOff + emit moveFinished
  ├→ motor->stopMove(AXIS_Y)
  ├→ m_homeState = IDLE        // 中断回零序列
  ├→ m_pendingMoves = 0
  └→ m_moveFailed = false
```

---

## 4. 错误处理 → 报错栏

### 错误来源与级别

| 场景 | 级别 | source | message 示例 |
|------|------|--------|-------------|
| 回零 X 失败 | WARNING | "点胶平台" | "X轴回零失败: home error code=3" |
| 回零 Y 失败 | WARNING | "点胶平台" | "Y轴回零失败: home timeout" |
| 移动 X 失败 | WARNING | "点胶平台" | "X轴移动失败: 超出软限位" |
| 移动 Y 失败 | WARNING | "取晶平台" | "Y轴移动失败: 驱动器报警" |
| 双轴有一失败 | WARNING | "点胶平台" | "点位移动失败: X/Y轴移动异常" |

### 上报方式

```cpp
void DispensingPlatformController::reportError(const QString &action, const QString &detail) {
    m_alarm->raiseAlarm(ALARM_LEVEL_WARNING,
                        QStringLiteral("点胶平台"),
                        QStringLiteral("%1: %2").arg(action, detail));
}
```

- 错误不恢复（WARNING 级别不会阻塞流程，由 ProcessManager 决定是否暂停）
- 如果要致命级（如驱动器过流），由 MotorManager 的 `motorError` 信号走独立通道上报

### 错误恢复

- `home()` / `moveTo()` 再次调用时自动清除之前的错误状态
- 回零序列中某步失败后 `m_homeState` 回到 IDLE，下次 `home()` 从头开始

---

## 5. 多点移动与跨平台协调

### 多点移动（晶圆阵列）

由 **ProcessManager** 维护点位列表，逐点下发：

```
ProcessManager:
  m_waferPositions = [(x1,y1), (x2,y2), ..., (xn,yn)]
  m_currentWaferIdx = 0

  executeStep(STEP_PICK):
    if (m_currentWaferIdx >= m_waferPositions.size()) → 步进到下一子步骤
    auto [x, y] = m_waferPositions[m_currentWaferIdx]
    m_pickupPlatform->moveTo(x, y)

  onPickupMoveFinished(success):
    if (success)
        m_currentWaferIdx++
        // 执行拾取动作...
```

PlatformController 只负责"走到当前给定点"，不维护点位列表。

### 跨平台协调

由 ProcessManager 在流程步骤中编排。例如：

```
Step ② 上料:   点胶平台.moveTo(取料位)  → 完成 →
Step ③ 定位:   取晶平台.home()          → 完成 → 取晶平台.moveTo(视觉定位点)
Step ④ 顶晶:   (纯IO动作, 无运动)
Step ⑤ 拾取:   取晶平台.moveTo(晶圆1)   → 完成 → 拾取臂动作
...
```

两个平台的操作通过 ProcessManager 步骤天然串行化，不会出现竞争。

---

## 6. 文件清单

### 新增文件

| 文件 | 职责 |
|------|------|
| `core/DispensingPlatformController.h` | 点胶平台接口声明 |
| `core/DispensingPlatformController.cpp` | 点胶平台实现 |
| `core/PickupPlatformController.h` | 取晶平台接口声明 |
| `core/PickupPlatformController.cpp` | 取晶平台实现 |
| `docs/platform-control-guide.md` | 平台控制使用说明文档 |

### 修改文件

| 文件 | 变更 |
|------|------|
| `chipSetter.pro` | 添加4个新 .h/.cpp |
| `core/ProcessManager.h/.cpp` | 注入两个 PlatformController，在 executeStep 中调用 |
| `mainwindow.h/.cpp` | 创建 PlatformController 实例，注入 ProcessManager |

---

## 7. 速度/加速度默认值

### 点胶平台 (轴2,3)

| 参数 | 默认值 | 说明 |
|------|--------|------|
| moveVel | 50 mm/s | 移动速度 |
| moveAcc | 500 mm/s² | 加速度 |
| moveDec | 500 mm/s² | 减速度 |
| homeHighSpeed | 20 mm/s | 回零高速段 |
| homeLowSpeed | 5 mm/s | 回零低速段 |
| homeAcc | 200 mm/s² | 回零加速度 |
| homeOffset | 0 mm | 回零偏移 |

### 取晶平台 (轴10,11)

同点胶平台默认值，实际调优时独立在 MotorManager 中修改。

### 参数来源

优先从 `MotorManager::axisState(axisId).params` 读取；如无则用上表默认值。不在 PlatformController 中存储运动参数。

---

## 8. 约束与注意事项

### 线程模型

所有操作在主线程（Qt 事件循环）中执行：
- `home()` / `moveTo()` 在调用线程发出 `moveRequest` / `homeRequest`，立即返回
- MotorManager 的 50ms 轮询定时器在主线程中触发完成检测
- 没有新线程或工作线程

### 并发限制

- `home()` 期间调用 `moveTo()` 被忽略（`m_homeState != IDLE` 时返回）
- `moveTo()` 期间调用 `home()` 被忽略（`m_pendingMoves > 0` 时返回）
- `stop()` 可在任何状态调用

### 已知问题（需同步修复）

- **MotorManager `homeFinished` 检测**: 当前使用旧式 `stage==9` 判断回零完成。GncControllerImpl 使用 `GT_GoHome`（旧 API），这是匹配的。但如果将来切换到 `GT_ExecuteStandardHome`（标准 API，stage=100 完成），检测条件需同步更新。
- **GncControllerImpl 单位换算**: `moveAbsolute` 中 `pos * 1000.0` 可能与 MotorManager `mmToPulse` 产生双重换算，实现前需实测确认。

---

## 9. 文档要求

在 `docs/platform-control-guide.md` 中编写使用说明，内容包含：

1. **概述** — 两个平台的轴组成和接口能力
2. **快速开始** — `initialize()` + `home()` + `moveTo()` 的最小示例代码
3. **回零流程** — X→Y 顺序、完成信号、失败处理
4. **点位移动** — 双轴并发机制、完成判断
5. **错误处理** — 报警栏集成方式、常见错误码含义
6. **多点移动示例** — ProcessManager 中驱动多点序列的代码模板
7. **跨平台协调** — 如何在流程步骤中编排两个平台的操作
8. **参数配置** — 速度/加速度在 MotorManager 中的设置方法
