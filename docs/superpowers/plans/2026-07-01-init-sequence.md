# 上电初始化流程 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将步骤①(初始化)从定时器模拟改为真实硬件事件驱动序列：亮黄灯→两组并发回零→点胶臂旋出→点胶上下回零

**Architecture:** ProcessManager 内部新增 InitPhase 状态机 + 位掩码并发跟踪，通过 MotorManager/homeFinished 信号驱动相位推进。前后端独立：ProcessManager 只管调度，UI 通过现有 signal/slot 自动更新。

**Tech Stack:** Qt 5.15.2 C++, 固高 GNC-C610 运动控制器, GTS SDK

---

### Task 1: ProcessManager.h — 新增类型和成员

**Files:**
- Modify: `core/ProcessManager.h`

- [ ] **Step 1: 添加前置声明和 include**

在 `#include <QDateTime>` 之后，`class DispensingPlatformController` 之前添加：

```cpp
#include <QMap>

class IoManager;
class MotorManager;
```

- [ ] **Step 2: 在 public 区域添加注入方法和信号**

在 `void setPickupPlatform(...)` 之后添加：

```cpp
    void setIoManager(IoManager* mgr);
    void setMotorManager(MotorManager* mgr);
```

在 `signals:` 块末尾 (`realtimeDataUpdated` 之后) 添加：

```cpp
    void initError(const QString& reason);
```

- [ ] **Step 3: 在 private 区域添加 InitPhase 枚举和新成员**

在 `private:` 区域开头 (在 `void initStepDefs()` 之前) 添加：

```cpp
    // --- 初始化流程状态机 ---
    enum InitPhase {
        INIT_IDLE = -1,
        PHASE0_LIGHT = 0,           // 亮黄灯
        PHASE1_GROUP1 = 1,          // 第一组回零 (轴12+7+平台2,3)
        PHASE2_GROUP2 = 2,          // 第二组回零 (平台13,14+轴15+8+10)
        PHASE3_DISPENSE_MOVE = 3,   // 点胶臂旋出5450pulse
        PHASE4_HOME_AXIS9 = 4,      // 点胶上下回零
        INIT_COMPLETE = 5
    };

    void startInitPhase(InitPhase phase);
    void advanceInitPhase();
    void abortInit(const QString& reason);
    void cleanupInitState();
    void stopAllInitAxes();
    bool retryFailedAxis(int axisId);

    IoManager*    m_ioManager = nullptr;
    MotorManager* m_motorManager = nullptr;

    int           m_initPhase = INIT_IDLE;
    int           m_initPendingMask = 0;
    QMap<int,int> m_initRetryCount;     // axisId → retry count
    QTimer*       m_initPhaseTimer = nullptr;
```

- [ ] **Step 4: 添加新私有槽**

在 `private slots:` 区域 (在 `onStepTimerTick` 之后) 添加：

```cpp
    void onInitMotorHomeFinished(int axisId, bool success, int homeStage);
    void onInitMotorMoveFinished(int axisId, bool success);
    void onInitDispensingHomeFinished(bool success);
    void onInitPickupHomeFinished(bool success);
    void onInitPhaseTimeout();
```

- [ ] **Step 5: 提交**

```bash
git add core/ProcessManager.h
git commit -m "feat: ProcessManager 声明初始化状态机成员和接口"
```

---

### Task 2: ProcessManager.cpp — StepDef 和构造函数

**Files:**
- Modify: `core/ProcessManager.cpp`

- [ ] **Step 1: 添加 include**

在第 8 行 `PickupPlatformController.h` 之后添加：

```cpp
#include "core/IoManager.h"
#include "core/MotorManager.h"
#include "core/HardwareConfig.h"
```

- [ ] **Step 2: 更新 initStepDefs() 中步骤0的子步骤**

将第 81-84 行的 `m_steps[0] = ...` 替换为：

