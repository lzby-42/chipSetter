# Platform Controller 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 DispensingPlatformController 和 PickupPlatformController，封装点胶/取晶平台的回零序列和双轴点位移动，错误上报报警栏。

**Architecture:** 两个独立 QObject 类，注入 MotorManager* 和 AlarmLogger*。内部状态机管理回零序列 (IDLE→HOMING_X→HOMING_Y→DONE)，双轴移动并发发令等待两轴都完成。ProcessManager 在流程步骤中调用。

**Tech Stack:** Qt 5.15.2 C++, 信号/槽驱动, GTS SDK (通过 MotorManager 间接调用)

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| **Create** | `core/DispensingPlatformController.h` | 点胶平台接口声明 |
| **Create** | `core/DispensingPlatformController.cpp` | 点胶平台实现 |
| **Create** | `core/PickupPlatformController.h` | 取晶平台接口声明 |
| **Create** | `core/PickupPlatformController.cpp` | 取晶平台实现 |
| **Modify** | `chipSetter.pro` | 添加 4 个新文件 |
| **Modify** | `mainwindow.h` | 声明两个 PlatformController 成员和 include |
| **Modify** | `mainwindow.cpp` | 创建并注入 PlatformController |
| **Modify** | `core/ProcessManager.h` | 注入两个 PlatformController 指针 |
| **Create** | `docs/platform-control-guide.md` | 平台控制使用说明文档 |

---

### Task 1: 创建 DispensingPlatformController.h

**Files:**
- Create: `core/DispensingPlatformController.h`

- [ ] **Step 1: 写入头文件**

```cpp
/**
 * @file DispensingPlatformController.h
 * @brief 点胶平台控制器 — XY回零序列 + 双轴点位移动
 *
 * 职责:
 *   - 封装点胶平台 (轴2=X, 轴3=Y) 的自动回零 (X→Y序列)
 *   - 封装双轴并发点位移动 (非插补)
 *   - 错误上报 AlarmLogger
 *
 * 依赖: MotorManager, AlarmLogger, HardwareConfig.h
 */

#ifndef DISPENSINGPLATFORMCONTROLLER_H
#define DISPENSINGPLATFORMCONTROLLER_H

#include <QObject>
#include "core/HardwareConfig.h"

class MotorManager;
class AlarmLogger;

class DispensingPlatformController : public QObject
{
    Q_OBJECT

public:
    explicit DispensingPlatformController(MotorManager *motor,
                                          AlarmLogger *alarm,
                                          QObject *parent = nullptr);

    void initialize();

    // --- 动作 ---
    void home();                          // 自动回零: X→Y 序列
    void moveTo(double xMm, double yMm); // XY 移动到绝对坐标 (并发)
    void stop();                          // 停止当前动作

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
    void onMotorPositionUpdated(int axisId, double position);

private:
    void reportError(const QString &action, const QString &detail);

    // 点胶平台轴
    static constexpr int AXIS_X = AXIS_DISPENSE_X;   // 2
    static constexpr int AXIS_Y = AXIS_DISPENSE_Y;   // 3

    MotorManager* m_motor;   // 注入, 不拥有
    AlarmLogger*  m_alarm;   // 注入, 不拥有

    enum HomeState { IDLE, HOMING_X, HOMING_Y, DONE };
    HomeState m_homeState = IDLE;

    int  m_pendingMoves = 0;   // 当前 moveTo 等待完成的轴数
    bool m_moveFailed = false; // 当前 moveTo 是否有轴失败

    double m_currentX = 0.0;
    double m_currentY = 0.0;
};

#endif // DISPENSINGPLATFORMCONTROLLER_H
```

- [ ] **Step 2: 验证头文件语法**

```bash
cd tests && qmake && make -j4
```

(此时编译会因缺少 .cpp 而失败，仅验证头文件无语法错误)

---

### Task 2: 修改 chipSetter.pro — 添加全部新文件

**Files:**
- Modify: `chipSetter.pro`

- [ ] **Step 1: 在 SOURCES 块中添加两个 .cpp**

在 `core/ProcessManager.cpp \` 之后插入:
```
    core/DispensingPlatformController.cpp \
    core/PickupPlatformController.cpp \