```cpp
    m_steps[0] = {0, "初始化", {
        "亮黄灯",
        "胶盘回零",
        "下顶针回零",
        "取晶臂转动回零",
        "点胶平台回零",
        "取晶平台XY回零",
        "取晶平台W回零",
        "点胶臂转动回零",
        "取晶臂上下回零",
        "点胶臂旋出5450pulse",
        "点胶上下回零"
    }, {}};
```

- [ ] **Step 3: 构造函数中创建 m_initPhaseTimer**

在构造函数第 54 行 `connect(m_stepTimer...)` 之后添加：

```cpp
    // 初始化超时定时器 (单次触发, 每相位不同超时值)
    m_initPhaseTimer = new QTimer(this);
    m_initPhaseTimer->setSingleShot(true);
    connect(m_initPhaseTimer, &QTimer::timeout,
            this, &ProcessManager::onInitPhaseTimeout);
```

- [ ] **Step 4: 更新析构函数**

在第 57-60 行, 增加 m_initPhaseTimer 的停止：

```cpp
ProcessManager::~ProcessManager()
{
    if (m_stepTimer->isActive())
        m_stepTimer->stop();
    if (m_initPhaseTimer->isActive())
        m_initPhaseTimer->stop();
}
```

- [ ] **Step 5: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: 更新步骤0子步定义和初始化定时器"
```

---

### Task 3: ProcessManager.cpp — setter 方法实现

**Files:**
- Modify: `core/ProcessManager.cpp`

- [ ] **Step 1: 修改 setDispensingPlatform 增加信号连接**

将第 63-66 行替换为：

```cpp
void ProcessManager::setDispensingPlatform(DispensingPlatformController* platform)
{
    m_dispensingPlatform = platform;
    if (m_dispensingPlatform) {
        connect(m_dispensingPlatform, &DispensingPlatformController::homeFinished,
                this, &ProcessManager::onInitDispensingHomeFinished);
    }
}
```

- [ ] **Step 2: 修改 setPickupPlatform 增加信号连接**

将第 68-71 行替换为：

```cpp
void ProcessManager::setPickupPlatform(PickupPlatformController* platform)
{
    m_pickupPlatform = platform;
    if (m_pickupPlatform) {
        connect(m_pickupPlatform, &PickupPlatformController::homeFinished,
                this, &ProcessManager::onInitPickupHomeFinished);
    }
}
```

- [ ] **Step 3: 新增 setIoManager 实现**

在 setPickupPlatform 之后添加：

```cpp
void ProcessManager::setIoManager(IoManager* mgr)
{
    m_ioManager = mgr;
}
```

- [ ] **Step 4: 新增 setMotorManager 实现**

之后添加：

```cpp
void ProcessManager::setMotorManager(MotorManager* mgr)
{
    m_motorManager = mgr;
    if (m_motorManager) {
        connect(m_motorManager, &MotorManager::homeFinished,
                this, &ProcessManager::onInitMotorHomeFinished);
        connect(m_motorManager, &MotorManager::moveFinished,
                this, &ProcessManager::onInitMotorMoveFinished);
    }
}
```

- [ ] **Step 5: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: setter注入IoManager/MotorManager并连接信号"
```

---

### Task 4: ProcessManager.cpp — 修改 executeStep 触发 INIT 事件驱动

**Files:**
- Modify: `core/ProcessManager.cpp`

- [ ] **Step 1: 替换 executeStep 函数**

将第 318-345 行的 `executeStep` 完整替换为：

```cpp
void ProcessManager::executeStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT) return;

    m_currentStepIdx = stepIndex;
    m_stepStates[stepIndex] = RUNNING;

    int n = m_substepStates[stepIndex].size();
    for (int i = 0; i < n; ++i)
        m_substepStates[stepIndex][i] = PENDING;

    m_substepProgress = 0;

    emit currentStepChanged(stepIndex);
    emit stepStateChanged(stepIndex, static_cast<int>(RUNNING));

    if (n > 0) {
        m_substepStates[stepIndex][0] = RUNNING;
        emit substepStateChanged(stepIndex, 0, static_cast<int>(RUNNING));
    }

    // STEP_INIT: 事件驱动, 不启动模拟定时器
    if (stepIndex == STEP_INIT) {
        m_stepTimer->stop();
        cleanupInitState();
        m_initRetryCount.clear();
        startInitPhase(PHASE0_LIGHT);
        return;
    }

    // 其余步骤保持定时器模拟
    m_stepTimer->start(500);

    qDebug() << "[ProcessManager] 执行步骤" << stepIndex
             << m_steps[stepIndex].name;
}
```

- [ ] **Step 2: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: STEP_INIT改为事件驱动, 调用startInitPhase"
```

---

### Task 5: ProcessManager.cpp — 初始化状态机核心实现

**Files:**
- Modify: `core/ProcessManager.cpp`

- [ ] **Step 1: 在文件末尾添加 startInitPhase 实现**

在 `advanceToNextStep()` 之后 (第 465 行之后) 添加：

```cpp
// ============================================================
// 初始化流程 — 事件驱动状态机
// ============================================================

void ProcessManager::startInitPhase(InitPhase phase)
{
    m_initPhase = phase;
    m_initPendingMask = 0;
    m_initPhaseTimer->stop();

    // 更新当前相位对应的子步骤为 RUNNING
    int subIdx = static_cast<int>(phase);
    if (subIdx >= 0 && subIdx < m_substepStates[STEP_INIT].size()) {
        // 前一个子步骤标记完成
        if (subIdx > 0) {
            m_substepStates[STEP_INIT][subIdx - 1] = COMPLETED;
            emit substepStateChanged(STEP_INIT, subIdx - 1,
                                     static_cast<int>(COMPLETED));
        }
        m_substepStates[STEP_INIT][subIdx] = RUNNING;
        emit substepStateChanged(STEP_INIT, subIdx,
                                 static_cast<int>(RUNNING));
        m_substepProgress = subIdx;
    }

    switch (phase) {

    case PHASE0_LIGHT:
        // 亮黄灯 — 同步完成
        if (m_ioManager)
            m_ioManager->setDO(DO_YELLOW_LIGHT, 1);
        qDebug() << "[ProcessManager] INIT Phase0: 黄灯亮";
        advanceInitPhase();
        break;

    case PHASE1_GROUP1: {
        // 轴1 mode=35 同步零位 (不加入mask)
        if (m_motorManager)
            m_motorManager->homeRequest(AXIS_JIAO_PAN);

        // 并发回零: 下顶针(12) + 取晶臂转动(7) + 点胶平台(2,3)
        // bit(0)虚拟位 = 点胶平台
        m_initPendingMask = (1 << 12) | (1 << 7) | (1 << 0);
        m_initPhaseTimer->start(HOME_TIMEOUT_MS);
        qDebug() << "[ProcessManager] INIT Phase1: 第一组并发回零 mask=0x"
                 << Qt::hex << m_initPendingMask;

        if (m_motorManager) {
            m_motorManager->homeRequest(AXIS_EJECTOR_DOWN);
            m_motorManager->homeRequest(AXIS_PICKUP_ARM_ROT);
        }
        if (m_dispensingPlatform)
            m_dispensingPlatform->home();
        break;
    }

    case PHASE2_GROUP2:
        // 并发回零: 取晶平台(13,14) + 取晶W(15) + 点胶臂转动(8) + 取晶上下(10)
        // bit(1)虚拟位 = 取晶平台
        m_initPendingMask = (1 << 15) | (1 << 8) | (1 << 10) | (1 << 1);
        m_initPhaseTimer->start(HOME_TIMEOUT_MS);
        qDebug() << "[ProcessManager] INIT Phase2: 第二组并发回零 mask=0x"
                 << Qt::hex << m_initPendingMask;

        if (m_motorManager) {
            m_motorManager->homeRequest(AXIS_PICKUP_W);
            m_motorManager->homeRequest(AXIS_DISPENSE_ARM_ROT);
            m_motorManager->homeRequest(AXIS_PICKUP_ARM_UD);
        }
        if (m_pickupPlatform)
            m_pickupPlatform->home();
        break;

    case PHASE3_DISPENSE_MOVE:
        m_initPendingMask = (1 << 8);
        m_initPhaseTimer->start(MOVE_TIMEOUT_MS);
        qDebug() << "[ProcessManager] INIT Phase3: 点胶臂旋出5450pulse";

        if (m_motorManager) {
            const MotorAxis& ax8 = m_motorManager->axisState(AXIS_DISPENSE_ARM_ROT);
            m_motorManager->moveRequest(AXIS_DISPENSE_ARM_ROT, 5450.0,
                                        ax8.velocity, ax8.acceleration, ax8.deceleration);
        }
        break;

    case PHASE4_HOME_AXIS9:
        m_initPendingMask = (1 << 9);
        m_initPhaseTimer->start(HOME_TIMEOUT_MS);
        qDebug() << "[ProcessManager] INIT Phase4: 点胶上下回零";

        if (m_motorManager)
            m_motorManager->homeRequest(AXIS_DISPENSE_ARM_UD);
        break;

    default:
        break;
    }
}
```

- [ ] **Step 2: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: startInitPhase 5阶段硬件调度实现"
```