```

- [ ] **Step 2: 在 HEADERS 块中添加两个 .h**

在 `core/ProcessManager.h \` 之后插入:
```
    core/DispensingPlatformController.h \
    core/PickupPlatformController.h \
```

- [ ] **Step 3: 验证 .pro 语法**

```bash
qmake chipSetter.pro
```

Expected: 无错误输出, 生成 Makefile.Debug 且包含新文件


### Task 3: 创建 DispensingPlatformController.cpp

**Files:**
- Create: `core/DispensingPlatformController.cpp`

- [ ] **Step 1: 写入实现文件**

```cpp
/**
 * @file DispensingPlatformController.cpp
 * @brief 点胶平台控制器实现
 */

#include "core/DispensingPlatformController.h"
#include "core/MotorManager.h"
#include "core/AlarmLogger.h"
#include <QDebug>

DispensingPlatformController::DispensingPlatformController(
        MotorManager *motor, AlarmLogger *alarm, QObject *parent)
    : QObject(parent)
    , m_motor(motor)
    , m_alarm(alarm)
{
}

void DispensingPlatformController::initialize()
{
    // 连接 MotorManager 信号
    connect(m_motor, &MotorManager::homeFinished,
            this, &DispensingPlatformController::onMotorHomeFinished);
    connect(m_motor, &MotorManager::moveFinished,
            this, &DispensingPlatformController::onMotorMoveFinished);
    connect(m_motor, &MotorManager::positionUpdated,
            this, &DispensingPlatformController::onMotorPositionUpdated);

    qDebug() << "[DispensingPlatform] 初始化完成, AXIS_X=" << AXIS_X << "AXIS_Y=" << AXIS_Y;
}

// ============================================================
// 回零: X → Y 序列
// ============================================================

void DispensingPlatformController::home()
{
    if (m_homeState != IDLE && m_homeState != DONE) {
        qDebug() << "[DispensingPlatform] home() 忽略 — 已在回零中, state=" << m_homeState;
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[DispensingPlatform] home() 忽略 — 移动中";
        return;
    }

    qDebug() << "[DispensingPlatform] 开始回零序列: X→Y";
    m_homeState = HOMING_X;
    m_motor->homeRequest(AXIS_X);
}