---

### Task 6: ProcessManager.cpp — 信号处理和相位推进

**Files:**
- Modify: `core/ProcessManager.cpp`

- [ ] **Step 1: 在 startInitPhase 之后添加 advanceInitPhase**

```cpp
void ProcessManager::advanceInitPhase()
{
    // 标记当前相位子步骤完成
    int curSub = static_cast<int>(m_initPhase);
    if (curSub >= 0 && curSub < m_substepStates[STEP_INIT].size()) {
        m_substepStates[STEP_INIT][curSub] = COMPLETED;
        emit substepStateChanged(STEP_INIT, curSub,
                                 static_cast<int>(COMPLETED));
        m_substepProgress = curSub;
    }

    InitPhase next = static_cast<InitPhase>(m_initPhase + 1);
    if (next > PHASE4_HOME_AXIS9) {
        // 所有相位完成
        m_initPhase = INIT_COMPLETE;
        qDebug() << "[ProcessManager] INIT 全部完成 → 进入上料步骤";
        cleanupInitState();
        completeCurrentStep();
    } else {
        startInitPhase(next);
    }
}
```

- [ ] **Step 2: 添加 abortInit**

```cpp
void ProcessManager::abortInit(const QString& reason)
{
    qWarning() << "[ProcessManager] INIT 失败:" << reason;

    m_initPhaseTimer->stop();
    m_initPendingMask = 0;
    m_initRetryCount.clear();

    // 停止所有轴
    stopAllInitAxes();
    if (m_dispensingPlatform) m_dispensingPlatform->stop();
    if (m_pickupPlatform)     m_pickupPlatform->stop();

    // 红灯亮, 黄灯灭
    if (m_ioManager) {
        m_ioManager->setDO(DO_YELLOW_LIGHT, 0);
        m_ioManager->setDO(DO_RED_LIGHT, 1);
    }

    // 标记当前子步骤 FAILED
    int sub = static_cast<int>(m_initPhase);
    if (sub >= 0 && sub < m_substepStates[STEP_INIT].size()) {
        m_substepStates[STEP_INIT][sub] = FAILED;
        emit substepStateChanged(STEP_INIT, sub, static_cast<int>(FAILED));
    }

    // 标记步骤0 FAILED
    m_stepStates[STEP_INIT] = FAILED;
    emit stepStateChanged(STEP_INIT, static_cast<int>(FAILED));

    m_running = false;
    m_currentStepIdx = -1;
    emit currentStepChanged(-1);

    emit initError(reason);
}
```

- [ ] **Step 3: 添加单轴回零完成处理**

```cpp
void ProcessManager::onInitMotorHomeFinished(int axisId, bool success, int homeStage)
{
    Q_UNUSED(homeStage);
    if (m_currentStepIdx != STEP_INIT) return;
    if (m_initPendingMask == 0) return;

    int bit = (1 << axisId);
    if (!(m_initPendingMask & bit)) return;

    m_initPendingMask &= ~bit;

    if (!success) {
        if (retryFailedAxis(axisId))
            return;  // 重试中, mask位已恢复
        abortInit(QString("轴%1回零失败(已重试)").arg(axisId));
        return;
    }

    qDebug() << "[ProcessManager] INIT 轴" << axisId << "回零完成"
             << "剩余mask=0x" << Qt::hex << m_initPendingMask;

    if (m_initPendingMask == 0)
        advanceInitPhase();
}
```

- [ ] **Step 4: 添加平台控制器和移动完成处理**

```cpp
void ProcessManager::onInitDispensingHomeFinished(bool success)
{
    if (m_currentStepIdx != STEP_INIT) return;
    if (m_initPhase != PHASE1_GROUP1) return;
    if (!(m_initPendingMask & (1 << 0))) return;

    m_initPendingMask &= ~(1 << 0);

    if (!success) {
        // 平台控制器失败=两个轴都失败, 不重试
        abortInit("点胶平台回零失败");
        return;
    }

    qDebug() << "[ProcessManager] INIT 点胶平台回零完成";
    if (m_initPendingMask == 0)
        advanceInitPhase();
}

void ProcessManager::onInitPickupHomeFinished(bool success)
{
    if (m_currentStepIdx != STEP_INIT) return;
    if (m_initPhase != PHASE2_GROUP2) return;
    if (!(m_initPendingMask & (1 << 1))) return;

    m_initPendingMask &= ~(1 << 1);

    if (!success) {
        abortInit("取晶平台回零失败");
        return;
    }

    qDebug() << "[ProcessManager] INIT 取晶平台回零完成";
    if (m_initPendingMask == 0)
        advanceInitPhase();
}

void ProcessManager::onInitMotorMoveFinished(int axisId, bool success)
{
    if (m_currentStepIdx != STEP_INIT) return;
    if (m_initPhase != PHASE3_DISPENSE_MOVE) return;

    int bit = (1 << axisId);
    if (!(m_initPendingMask & bit)) return;

    m_initPendingMask &= ~bit;

    if (!success) {
        abortInit(QString("点胶臂旋出失败, 轴%1").arg(axisId));
        return;
    }

    qDebug() << "[ProcessManager] INIT 点胶臂旋出完成";
    if (m_initPendingMask == 0)
        advanceInitPhase();
}

void ProcessManager::onInitPhaseTimeout()
{
    if (m_initPhase < PHASE0_LIGHT || m_initPhase > PHASE4_HOME_AXIS9) return;
    if (m_initPendingMask == 0) return;

    qWarning() << "[ProcessManager] INIT Phase" << m_initPhase
               << "超时! 剩余mask=0x" << Qt::hex << m_initPendingMask;
    abortInit(QString("初始化Phase%1超时").arg(m_initPhase));
}
```

- [ ] **Step 5: 添加辅助方法**

```cpp
void ProcessManager::cleanupInitState()
{
    m_initPhase = INIT_IDLE;
    m_initPendingMask = 0;
    m_initPhaseTimer->stop();
}

void ProcessManager::stopAllInitAxes()
{
    if (!m_motorManager) return;
    int axes[] = {1, 2, 3, 7, 8, 9, 10, 12, 13, 14, 15};
    for (int id : axes)
        m_motorManager->stopMove(id);
}

bool ProcessManager::retryFailedAxis(int axisId)
{
    int count = m_initRetryCount.value(axisId, 0);
    if (count >= 1) return false;  // 已经重试过

    m_initRetryCount[axisId] = count + 1;
    // 恢复该轴的mask位
    m_initPendingMask |= (1 << axisId);
    qWarning() << "[ProcessManager] INIT 轴" << axisId
               << "回零失败, 重试第" << (count + 1) << "次";
    m_motorManager->homeRequest(axisId);
    return true;
}
```