void DispensingPlatformController::onMotorHomeFinished(int axisId, bool success, int homeStage)
{
    Q_UNUSED(homeStage);

    if (m_homeState == IDLE || m_homeState == DONE)
        return;

    if (axisId == AXIS_X && m_homeState == HOMING_X) {
        if (!success) {
            reportError("X轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[DispensingPlatform] X轴回零完成, 开始Y轴";
        m_homeState = HOMING_Y;
        m_motor->homeRequest(AXIS_Y);
        return;
    }

    if (axisId == AXIS_Y && m_homeState == HOMING_Y) {
        if (!success) {
            reportError("Y轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[DispensingPlatform] 回零序列完成";
        m_homeState = DONE;
        emit homeFinished(true);
        return;
    }
}

// ============================================================
// 双轴并发移动 (非插补)
// ============================================================

void DispensingPlatformController::moveTo(double xMm, double yMm)
{
    if (m_homeState == HOMING_X || m_homeState == HOMING_Y) {
        qDebug() << "[DispensingPlatform] moveTo() 忽略 — 回零中";
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[DispensingPlatform] moveTo() 忽略 — 上一次移动未完成";
        return;
    }

    qDebug() << "[DispensingPlatform] moveTo(" << xMm << "," << yMm << ")";

    const MotorAxis& axisX = m_motor->axisState(AXIS_X);
    const MotorAxis& axisY = m_motor->axisState(AXIS_Y);

    m_pendingMoves = 2;
    m_moveFailed = false;

    m_motor->moveRequest(AXIS_X, xMm, axisX.velocity, axisX.acceleration, axisX.deceleration);
    m_motor->moveRequest(AXIS_Y, yMm, axisY.velocity, axisY.acceleration, axisY.deceleration);
}

void DispensingPlatformController::onMotorMoveFinished(int axisId, bool success)
{
    if (m_pendingMoves == 0)
        return;

    if (axisId != AXIS_X && axisId != AXIS_Y)
        return;

    if (!success) {
        m_moveFailed = true;
        qWarning() << "[DispensingPlatform] 轴" << axisId << "移动失败";
    }

    m_pendingMoves--;

    if (m_pendingMoves == 0) {
        if (m_moveFailed) {
            reportError("点位移动", "X/Y轴移动异常");
            emit moveFinished(false);
        } else {
            emit moveFinished(true);
        }
    }
}

// ============================================================
// 停止
// ============================================================

void DispensingPlatformController::stop()
{
    qDebug() << "[DispensingPlatform] stop()";
    m_motor->stopMove(AXIS_X);
    m_motor->stopMove(AXIS_Y);
    m_homeState = IDLE;
    m_pendingMoves = 0;
    m_moveFailed = false;
}

// ============================================================
// 状态查询
// ============================================================

bool DispensingPlatformController::isHomed() const
{
    return m_homeState == DONE;
}

bool DispensingPlatformController::isMoving() const
{
    return m_pendingMoves > 0
        || m_homeState == HOMING_X
        || m_homeState == HOMING_Y;
}

double DispensingPlatformController::currentX() const
{
    return m_currentX;
}

double DispensingPlatformController::currentY() const
{
    return m_currentY;
}

// ============================================================
// 位置更新
// ============================================================

void DispensingPlatformController::onMotorPositionUpdated(int axisId, double position)
{
    if (axisId == AXIS_X) {
        m_currentX = position;
        emit positionUpdated(m_currentX, m_currentY);
    } else if (axisId == AXIS_Y) {
        m_currentY = position;
        emit positionUpdated(m_currentX, m_currentY);
    }
}

// ============================================================
// 错误上报
// ============================================================

void DispensingPlatformController::reportError(const QString &action, const QString &detail)
{
    m_alarm->raiseAlarm(ALARM_LEVEL_WARNING,
                        QStringLiteral("点胶平台"),
                        QStringLiteral("%1: %2").arg(action, detail));
}
```

- [ ] **Step 2: 写入完成**

(编译验证推迟到 Task 5 之后所有 .cpp 文件就位)


### Task 4: 创建 PickupPlatformController.h

**Files:**
- Create: `core/PickupPlatformController.h`

- [ ] **Step 1: 写入头文件**

```cpp
/**
 * @file PickupPlatformController.h
 * @brief 取晶平台控制器 — XY回零序列 + 双轴点位移动
 *
 * 职责:
 *   - 封装取晶平台 (轴10=X, 轴11=Y) 的自动回零 (X→Y序列)
 *   - 封装双轴并发点位移动 (非插补)
 *   - 错误上报 AlarmLogger
 *
 * 注: W轴(轴12)暂不纳入控制范围
 *
 * 依赖: MotorManager, AlarmLogger, HardwareConfig.h
 */

#ifndef PICKUPPLATFORMCONTROLLER_H
#define PICKUPPLATFORMCONTROLLER_H

#include <QObject>
#include "core/HardwareConfig.h"

class MotorManager;
class AlarmLogger;

class PickupPlatformController : public QObject
{
    Q_OBJECT

public:
    explicit PickupPlatformController(MotorManager *motor,
                                      AlarmLogger *alarm,
                                      QObject *parent = nullptr);

    void initialize();

    // --- 动作 ---
    void home();                          // 自动回零: X→Y 序列
    void moveTo(double xMm, double yMm); // XY 移动到绝对坐标 (并发)
    void stop();                          // 停止当前动作

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
    void onMotorPositionUpdated(int axisId, double position);

private:
    void reportError(const QString &action, const QString &detail);

    // 取晶平台轴
    static constexpr int AXIS_X = AXIS_PICKUP_X;   // 10
    static constexpr int AXIS_Y = AXIS_PICKUP_Y;   // 11

    MotorManager* m_motor;   // 注入, 不拥有
    AlarmLogger*  m_alarm;   // 注入, 不拥有

    enum HomeState { IDLE, HOMING_X, HOMING_Y, DONE };
    HomeState m_homeState = IDLE;

    int  m_pendingMoves = 0;
    bool m_moveFailed = false;

    double m_currentX = 0.0;
    double m_currentY = 0.0;
};

#endif // PICKUPPLATFORMCONTROLLER_H
```

- [ ] **Step 2: 验证编译**

```bash
cd tests && qmake && make -j4
```

(仅验证头文件语法，缺少 .cpp 会链接失败)


### Task 5: 创建 PickupPlatformController.cpp

**Files:**
- Create: `core/PickupPlatformController.cpp`

- [ ] **Step 1: 写入实现文件**

```cpp
/**
 * @file PickupPlatformController.cpp
 * @brief 取晶平台控制器实现
 */

#include "core/PickupPlatformController.h"
#include "core/MotorManager.h"
#include "core/AlarmLogger.h"
#include <QDebug>

PickupPlatformController::PickupPlatformController(
        MotorManager *motor, AlarmLogger *alarm, QObject *parent)
    : QObject(parent)
    , m_motor(motor)
    , m_alarm(alarm)
{
}

void PickupPlatformController::initialize()
{
    connect(m_motor, &MotorManager::homeFinished,
            this, &PickupPlatformController::onMotorHomeFinished);
    connect(m_motor, &MotorManager::moveFinished,
            this, &PickupPlatformController::onMotorMoveFinished);
    connect(m_motor, &MotorManager::positionUpdated,
            this, &PickupPlatformController::onMotorPositionUpdated);

    qDebug() << "[PickupPlatform] 初始化完成, AXIS_X=" << AXIS_X << "AXIS_Y=" << AXIS_Y;
}

// ============================================================
// 回零: X → Y 序列
// ============================================================

void PickupPlatformController::home()
{
    if (m_homeState != IDLE && m_homeState != DONE) {
        qDebug() << "[PickupPlatform] home() 忽略 — 已在回零中, state=" << m_homeState;
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[PickupPlatform] home() 忽略 — 移动中";
        return;
    }

    qDebug() << "[PickupPlatform] 开始回零序列: X→Y";
    m_homeState = HOMING_X;
    m_motor->homeRequest(AXIS_X);
}

void PickupPlatformController::onMotorHomeFinished(int axisId, bool success, int homeStage)
{
    Q_UNUSED(homeStage);

    if (m_homeState == IDLE || m_homeState == DONE)
        return;

    if (axisId == AXIS_X && m_homeState == HOMING_X) {
        if (!success) {
            reportError("X轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[PickupPlatform] X轴回零完成, 开始Y轴";
        m_homeState = HOMING_Y;
        m_motor->homeRequest(AXIS_Y);
        return;
    }

    if (axisId == AXIS_Y && m_homeState == HOMING_Y) {
        if (!success) {
            reportError("Y轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[PickupPlatform] 回零序列完成";
        m_homeState = DONE;
        emit homeFinished(true);
        return;
    }
}

// ============================================================
// 双轴并发移动
// ============================================================

void PickupPlatformController::moveTo(double xMm, double yMm)
{
    if (m_homeState == HOMING_X || m_homeState == HOMING_Y) {
        qDebug() << "[PickupPlatform] moveTo() 忽略 — 回零中";
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[PickupPlatform] moveTo() 忽略 — 上一次移动未完成";
        return;
    }

    qDebug() << "[PickupPlatform] moveTo(" << xMm << "," << yMm << ")";

    const MotorAxis& axisX = m_motor->axisState(AXIS_X);
    const MotorAxis& axisY = m_motor->axisState(AXIS_Y);

    m_pendingMoves = 2;
    m_moveFailed = false;

    m_motor->moveRequest(AXIS_X, xMm, axisX.velocity, axisX.acceleration, axisX.deceleration);
    m_motor->moveRequest(AXIS_Y, yMm, axisY.velocity, axisY.acceleration, axisY.deceleration);
}

void PickupPlatformController::onMotorMoveFinished(int axisId, bool success)
{
    if (m_pendingMoves == 0)
        return;

    if (axisId != AXIS_X && axisId != AXIS_Y)
        return;

    if (!success) {
        m_moveFailed = true;
        qWarning() << "[PickupPlatform] 轴" << axisId << "移动失败";
    }

    m_pendingMoves--;

    if (m_pendingMoves == 0) {
        if (m_moveFailed) {
            reportError("点位移动", "X/Y轴移动异常");
            emit moveFinished(false);
        } else {
            emit moveFinished(true);
        }
    }
}

// ============================================================
// 停止
// ============================================================

void PickupPlatformController::stop()
{
    qDebug() << "[PickupPlatform] stop()";
    m_motor->stopMove(AXIS_X);
    m_motor->stopMove(AXIS_Y);
    m_homeState = IDLE;
    m_pendingMoves = 0;
    m_moveFailed = false;
}

// ============================================================
// 状态查询
// ============================================================

bool PickupPlatformController::isHomed() const
{
    return m_homeState == DONE;
}

bool PickupPlatformController::isMoving() const
{
    return m_pendingMoves > 0
        || m_homeState == HOMING_X
        || m_homeState == HOMING_Y;
}

double PickupPlatformController::currentX() const
{
    return m_currentX;
}

double PickupPlatformController::currentY() const
{
    return m_currentY;
}

// ============================================================
// 位置更新
// ============================================================

void PickupPlatformController::onMotorPositionUpdated(int axisId, double position)
{
    if (axisId == AXIS_X) {
        m_currentX = position;
        emit positionUpdated(m_currentX, m_currentY);
    } else if (axisId == AXIS_Y) {
        m_currentY = position;
        emit positionUpdated(m_currentX, m_currentY);
    }
}

// ============================================================
// 错误上报
// ============================================================

void PickupPlatformController::reportError(const QString &action, const QString &detail)
{
    m_alarm->raiseAlarm(ALARM_LEVEL_WARNING,
                        QStringLiteral("取晶平台"),
                        QStringLiteral("%1: %2").arg(action, detail));
}
```

- [ ] **Step 2: 更新 chipSetter.pro 添加新文件**

在 `SOURCES` 块中添加到 `DispensingPlatformController.cpp` 之后:
```
    core/PickupPlatformController.cpp \
```

在 `HEADERS` 块中添加到 `DispensingPlatformController.h` 之后:
```
    core/PickupPlatformController.h \
```

- [ ] **Step 2: 全量编译验证**

```bash
scripts/build_debug.bat
```

Expected: 编译成功 (Mock 模式), 所有 4 个新文件参与编译, 生成 debug/chipSetter.exe


### Task 6: 修改 mainwindow.h — 声明 PlatformController 成员

**Files:**
- Modify: `mainwindow.h:19-23` (在现有 include 块中添加)
- Modify: `mainwindow.h:56-61` (在 Core 成员区添加)

- [ ] **Step 1: 添加 include**

在 `#include "core/ProcessManager.h"` 之后添加:
```cpp
#include "core/DispensingPlatformController.h"
#include "core/PickupPlatformController.h"
```

- [ ] **Step 2: 添加成员变量**

在 `ProcessManager* m_processManager;` 之后添加:
```cpp
    DispensingPlatformController* m_dispensingPlatform;
    PickupPlatformController*     m_pickupPlatform;
```

- [ ] **Step 3: 验证编译**

```bash
scripts/build_debug.bat
```

Expected: 编译通过 (mainwindow.cpp 中创建实例后会报 unused 警告，可忽略)


### Task 7: 修改 mainwindow.cpp — 创建并注入 PlatformController

**Files:**
- Modify: `mainwindow.cpp:34-46` (createCoreModules)
- Modify: `mainwindow.cpp:448` (initSystem — 在 motorManager->initialize() 之后)

- [ ] **Step 1: 在 createCoreModules() 中创建实例**

在 `m_processManager = new ProcessManager(this);` 之后添加:
```cpp
    m_dispensingPlatform = new DispensingPlatformController(m_motorManager, m_alarmLogger, this);
    m_pickupPlatform     = new PickupPlatformController(m_motorManager, m_alarmLogger, this);
```

- [ ] **Step 2: 在 initSystem() 中初始化**

在 `m_motorManager->initialize();` 之后添加:
```cpp
    m_dispensingPlatform->initialize();
    m_pickupPlatform->initialize();
```

- [ ] **Step 3: 验证编译和运行**

```bash
scripts/build_debug.bat && ./debug/chipSetter.exe &
```

Expected: 编译通过, 启动无 crash, 日志中可见:
```
[DispensingPlatform] 初始化完成, AXIS_X= 2 AXIS_Y= 3
[PickupPlatform] 初始化完成, AXIS_X= 10 AXIS_Y= 11
```


### Task 8: 修改 ProcessManager.h — 注入 PlatformController

**Files:**
- Modify: `core/ProcessManager.h`

- [ ] **Step 1: 添加前向声明和 setter**

在 `#include <QDateTime>` 之后添加前向声明:
```cpp
class DispensingPlatformController;
class PickupPlatformController;
```

在 `public:` 区域、构造函数之后添加 setter:
```cpp
    void setDispensingPlatform(DispensingPlatformController* platform);
    void setPickupPlatform(PickupPlatformController* platform);
```

在 `private:` 区域、`QTimer* m_stepTimer;` 之后添加:
```cpp
    DispensingPlatformController* m_dispensingPlatform = nullptr;
    PickupPlatformController*     m_pickupPlatform     = nullptr;
```

- [ ] **Step 2: 在 ProcessManager.cpp 中实现 setter**

在 `ProcessManager.cpp` 的 `#include` 区域添加:
```cpp
#include "core/DispensingPlatformController.h"
#include "core/PickupPlatformController.h"
```

在构造函数之后添加:
```cpp
void ProcessManager::setDispensingPlatform(DispensingPlatformController* platform)
{
    m_dispensingPlatform = platform;
}

void ProcessManager::setPickupPlatform(PickupPlatformController* platform)
{
    m_pickupPlatform = platform;
}
```

- [ ] **Step 3: 在 MainWindow 中调用 setter**

在 `mainwindow.cpp` 的 `initSystem()` 中，`m_pickupPlatform->initialize();` 之后添加:
```cpp
    m_processManager->setDispensingPlatform(m_dispensingPlatform);
    m_processManager->setPickupPlatform(m_pickupPlatform);
```

- [ ] **Step 4: 验证编译**

```bash
scripts/build_debug.bat
```

Expected: 编译通过


### Task 9: 编写平台控制使用说明文档

**Files:**
- Create: `docs/platform-control-guide.md`

- [ ] **Step 1: 写入文档**

```markdown
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
```

- [ ] **Step 2: 验证文档可读**

用 IDE 预览 markdown 渲染效果。


### Task 10: 全量编译 + 冒烟测试

**Files:** (验证用, 不修改)

- [ ] **Step 1: Mock 模式全量编译**

```bash
scripts/build_debug.bat
```

Expected: 编译 0 error, 0 warning (除已知的 Qt 5.15.2 QButtonGroup 弃用警告)

- [ ] **Step 2: 启动验证**

```bash
./debug/chipSetter.exe &
```

检查:
1. 窗口正常显示 (1024×768)
2. 报警栏中出现 "系统启动完成" (绿色 INFO)
3. 日志文件 `chipsetter_debug.log` 中包含:
   - `[DispensingPlatform] 初始化完成`
   - `[PickupPlatform] 初始化完成`

- [ ] **Step 3: Real GNC 模式编译 (可选, 需工控机环境)**

```bash
scripts/build_deploy.bat
```

Expected: 编译通过, 链接 gts.lib 成功

- [ ] **Step 4: Commit**

```bash
git add core/DispensingPlatformController.h core/DispensingPlatformController.cpp \
        core/PickupPlatformController.h core/PickupPlatformController.cpp \
        chipSetter.pro mainwindow.h mainwindow.cpp core/ProcessManager.h core/ProcessManager.cpp \
        docs/platform-control-guide.md
git commit -m "feat: 新增 DispensingPlatformController + PickupPlatformController

- 点胶平台(轴2,3)和取晶平台(轴10,11)独立控制
- 异步回零序列: X→Y 内部状态机, 限位触发
- 双轴并发点位移动(非插补), 两轴完成才 emit moveFinished
- 错误通过 AlarmLogger 上报报错栏 (WARNING 级别)
- ProcessManager 注入接口, 供流程步骤调用
- 添加平台控制使用说明文档"
```