- [ ] **Step 6: 修改 emergencyStop 增加 INIT 清理**

在 emergencyStop() 第 294 行 `m_stepTimer->stop()` 之后添加：

```cpp
    // 清理初始化流程
    stopAllInitAxes();
    if (m_dispensingPlatform) m_dispensingPlatform->stop();
    if (m_pickupPlatform)     m_pickupPlatform->stop();
    cleanupInitState();
    m_initRetryCount.clear();
```

- [ ] **Step 7: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: INIT状态机信号处理+重试+超时+急停清理"
```

---

### Task 7: mainwindow.cpp — 注入和连线

**Files:**
- Modify: `mainwindow.cpp`

- [ ] **Step 1: 在 initSystem() 中注入 IoManager 和 MotorManager**

在 `mainwindow.cpp` 的 `initSystem()` 函数中，第 521 行 `m_processManager->setPickupPlatform(...)` 之后添加：

```cpp
    m_processManager->setIoManager(m_ioManager);
    m_processManager->setMotorManager(m_motorManager);
```

- [ ] **Step 2: 在 connectSignals() 中添加 initError 连接**

在 connectSignals() 函数中，allFinished 连接之后添加：

```cpp
    // INIT 初始化错误 → 报警
    connect(m_processManager, &ProcessManager::initError,
            this, [this](const QString& reason) {
        m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "初始化", reason);
    });
```

- [ ] **Step 3: 添加步骤状态变化时的灯色切换**

在 connectSignals() 中，stepStateChanged 连接块之后添加：

```cpp
    // 步骤0(初始化)完成 → 黄灯灭, 绿灯亮
    connect(m_processManager, &ProcessManager::stepStateChanged,
            this, [this](int stepIndex, int state) {
        if (stepIndex != 0) return;
        if (state == static_cast<int>(ProcessManager::COMPLETED)) {
            m_ioManager->setDO(DO_YELLOW_LIGHT, 0);
            m_ioManager->setDO(DO_GREEN_LIGHT, 1);
        } else if (state == static_cast<int>(ProcessManager::FAILED)) {
            m_ioManager->setDO(DO_YELLOW_LIGHT, 0);
            m_ioManager->setDO(DO_RED_LIGHT, 1);
        }
    });
```

- [ ] **Step 4: 提交**

```bash
git add mainwindow.cpp
git commit -m "feat: MainWindow注入IoManager/MotorManager到ProcessManager"
```

---

### Task 8: 编译和验证

**Files:** (无修改, 仅验证)

- [ ] **Step 1: 编译**

```bash
powershell -File scripts/workflow.ps1 build
```

- [ ] **Step 2: 检查编译警告**

确认无新增 warning，特别是信号/槽签名不匹配警告。

- [ ] **Step 3: 运行单元测试**

```bash
cd tests && make -f Makefile.Debug -j4
./debug/test_process_manager.exe -txt
```

确认非 INIT 相关测试仍然通过（testStartCycle/testStepProgression 需要 INIT 实际硬件，可能需要跳过）。

- [ ] **Step 4: Code Review**

```bash
# 运行 code-review skill 检查修改
```

- [ ] **Step 5: 最终提交**

```bash
git add -A
git commit -m "feat: 上电初始化流程 — 事件驱动5阶段序列 + 组内并发 + 重试"
```

---

### 验证

1. **编译**: `powershell -File scripts/workflow.ps1 build` — 零 error 零 warning
2. **测试**: 非 INIT 测试保持通过；INIT 测试在 Mock 环境下跳过硬件调用
3. **实机**: 启动 → 黄灯亮 → 第一组并发回零(轴12/7/平台2,3) → 第二组(平台13,14/轴15/8/10) → 轴8旋出5450pulse → 轴9回零 → 绿灯 → 进入上料
