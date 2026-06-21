# 工艺流程操作 GUI — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建基于工艺流程的生产操作界面，含九步流程条(带循环框)、步骤详情三栏面板、设备状态概览，与现有调试界面通过开关切换。

**Architecture:** 新增 `ProcessManager` (Core层状态机) 管理步骤切换和循环逻辑；新增 `FlowStepBar`、`StepDetailPanel`、`DeviceStatusWidget` 三个独立Widget；重写 `ProductionWidget` 组合上述组件。所有通信通过 Qt Signal/Slot。

**Tech Stack:** Qt 5.15.2 + C++11 + MSVC, 固高GNC SDK (Mock), 1200×800 深色主题

---

## 文件清单

| 文件 | 操作 | 职责 |
|------|------|------|
| `core/ProcessManager.h` | 新建 | 流程状态机接口：9步定义、状态枚举、信号/槽 |
| `core/ProcessManager.cpp` | 新建 | 流程循环逻辑、步骤切换、自动循环 |
| `widgets/FlowStepBar.h` | 新建 | 流程步骤条：9个步骤块+循环框+回路箭头 |
| `widgets/FlowStepBar.cpp` | 新建 | 绘制逻辑、点击交互、状态颜色 |
| `widgets/StepDetailPanel.h` | 新建 | 步骤详情面板：子步骤+参数+实时状态三栏 |
| `widgets/StepDetailPanel.cpp` | 新建 | 三栏布局、数据更新 |
| `widgets/DeviceStatusWidget.h` | 新建 | 设备状态概览：轴灯+IO+通讯 |
| `widgets/DeviceStatusWidget.cpp` | 新建 | 轴灯颜色更新、IO摘要 |
| `widgets/ProductionWidget.h` | 重写 | 组合FlowStepBar+StepDetailPanel+DeviceStatusWidget+AlarmList |
| `widgets/ProductionWidget.cpp` | 重写 | 布局组装、信号转发 |
| `mainwindow.h` | 修改 | 新增 ProcessManager 成员、信号连接声明 |
| `mainwindow.cpp` | 修改 | 创建 ProcessManager、连接新信号槽 |
| `chipSetter.pro` | 修改 | 添加新源文件 |
| `tests/test_process_manager.cpp` | 新建 | ProcessManager 单元测试 |
| `tests/test_process_manager.pro` | 新建 | 测试工程文件 |
| `tests/tests.pro` | 修改 | 添加 test_process_manager 子项目 |

---

## 接口总览

### ProcessManager 接口

```cpp
class ProcessManager : public QObject
{
    Q_OBJECT
public:
    // 步骤状态枚举
    enum StepState { PENDING = 0, RUNNING = 1, COMPLETED = 2, FAILED = 3 };
    Q_ENUM(StepState)

    // 步骤定义 (9步)
    struct StepDef {
        int index;                    // 0~8
        QString name;                 // 步骤名
        QStringList substeps;         // 子步骤列表
        QVariantMap defaultParams;    // 默认参数 name→value
    };

    explicit ProcessManager(QObject *parent = nullptr);

    // 步骤状态查询
    StepState stepState(int stepIndex) const;
    int currentStep() const;          // 当前活跃步骤 (-1=无)
    const StepDef& stepDef(int stepIndex) const;
    QVector<StepDef> allSteps() const;
    bool isRunning() const;
    int cycleCount() const;           // 已完成循环数

    // 步骤详情数据 (供 StepDetailPanel 查询)
    QVector<int> substepStates(int stepIndex) const;    // 每个子步骤的状态
    QVariantMap stepParams(int stepIndex) const;        // 步骤参数
    QVariantMap realtimeData(int stepIndex) const;      // 实时状态数据

public slots:
    void startCycle();                // 开始生产循环 (从步骤0初始化开始)
    void pauseCycle();                // 暂停循环 (完成当前步骤后停止)
    void resumeCycle();               // 继续循环
    void finishCycle();               // 执行收尾(步骤8)后停止
    void emergencyStop();             // 急停 → 所有步骤标记待执行

signals:
    void stepStateChanged(int stepIndex, ProcessManager::StepState state);
    void currentStepChanged(int stepIndex);
    void substepStateChanged(int stepIndex, int substepIndex, ProcessManager::StepState state);
    void cycleCompleted(int totalCount);         // 一个生产循环完成(产量)
    void allFinished();                          // 收尾完成
    void realtimeDataUpdated(int stepIndex, const QVariantMap& data);

private slots:
    void onStepTimeout();             // 步骤模拟定时器

private:
    void executeStep(int stepIndex);
    void completeCurrentStep();

    QVector<StepDef>    m_steps;      // 9步定义
    QVector<StepState>  m_stepStates; // 9步状态
    QVector<QVector<StepState>> m_substepStates; // 9×n 子步骤状态
    int         m_currentStepIdx;     // 当前步骤 (-1=无)
    bool        m_running;
    bool        m_paused;
    int         m_cycleCount;
    QTimer*     m_stepTimer;          // 模拟步骤执行定时器
};
```

### FlowStepBar 接口

```cpp
class FlowStepBar : public QWidget
{
    Q_OBJECT
public:
    explicit FlowStepBar(QWidget *parent = nullptr);

    void setSteps(const QStringList& names);  // 设置步骤名称列表 (9项)
    int  currentStep() const;                 // 当前选中步骤

public slots:
    void setStepState(int stepIndex, int state);   // 0=PENDING,1=RUNNING,2=COMPLETED,3=FAILED
    void setCurrentStep(int stepIndex);            // 高亮当前步骤

signals:
    void stepClicked(int stepIndex);               // 用户点击步骤块

private:
    void setupUI();
    void updateVisualState(int stepIndex);
    QVector<QPushButton*> m_stepButtons;    // 9个步骤按钮
    QFrame*               m_loopFrame;      // 循环框 (包裹步骤1~7)
    int                   m_currentStep;
};
```

### StepDetailPanel 接口

```cpp
class StepDetailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit StepDetailPanel(QWidget *parent = nullptr);

public slots:
    void showStepDetail(int stepIndex,
                        const QStringList& substeps,
                        const QVector<int>& substepStates,
                        const QVariantMap& params,
                        const QVariantMap& realtimeData);

private:
    void setupUI();
    QLabel*             m_titleLabel;       // "步骤详情 — ⑤ 拾取"
    QVBoxLayout*        m_substepLayout;    // 子步骤列表
    QGridLayout*        m_paramLayout;      // 参数网格
    QGridLayout*        m_realtimeLayout;   // 实时状态网格
};
```

### DeviceStatusWidget 接口

```cpp
class DeviceStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceStatusWidget(QWidget *parent = nullptr);

public slots:
    void updateAxisStates(const QVector<int>& enabled,
                          const QVector<int>& moving,
                          const QVector<int>& alarmed);
    void setEmStopStatus(bool triggered);
    void setConnectionStatus(bool connected);
    void updateIoSignals(const QStringList& activeSignals);

private:
    void setupUI();
    QVector<QLabel*> m_axisLights;          // 13个轴指示灯
    QLabel*          m_emStopLabel;         // 急停状态
    QLabel*          m_connLabel;           // 通讯状态
    QLabel*          m_ioSummaryLabel;      // IO摘要
};
```

---

### Task 1: 新建 ProcessManager 头文件

**Files:**
- Create: `core/ProcessManager.h`

- [ ] **Step 1: 创建 core/ProcessManager.h**

```cpp
/**
 * @file ProcessManager.h
 * @brief 工艺流程管理器 — 9步状态机、自动循环、步骤切换
 *
 * 职责:
 *   - 管理9个工艺步骤（初始化→生产循环②~⑧→收尾）
 *   - 控制步骤切换和循环逻辑
 *   - 维护每个步骤的子步骤状态、参数、实时数据
 *   - 通过信号通知UI层状态变化
 *
 * 依赖: QObject, QTimer
 */

#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QVariantMap>

class ProcessManager : public QObject
{
    Q_OBJECT

public:
    // 步骤状态枚举
    enum StepState {
        PENDING   = 0,    // 待执行 (灰色)
        RUNNING   = 1,    // 运行中 (蓝色发光)
        COMPLETED = 2,    // 已完成 (绿色)
        FAILED    = 3     // 失败 (红色)
    };
    Q_ENUM(StepState)

    // 步骤定义结构
    struct StepDef {
        int         index;            // 步骤索引 0~8
        QString     name;             // 步骤名称
        QStringList substeps;         // 子步骤名称列表
        QVariantMap defaultParams;    // 默认参数
    };

    explicit ProcessManager(QObject *parent = nullptr);
    ~ProcessManager();

    // ---- 状态查询 ----
    StepState stepState(int stepIndex) const;
    int currentStep() const;
    const StepDef& stepDef(int stepIndex) const;
    QVector<StepDef> allSteps() const;
    bool isRunning() const;
    bool isPaused() const;
    int  cycleCount() const;

    // ---- 步骤详情 (供 StepDetailPanel) ----
    QVector<int>  substepStates(int stepIndex) const;
    QVariantMap   stepParams(int stepIndex) const;
    QVariantMap   realtimeData(int stepIndex) const;

public slots:
    void startCycle();                // 开始: 从初始化(0)开始
    void pauseCycle();                // 暂停: 完成当前步骤后停
    void resumeCycle();               // 继续: 继续自动循环
    void finishCycle();               // 收尾: 跳至步骤8, 完成后停止
    void emergencyStop();             // 急停: 全部重置为待执行

signals:
    void stepStateChanged(int stepIndex, int state);
    void currentStepChanged(int stepIndex);
    void substepStateChanged(int stepIndex, int substepIndex, int state);
    void cycleCompleted(int totalCount);
    void allFinished();
    void realtimeDataUpdated(int stepIndex, const QVariantMap& data);

private slots:
    void onStepTimerTick();

private:
    void initStepDefs();              // 初始化9步定义
    void executeStep(int stepIndex);
    void completeCurrentStep();
    void advanceToNextStep();

    // 9步定义
    QVector<StepDef> m_steps;
    // 9步状态
    QVector<StepState> m_stepStates;
    // 9×n 子步骤状态
    QVector<QVector<StepState>> m_substepStates;

    int     m_currentStepIdx;         // -1=无, 0~8
    bool    m_running;
    bool    m_paused;
    int     m_cycleCount;
    int     m_substepProgress;        // 当前子步骤进度 (模拟用)
    QTimer* m_stepTimer;
};

#endif // PROCESSMANAGER_H
```

- [ ] **Step 2: 提交**

```bash
git add core/ProcessManager.h
git commit -m "feat: add ProcessManager header — process flow state machine interface

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 2: 实现 ProcessManager

**Files:**
- Create: `core/ProcessManager.cpp`

- [ ] **Step 1: 创建 core/ProcessManager.cpp**

```cpp
/**
 * @file ProcessManager.cpp
 * @brief ProcessManager 实现 — 9步状态机 + 自动循环
 */

#include "ProcessManager.h"
#include <QDebug>

// ============================================================
// 9步定义 — 集中管理点
// ============================================================

static const int STEP_INIT    = 0;   // 初始化
static const int STEP_LOAD   = 1;   // 上料 (循环起点)
static const int STEP_ALIGN  = 2;   // 定位
static const int STEP_EJECT  = 3;   // 顶晶
static const int STEP_PICK   = 4;   // 拾取
static const int STEP_DISPENSE = 5; // 点胶
static const int STEP_PLACE  = 6;   // 贴装
static const int STEP_UNLOAD = 7;   // 下料 (循环终点)
static const int STEP_FINISH = 8;   // 收尾

static const int LOOP_START  = 1;   // 循环从步骤1开始
static const int LOOP_END    = 7;   // 循环到步骤7
static const int STEP_COUNT  = 9;

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent)
    , m_currentStepIdx(-1)
    , m_running(false)
    , m_paused(false)
    , m_cycleCount(0)
    , m_substepProgress(0)
{
    initStepDefs();

    // 初始化9步状态
    m_stepStates.resize(STEP_COUNT);
    m_stepStates.fill(PENDING);

    // 初始化子步骤状态
    m_substepStates.resize(STEP_COUNT);
    for (int i = 0; i < STEP_COUNT; ++i) {
        int n = m_steps[i].substeps.size();
        m_substepStates[i].resize(n > 0 ? n : 1);
        m_substepStates[i].fill(PENDING);
    }

    // 模拟定时器: 每500ms完成一个子步骤
    m_stepTimer = new QTimer(this);
    connect(m_stepTimer, &QTimer::timeout, this, &ProcessManager::onStepTimerTick);
}

ProcessManager::~ProcessManager()
{
    if (m_stepTimer->isActive())
        m_stepTimer->stop();
}

// ============================================================
// 初始化9步定义
// ============================================================

void ProcessManager::initStepDefs()
{
    m_steps.resize(STEP_COUNT);

    m_steps[0] = {0, "初始化", {
        "13轴使能与归零", "IO通道检测", "加载运动参数",
        "气压检测", "通讯链路确认"
    }, {}};

    m_steps[1] = {1, "上料", {
        "晶盘X/Y移动到取料位", "吸嘴下降到安全高度",
        "确认晶圆在位"
    }, {
        {"晶盘X取料位", 120.0}, {"晶盘Y取料位", 80.0},
        {"安全高度", 15.0}, {"上料速度", 80.0}
    }};

    m_steps[2] = {2, "定位", {
        "视觉拍照", "图像识别晶粒位置",
        "计算X/Y偏移量", "修正晶盘位置"
    }, {
        {"识别阈值", 0.85}, {"最大偏移", 2.0},
        {"定位超时", 3000}
    }};

    m_steps[3] = {3, "顶晶", {
        "Z1顶针上升到接触高度", "Z2顶针二次顶出",
        "确认晶粒分离"
    }, {
        {"接触高度", -1.200}, {"顶出距离", 0.500},
        {"推力上限", 1.0}, {"顶出速度", 5.0}
    }};

    m_steps[4] = {4, "拾取", {
        "Z轴下降到拾取高度", "吸嘴开启真空",
        "顶针退回", "Z轴上升到安全高度",
        "真空检测确认"
    }, {
        {"拾取高度", -2.500}, {"真空延时", 150},
        {"安全高度", 15.0}, {"真空阈值", -70}
    }};

    m_steps[5] = {5, "点胶", {
        "点胶X/Y移动到目标", "点胶阀开启",
        "点胶时间控制", "点胶阀关闭"
    }, {
        {"点胶X位置", 200.0}, {"点胶Y位置", 100.0},
        {"点胶时间", 80}, {"点胶压力", 0.5}
    }};

    m_steps[6] = {6, "贴装", {
        "移动到贴装X/Y位置", "Z轴下降到贴装高度",
        "关闭真空释放晶粒", "Z轴回升"
    }, {
        {"贴装X位置", 220.0}, {"贴装Y位置", 110.0},
        {"贴装高度", -1.800}, {"贴装延时", 200}
    }};

    m_steps[7] = {7, "下料", {
        "吸嘴回到安全位置", "基板移出",
        "产量计数+1", "检查是否继续循环"
    }, {
        {"安全高度", 15.0}, {"下料延时", 300}
    }};

    m_steps[8] = {8, "收尾", {
        "所有轴回到安全位置", "关闭所有轴使能",
        "保存统计数据", "关闭气源"
    }, {}};
}

// ============================================================
// 状态查询
// ============================================================

ProcessManager::StepState ProcessManager::stepState(int stepIndex) const
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT) return PENDING;
    return m_stepStates[stepIndex];
}

int ProcessManager::currentStep() const
{
    return m_currentStepIdx;
}

const ProcessManager::StepDef& ProcessManager::stepDef(int stepIndex) const
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT) {
        static StepDef empty;
        return empty;
    }
    return m_steps[stepIndex];
}

QVector<ProcessManager::StepDef> ProcessManager::allSteps() const
{
    return m_steps;
}

bool ProcessManager::isRunning() const { return m_running; }
bool ProcessManager::isPaused() const  { return m_paused; }
int  ProcessManager::cycleCount() const { return m_cycleCount; }

QVector<int> ProcessManager::substepStates(int stepIndex) const
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT)
        return {};
    QVector<int> result;
    for (auto& s : m_substepStates[stepIndex])
        result.append(static_cast<int>(s));
    return result;
}

QVariantMap ProcessManager::stepParams(int stepIndex) const
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT)
        return {};
    return m_steps[stepIndex].defaultParams;
}

QVariantMap ProcessManager::realtimeData(int stepIndex) const
{
    Q_UNUSED(stepIndex);
    QVariantMap data;
    data["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
    data["cycleCount"] = m_cycleCount;
    return data;
}

// ============================================================
// 流程控制
// ============================================================

void ProcessManager::startCycle()
{
    if (m_running && !m_paused) {
        qDebug() << "[ProcessManager] 已在运行中, 忽略 startCycle";
        return;
    }

    m_running = true;
    m_paused  = false;
    m_substepProgress = 0;

    // 如果还没开始, 从初始化开始
    if (m_currentStepIdx < 0) {
        // 重置所有步骤
        for (int i = 0; i < STEP_COUNT; ++i) {
            m_stepStates[i] = PENDING;
        }
        m_cycleCount = 0;
        executeStep(STEP_INIT);
    } else {
        // 继续执行当前步骤
        executeStep(m_currentStepIdx);
    }

    qDebug() << "[ProcessManager] 启动流程, 当前步骤:" << m_currentStepIdx;
}

void ProcessManager::pauseCycle()
{
    m_paused = true;
    m_stepTimer->stop();
    qDebug() << "[ProcessManager] 暂停循环";
}

void ProcessManager::resumeCycle()
{
    if (!m_running) return;
    m_paused = false;
    if (m_currentStepIdx >= 0) {
        m_stepTimer->start(500);
    }
    qDebug() << "[ProcessManager] 继续循环";
}

void ProcessManager::finishCycle()
{
    m_paused = false;
    // 跳过当前步骤, 直接进入收尾
    executeStep(STEP_FINISH);
    qDebug() << "[ProcessManager] 开始收尾";
}

void ProcessManager::emergencyStop()
{
    m_running = false;
    m_paused  = false;
    m_stepTimer->stop();

    // 全部重置
    for (int i = 0; i < STEP_COUNT; ++i) {
        m_stepStates[i] = PENDING;
        emit stepStateChanged(i, static_cast<int>(PENDING));
    }

    if (m_currentStepIdx >= 0) {
        int old = m_currentStepIdx;
        m_currentStepIdx = -1;
        emit currentStepChanged(-1);
    }

    m_substepProgress = 0;
    qDebug() << "[ProcessManager] 急停!";
}

// ============================================================
// 步骤执行与切换
// ============================================================

void ProcessManager::executeStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= STEP_COUNT) return;

    m_currentStepIdx = stepIndex;
    m_stepStates[stepIndex] = RUNNING;

    // 重置该步骤的子步骤
    int n = m_substepStates[stepIndex].size();
    for (int i = 0; i < n; ++i)
        m_substepStates[stepIndex][i] = PENDING;

    m_substepProgress = 0;

    emit currentStepChanged(stepIndex);
    emit stepStateChanged(stepIndex, static_cast<int>(RUNNING));

    // 第一个子步骤开始
    if (n > 0) {
        m_substepStates[stepIndex][0] = RUNNING;
        emit substepStateChanged(stepIndex, 0, static_cast<int>(RUNNING));
    }

    m_stepTimer->start(500);

    qDebug() << "[ProcessManager] 执行步骤" << stepIndex
             << m_steps[stepIndex].name;
}

void ProcessManager::onStepTimerTick()
{
    if (m_currentStepIdx < 0 || m_currentStepIdx >= STEP_COUNT) {
        m_stepTimer->stop();
        return;
    }

    int stepIdx = m_currentStepIdx;
    int n = m_substepStates[stepIdx].size();

    if (n == 0) {
        // 无子步骤, 直接完成
        completeCurrentStep();
        return;
    }

    // 完成当前子步骤
    m_substepStates[stepIdx][m_substepProgress] = COMPLETED;
    emit substepStateChanged(stepIdx, m_substepProgress, static_cast<int>(COMPLETED));

    m_substepProgress++;

    if (m_substepProgress >= n) {
        // 所有子步骤完成 → 该步骤完成
        completeCurrentStep();
    } else {
        // 下一个子步骤开始
        m_substepStates[stepIdx][m_substepProgress] = RUNNING;
        emit substepStateChanged(stepIdx, m_substepProgress, static_cast<int>(RUNNING));
    }

    // 更新实时数据
    QVariantMap rtData;
    rtData["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
    rtData["stepElapsed"] = m_substepProgress * 0.5;
    rtData["cycleCount"]  = m_cycleCount;
    emit realtimeDataUpdated(stepIdx, rtData);
}

void ProcessManager::completeCurrentStep()
{
    if (m_currentStepIdx < 0) return;

    int completedIdx = m_currentStepIdx;

    m_stepStates[completedIdx] = COMPLETED;
    emit stepStateChanged(completedIdx, static_cast<int>(COMPLETED));

    m_stepTimer->stop();

    qDebug() << "[ProcessManager] 步骤完成:" << m_steps[completedIdx].name;

    // 如果是收尾
    if (completedIdx == STEP_FINISH) {
        m_running = false;
        m_currentStepIdx = -1;
        emit currentStepChanged(-1);
        emit allFinished();
        qDebug() << "[ProcessManager] 收尾完成, 流程结束";
        return;
    }

    // 如果是下料(循环终点)
    if (completedIdx == LOOP_END) {
        m_cycleCount++;
        emit cycleCompleted(m_cycleCount);

        if (m_paused) {
            m_currentStepIdx = -1;
            emit currentStepChanged(-1);
            qDebug() << "[ProcessManager] 循环完成, 暂停中...";
            return;
        }

        // 自动回到循环起点 (上料)
        executeStep(LOOP_START);
        qDebug() << "[ProcessManager] 自动循环 → 上料 (第" << m_cycleCount << "件)";
        return;
    }

    // 正常前进下一步
    advanceToNextStep();
}

void ProcessManager::advanceToNextStep()
{
    int nextIdx = m_currentStepIdx + 1;
    if (nextIdx >= STEP_COUNT) {
        // 不应到达这里 — 收尾已处理
        m_running = false;
        return;
    }

    executeStep(nextIdx);
}
```

- [ ] **Step 2: 编译验证**

```bash
cd d:/my_information/qtProject/chipSetter
qmake chipSetter.pro && make
```
Expected: 编译通过 (ProcessManager.o 链接成功, 可能有未引用的 mainwindow 符号 — 后续 Task 解决)

- [ ] **Step 3: 提交**

```bash
git add core/ProcessManager.cpp
git commit -m "feat: implement ProcessManager — 9-step state machine with auto-loop

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 3: 新建 FlowStepBar 头文件

**Files:**
- Create: `widgets/FlowStepBar.h`

- [ ] **Step 1: 创建 widgets/FlowStepBar.h**

```cpp
/**
 * @file FlowStepBar.h
 * @brief 工艺流程步骤条 — 9步骤块 + 循环框 + 回路箭头
 *
 * 职责:
 *   - 显示9个步骤按钮 (①~⑨)
 *   - 用绿色QFrame包裹循环步骤(②~⑧, index 1~7)
 *   - 在循环框内绘制虚线回路箭头
 *   - 步骤按钮颜色反映状态 (PENDING/RUNNING/COMPLETED/FAILED)
 *   - 发出 stepClicked 信号给 StepDetailPanel
 *
 * 纯UI: 不包含业务逻辑
 */

#ifndef FLOWSTEPBAR_H
#define FLOWSTEPBAR_H

#include <QWidget>
#include <QPushButton>
#include <QFrame>
#include <QVector>
#include <QStringList>

class FlowStepBar : public QWidget
{
    Q_OBJECT

public:
    explicit FlowStepBar(QWidget *parent = nullptr);
    ~FlowStepBar();

    void setSteps(const QStringList& names);

public slots:
    void setStepState(int stepIndex, int state);   // 0=PENDING, 1=RUNNING, 2=COMPLETED, 3=FAILED
    void setCurrentStep(int stepIndex);            // 设置当前活跃步骤 (-1=无)

signals:
    void stepClicked(int stepIndex);

private:
    void setupUI();
    void updateButtonStyle(int stepIndex, int state);

    QVector<QPushButton*> m_stepButtons;   // 9个按钮
    QFrame*               m_loopFrame;     // 循环框
    QWidget*              m_loopContainer; // 循环框内按钮容器
    int                   m_currentStep;   // 当前活跃步骤 (-1=无)

    static const int STEP_COUNT = 9;
    static const int LOOP_START = 1;       // 循环起点 index
    static const int LOOP_END   = 7;       // 循环终点 index
};

#endif // FLOWSTEPBAR_H
```

- [ ] **Step 2: 提交**

```bash
git add widgets/FlowStepBar.h
git commit -m "feat: add FlowStepBar header — process step bar widget interface

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 4: 实现 FlowStepBar

**Files:**
- Create: `widgets/FlowStepBar.cpp`

- [ ] **Step 1: 创建 widgets/FlowStepBar.cpp**

```cpp
/**
 * @file FlowStepBar.cpp
 * @brief FlowStepBar 实现 — 流程步骤条绘制与交互
 */

#include "FlowStepBar.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QStyleOption>

FlowStepBar::FlowStepBar(QWidget *parent)
    : QWidget(parent)
    , m_currentStep(-1)
{
    setupUI();
}

FlowStepBar::~FlowStepBar() {}

// ============================================================
// 构建UI
// ============================================================

void FlowStepBar::setupUI()
{
    setObjectName("FlowStepBar");

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);

    // 步骤按钮将动态创建, 在 setSteps 中
    // 这里先创建占位布局
    mainLayout->addStretch();
}

void FlowStepBar::setSteps(const QStringList& names)
{
    // 清除旧布局
    QLayout* oldLayout = layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }

    m_stepButtons.clear();

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);

    auto addArrow = [&]() {
        QLabel* arrow = new QLabel("→", this);
        arrow->setStyleSheet("color:#455a64; font-size:18px; font-weight:bold;");
        arrow->setFixedWidth(28);
        arrow->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(arrow);
    };

    // ===== 步骤0: 初始化 (循环框外) =====
    QPushButton* btn0 = new QPushButton(this);
    btn0->setText(QString("①\n%1").arg(names.value(0, "初始化")));
    btn0->setFixedSize(72, 60);
    btn0->setCursor(Qt::PointingHandCursor);
    btn0->setProperty("stepIndex", 0);
    connect(btn0, &QPushButton::clicked, this, [this]() {
        int idx = sender()->property("stepIndex").toInt();
        emit stepClicked(idx);
    });
    m_stepButtons.append(btn0);
    mainLayout->addWidget(btn0);

    addArrow();

    // ===== 步骤1~7: 循环框内 =====
    m_loopFrame = new QFrame(this);
    m_loopFrame->setObjectName("LoopFrame");
    m_loopFrame->setStyleSheet(
        "QFrame#LoopFrame {"
        "  background:#0d1a0d;"
        "  border:2px solid #2e7d32;"
        "  border-radius:10px;"
        "}");

    QVBoxLayout* loopOuterLayout = new QVBoxLayout(m_loopFrame);
    loopOuterLayout->setContentsMargins(10, 6, 10, 4);
    loopOuterLayout->setSpacing(0);

    // 循环内按钮行
    QHBoxLayout* loopInnerLayout = new QHBoxLayout();
    loopInnerLayout->setContentsMargins(0, 0, 0, 0);
    loopInnerLayout->setSpacing(0);

    for (int i = LOOP_START; i <= LOOP_END; ++i) {
        if (i > LOOP_START) addArrow(); // 框内箭头 (需要先在loopInnerLayout中处理)

        QPushButton* btn = new QPushButton(this);
        btn->setText(QString("%1\n%2")
                     .arg(QString("①②③④⑤⑥⑦⑧⑨").mid(i, 1))
                     .arg(names.value(i, QString("步骤%1").arg(i+1))));
        btn->setFixedSize(72, 60);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("stepIndex", i);
        connect(btn, &QPushButton::clicked, this, [this]() {
            int idx = sender()->property("stepIndex").toInt();
            emit stepClicked(idx);
        });
        m_stepButtons.append(btn);

        // 注意: 框内箭头需要插在按钮之间
        // 这里简化处理 — 箭头用QLabel加在每个按钮前面(除第一个)
        if (i > LOOP_START) {
            QLabel* innerArrow = new QLabel("→", this);
            innerArrow->setStyleSheet("color:#455a64; font-size:16px; font-weight:bold;");
            innerArrow->setFixedWidth(24);
            innerArrow->setAlignment(Qt::AlignCenter);
            loopInnerLayout->addWidget(innerArrow);
        }
        loopInnerLayout->addWidget(btn);
    }

    loopInnerLayout->addStretch();
    loopOuterLayout->addLayout(loopInnerLayout);

    // 回路虚线箭头 (用QLabel + 特殊字符模拟)
    QLabel* loopArrow = new QLabel("⤴", this);
    loopArrow->setStyleSheet("color:#ffd740; font-size:16px;");
    loopArrow->setAlignment(Qt::AlignCenter);
    loopArrow->setToolTip("循环回到上料");
    loopOuterLayout->addWidget(loopArrow);

    mainLayout->addWidget(m_loopFrame);

    addArrow();

    // ===== 步骤8: 收尾 (循环框外) =====
    QPushButton* btn8 = new QPushButton(this);
    btn8->setText(QString("⑨\n%1").arg(names.value(8, "收尾")));
    btn8->setFixedSize(72, 60);
    btn8->setCursor(Qt::PointingHandCursor);
    btn8->setProperty("stepIndex", 8);
    connect(btn8, &QPushButton::clicked, this, [this]() {
        int idx = sender()->property("stepIndex").toInt();
        emit stepClicked(idx);
    });
    m_stepButtons.append(btn8);
    mainLayout->addWidget(btn8);

    mainLayout->addStretch();

    // 应用初始样式
    for (int i = 0; i < m_stepButtons.size(); ++i) {
        updateButtonStyle(i, 0); // PENDING
    }
}

// ============================================================
// 状态更新
// ============================================================

void FlowStepBar::setStepState(int stepIndex, int state)
{
    if (stepIndex < 0 || stepIndex >= m_stepButtons.size()) return;
    updateButtonStyle(stepIndex, state);
}

void FlowStepBar::setCurrentStep(int stepIndex)
{
    m_currentStep = stepIndex;
}

// ============================================================
// 按钮样式
// ============================================================

void FlowStepBar::updateButtonStyle(int stepIndex, int state)
{
    if (stepIndex < 0 || stepIndex >= m_stepButtons.size()) return;
    QPushButton* btn = m_stepButtons[stepIndex];

    QString baseStyle;
    switch (state) {
    case 0: // PENDING — 灰色
        baseStyle =
            "QPushButton {"
            "  background:#2a2a3a; color:#666;"
            "  border:1px solid #333; border-radius:5px;"
            "  font-size:11px; padding:8px 10px;"
            "}";
        break;
    case 1: // RUNNING — 蓝色发光
        baseStyle =
            "QPushButton {"
            "  background:#1565c0; color:#fff;"
            "  border:2px solid #64b5f6; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    case 2: // COMPLETED — 绿色
        baseStyle =
            "QPushButton {"
            "  background:#1b5e20; color:#81c784;"
            "  border:1px solid #388e3c; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    case 3: // FAILED — 红色
        baseStyle =
            "QPushButton {"
            "  background:#b71c1c; color:#ef9a9a;"
            "  border:1px solid #f44336; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    default:
        break;
    }

    btn->setStyleSheet(baseStyle);
}
```

- [ ] **Step 2: 提交**

```bash
git add widgets/FlowStepBar.cpp
git commit -m "feat: implement FlowStepBar — 9-step bar with loop frame and return arrow

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 5: 新建 StepDetailPanel 头文件与实现

**Files:**
- Create: `widgets/StepDetailPanel.h`
- Create: `widgets/StepDetailPanel.cpp`

- [ ] **Step 1: 创建 widgets/StepDetailPanel.h**

```cpp
/**
 * @file StepDetailPanel.h
 * @brief 步骤详情面板 — 子步骤列表 + 步骤参数 + 实时状态 三栏布局
 *
 * 职责:
 *   - 三栏展示当前选中步骤的详细信息
 *   - 左栏: 子步骤列表 (带状态标记)
 *   - 中栏: 步骤参数 (名值对)
 *   - 右栏: 实时状态数据
 *
 * 纯UI: 通过槽函数接收数据, 不包含业务逻辑
 */

#ifndef STEPDETAILPANEL_H
#define STEPDETAILPANEL_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class StepDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StepDetailPanel(QWidget *parent = nullptr);
    ~StepDetailPanel();

public slots:
    // 切换步骤详情
    void showStepDetail(int stepIndex,
                        const QString& stepName,
                        const QStringList& substeps,
                        const QVector<int>& substepStates,
                        const QVariantMap& params,
                        const QVariantMap& realtimeData);

    // 增量更新 (避免全量刷新)
    void updateSubstepState(int stepIndex, int substepIndex, int state);
    void updateRealtimeData(const QVariantMap& data);

private:
    void setupUI();
    void clearContent();

    // 标题
    QLabel* m_titleLabel;

    // 左栏: 子步骤
    QLabel*     m_substepTitle;
    QVBoxLayout* m_substepLayout;
    QVector<QLabel*> m_substepLabels;

    // 中栏: 参数
    QLabel*     m_paramTitle;
    QGridLayout* m_paramLayout;
    QVector<QPair<QLabel*, QLabel*>> m_paramLabels;

    // 右栏: 实时状态
    QLabel*     m_realtimeTitle;
    QGridLayout* m_realtimeLayout;
    QVector<QPair<QLabel*, QLabel*>> m_realtimeLabels;

    int m_currentStepIndex;
};

#endif // STEPDETAILPANEL_H
```

- [ ] **Step 2: 创建 widgets/StepDetailPanel.cpp**

```cpp
/**
 * @file StepDetailPanel.cpp
 * @brief StepDetailPanel 实现
 */

#include "StepDetailPanel.h"
#include <QHBoxLayout>
#include <QFrame>

StepDetailPanel::StepDetailPanel(QWidget *parent)
    : QWidget(parent)
    , m_currentStepIndex(-1)
{
    setupUI();
}

StepDetailPanel::~StepDetailPanel() {}

void StepDetailPanel::setupUI()
{
    setObjectName("StepDetailPanel");
    setStyleSheet(
        "QWidget#StepDetailPanel {"
        "  background:#151525;"
        "  border:1px dashed #64b5f6;"
        "  border-radius:8px;"
        "}");

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(10, 8, 10, 8);
    outerLayout->setSpacing(6);

    // 标题
    m_titleLabel = new QLabel("步骤详情", this);
    m_titleLabel->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold;"
        "border:none;");
    outerLayout->addWidget(m_titleLabel);

    // 三栏
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(8);

    // --- 左栏: 子步骤列表 ---
    QFrame* leftCol = new QFrame(this);
    leftCol->setStyleSheet(
        "QFrame{background:#1a1a30; border-radius:4px; border:none;}");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftCol);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(4);

    m_substepTitle = new QLabel("子步骤列表", this);
    m_substepTitle->setStyleSheet(
        "color:#78909c; font-size:10px; border:none;");
    leftLayout->addWidget(m_substepTitle);

    m_substepLayout = new QVBoxLayout();
    m_substepLayout->setSpacing(2);
    leftLayout->addLayout(m_substepLayout);
    leftLayout->addStretch();

    // --- 中栏: 步骤参数 ---
    QFrame* midCol = new QFrame(this);
    midCol->setStyleSheet(
        "QFrame{background:#1a1a30; border-radius:4px; border:none;}");
    QVBoxLayout* midLayout = new QVBoxLayout(midCol);
    midLayout->setContentsMargins(8, 8, 8, 8);
    midLayout->setSpacing(4);

    m_paramTitle = new QLabel("步骤参数", this);
    m_paramTitle->setStyleSheet(
        "color:#78909c; font-size:10px; border:none;");
    midLayout->addWidget(m_paramTitle);

    m_paramLayout = new QGridLayout();
    m_paramLayout->setSpacing(2);
    midLayout->addLayout(m_paramLayout);
    midLayout->addStretch();

    // --- 右栏: 实时状态 ---
    QFrame* rightCol = new QFrame(this);
    rightCol->setStyleSheet(
        "QFrame{background:#1a1a30; border-radius:4px; border:none;}");
    QVBoxLayout* rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(4);

    m_realtimeTitle = new QLabel("实时状态", this);
    m_realtimeTitle->setStyleSheet(
        "color:#78909c; font-size:10px; border:none;");
    rightLayout->addWidget(m_realtimeTitle);

    m_realtimeLayout = new QGridLayout();
    m_realtimeLayout->setSpacing(2);
    rightLayout->addLayout(m_realtimeLayout);
    rightLayout->addStretch();

    columnsLayout->addWidget(leftCol, 1);
    columnsLayout->addWidget(midCol, 1);
    columnsLayout->addWidget(rightCol, 1);

    outerLayout->addLayout(columnsLayout, 1);
}

// ============================================================
// 显示步骤详情
// ============================================================

void StepDetailPanel::showStepDetail(int stepIndex,
                                      const QString& stepName,
                                      const QStringList& substeps,
                                      const QVector<int>& substepStates,
                                      const QVariantMap& params,
                                      const QVariantMap& realtimeData)
{
    m_currentStepIndex = stepIndex;

    // 更新标题
    QString stepChar = QString("①②③④⑤⑥⑦⑧⑨").mid(stepIndex, 1);
    m_titleLabel->setText(
        QString("步骤详情 — %1 %2").arg(stepChar, stepName));

    // --- 清除旧内容 ---
    clearContent();

    // --- 左栏: 子步骤 ---
    static const char* stateMarks[] = {"○", "◉", "✓", "✕"};
    static const char* stateColors[] = {"#666", "#ffd740", "#81c784", "#ef9a9a"};

    for (int i = 0; i < substeps.size(); ++i) {
        int s = (i < substepStates.size()) ? substepStates[i] : 0;
        QLabel* label = new QLabel(
            QString("%1 %2").arg(stateMarks[s], substeps[i]), this);
        label->setStyleSheet(
            QString("color:%1; font-size:9px; border:none;").arg(stateColors[s]));
        m_substepLayout->addWidget(label);
        m_substepLabels.append(label);
    }

    // --- 中栏: 参数 ---
    QStringList paramKeys = params.keys();
    for (int i = 0; i < paramKeys.size(); ++i) {
        QLabel* nameLabel = new QLabel(paramKeys[i], this);
        nameLabel->setStyleSheet(
            "color:#78909c; font-size:9px; border:none;");
        QLabel* valueLabel = new QLabel(params[paramKeys[i]].toString(), this);
        valueLabel->setStyleSheet(
            "color:#fff; font-size:9px; border:none;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_paramLayout->addWidget(nameLabel, i, 0);
        m_paramLayout->addWidget(valueLabel, i, 1);
        m_paramLabels.append({nameLabel, valueLabel});
    }

    // --- 右栏: 实时状态 ---
    QStringList rtKeys = realtimeData.keys();
    for (int i = 0; i < rtKeys.size(); ++i) {
        QLabel* nameLabel = new QLabel(rtKeys[i], this);
        nameLabel->setStyleSheet(
            "color:#78909c; font-size:9px; border:none;");
        QLabel* valueLabel = new QLabel(realtimeData[rtKeys[i]].toString(), this);
        valueLabel->setStyleSheet(
            "color:#4caf50; font-size:9px; border:none;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_realtimeLayout->addWidget(nameLabel, i, 0);
        m_realtimeLayout->addWidget(valueLabel, i, 1);
        m_realtimeLabels.append({nameLabel, valueLabel});
    }
}

void StepDetailPanel::updateSubstepState(int stepIndex, int substepIndex, int state)
{
    if (stepIndex != m_currentStepIndex) return;
    if (substepIndex < 0 || substepIndex >= m_substepLabels.size()) return;

    static const char* stateMarks[] = {"○", "◉", "✓", "✕"};
    static const char* stateColors[] = {"#666", "#ffd740", "#81c784", "#ef9a9a"};

    QString text = m_substepLabels[substepIndex]->text();
    // 保留文字, 替换标记
    int spacePos = text.indexOf(' ');
    QString rest = (spacePos >= 0) ? text.mid(spacePos + 1) : text;

    m_substepLabels[substepIndex]->setText(
        QString("%1 %2").arg(stateMarks[state], rest));
    m_substepLabels[substepIndex]->setStyleSheet(
        QString("color:%1; font-size:9px; border:none;").arg(stateColors[state]));
}

void StepDetailPanel::updateRealtimeData(const QVariantMap& data)
{
    // 更新实时状态列的值
    QStringList keys = data.keys();
    for (int i = 0; i < keys.size() && i < m_realtimeLabels.size(); ++i) {
        if (m_realtimeLabels[i].first->text() == keys[i]) {
            m_realtimeLabels[i].second->setText(data[keys[i]].toString());
        }
    }
}

void StepDetailPanel::clearContent()
{
    // 清除子步骤标签
    for (auto* label : m_substepLabels) {
        m_substepLayout->removeWidget(label);
        delete label;
    }
    m_substepLabels.clear();

    // 清除参数标签
    for (auto& pair : m_paramLabels) {
        m_paramLayout->removeWidget(pair.first);
        m_paramLayout->removeWidget(pair.second);
        delete pair.first;
        delete pair.second;
    }
    m_paramLabels.clear();

    // 清除实时状态标签
    for (auto& pair : m_realtimeLabels) {
        m_realtimeLayout->removeWidget(pair.first);
        m_realtimeLayout->removeWidget(pair.second);
        delete pair.first;
        delete pair.second;
    }
    m_realtimeLabels.clear();
}
```

- [ ] **Step 3: 提交**

```bash
git add widgets/StepDetailPanel.h widgets/StepDetailPanel.cpp
git commit -m "feat: add StepDetailPanel — 3-column step detail (substeps/params/realtime)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 6: 新建 DeviceStatusWidget 头文件与实现

**Files:**
- Create: `widgets/DeviceStatusWidget.h`
- Create: `widgets/DeviceStatusWidget.cpp`

- [ ] **Step 1: 创建 widgets/DeviceStatusWidget.h**

```cpp
/**
 * @file DeviceStatusWidget.h
 * @brief 设备状态概览 — 13轴灯 + IO关键信号 + 通讯状态
 */

#ifndef DEVICESTATUSWIDGET_H
#define DEVICESTATUSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>

class DeviceStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceStatusWidget(QWidget *parent = nullptr);
    ~DeviceStatusWidget();

public slots:
    void updateAxisStates(const QVector<int>& enabled,
                          const QVector<int>& moving,
                          const QVector<int>& alarmed);
    void setEmStopStatus(bool triggered);
    void setConnectionStatus(bool connected);
    void updateActiveIoSignals(const QStringList& activeSignals);

private:
    void setupUI();

    QVector<QLabel*> m_axisLights;
    QVector<QLabel*> m_axisNames;
    QLabel* m_emStopLabel;
    QLabel* m_connLabel;
    QLabel* m_ioSummaryLabel;

    static const int AXIS_COUNT = 13;
};

#endif // DEVICESTATUSWIDGET_H
```

- [ ] **Step 2: 创建 widgets/DeviceStatusWidget.cpp**

```cpp
/**
 * @file DeviceStatusWidget.cpp
 * @brief DeviceStatusWidget 实现
 */

#include "DeviceStatusWidget.h"
#include "core/HardwareConfig.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

DeviceStatusWidget::DeviceStatusWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

DeviceStatusWidget::~DeviceStatusWidget() {}

void DeviceStatusWidget::setupUI()
{
    setObjectName("DeviceStatusWidget");
    setStyleSheet(
        "QWidget#DeviceStatusWidget {"
        "  background:#1e1e30; border-radius:8px;"
        "}");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(6);

    // 标题
    QLabel* title = new QLabel("设备状态", this);
    title->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    mainLayout->addWidget(title);

    // 轴指示灯行
    QHBoxLayout* axisLayout = new QHBoxLayout();
    axisLayout->setSpacing(6);

    QStringList axisNames = AXIS_NAMES;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        QString shortName = (i < axisNames.size())
            ? axisNames[i] : QString("轴%1").arg(i + 1);

        QLabel* light = new QLabel("●", this);
        light->setStyleSheet("color:#555; font-size:10px; border:none;");
        light->setFixedWidth(14);
        light->setAlignment(Qt::AlignCenter);

        QLabel* name = new QLabel(shortName, this);
        name->setStyleSheet("color:#78909c; font-size:8px; border:none;");
        name->setAlignment(Qt::AlignCenter);

        QVBoxLayout* cell = new QVBoxLayout();
        cell->setSpacing(1);
        cell->addWidget(light, 0, Qt::AlignCenter);
        cell->addWidget(name, 0, Qt::AlignCenter);

        axisLayout->addLayout(cell);

        m_axisLights.append(light);
        m_axisNames.append(name);
    }

    axisLayout->addStretch();
    mainLayout->addLayout(axisLayout);

    // IO & 通讯行
    QHBoxLayout* infoRow = new QHBoxLayout();
    infoRow->setSpacing(16);

    m_emStopLabel = new QLabel("急停: 正常", this);
    m_emStopLabel->setStyleSheet(
        "color:#4caf50; font-size:9px; border:none;");
    infoRow->addWidget(m_emStopLabel);

    m_ioSummaryLabel = new QLabel("", this);
    m_ioSummaryLabel->setStyleSheet(
        "color:#78909c; font-size:9px; border:none;");
    infoRow->addWidget(m_ioSummaryLabel);

    infoRow->addStretch();

    m_connLabel = new QLabel("GNC未连接", this);
    m_connLabel->setStyleSheet(
        "color:#f44336; font-size:9px; border:none;");
    infoRow->addWidget(m_connLabel);

    mainLayout->addLayout(infoRow);
}

// ============================================================
// 槽函数
// ============================================================

void DeviceStatusWidget::updateAxisStates(const QVector<int>& enabled,
                                           const QVector<int>& moving,
                                           const QVector<int>& alarmed)
{
    for (int i = 0; i < m_axisLights.size() && i < AXIS_COUNT; ++i) {
        int axisId = i + 1;
        QString color = "#555"; // 灰色: 未使能

        if (alarmed.contains(axisId))
            color = "#f44336";  // 红色: 报警
        else if (moving.contains(axisId))
            color = "#2196f3";  // 蓝色: 运动中
        else if (enabled.contains(axisId))
            color = "#4caf50";  // 绿色: 就绪

        m_axisLights[i]->setStyleSheet(
            QString("color:%1; font-size:10px; border:none;").arg(color));
    }
}

void DeviceStatusWidget::setEmStopStatus(bool triggered)
{
    if (triggered) {
        m_emStopLabel->setText("急停: 触发!");
        m_emStopLabel->setStyleSheet(
            "color:#f44336; font-size:9px; font-weight:bold; border:none;");
    } else {
        m_emStopLabel->setText("急停: 正常");
        m_emStopLabel->setStyleSheet(
            "color:#4caf50; font-size:9px; border:none;");
    }
}

void DeviceStatusWidget::setConnectionStatus(bool connected)
{
    if (connected) {
        m_connLabel->setText("GNC已连接");
        m_connLabel->setStyleSheet(
            "color:#4caf50; font-size:9px; border:none;");
    } else {
        m_connLabel->setText("GNC未连接");
        m_connLabel->setStyleSheet(
            "color:#f44336; font-size:9px; border:none;");
    }
}

void DeviceStatusWidget::updateActiveIoSignals(const QStringList& activeSignals)
{
    if (activeSignals.isEmpty()) {
        m_ioSummaryLabel->setText("");
        return;
    }
    m_ioSummaryLabel->setText(
        QString("IO: %1").arg(activeSignals.join(", ")));
}
```

- [ ] **Step 3: 提交**

```bash
git add widgets/DeviceStatusWidget.h widgets/DeviceStatusWidget.cpp
git commit -m "feat: add DeviceStatusWidget — axis lights + IO summary + connection status

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 7: 重写 ProductionWidget

**Files:**
- Modify: `widgets/ProductionWidget.h`
- Modify: `widgets/ProductionWidget.cpp`

- [ ] **Step 1: 重写 widgets/ProductionWidget.h**

```cpp
/**
 * @file ProductionWidget.h
 * @brief 生产运行界面 — 工艺流程UI (操作员视角)
 *
 * 组合组件:
 *   - FlowStepBar       (工艺流程步骤条)
 *   - StepDetailPanel   (步骤详情三栏)
 *   - DeviceStatusWidget(设备状态)
 *   - AlarmListWidget   (报警列表, 复用)
 *
 * 保持原有信号签名向后兼容
 */

#ifndef PRODUCTIONWIDGET_H
#define PRODUCTIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>
#include "models/AlarmRecord.h"

// 前向声明
class FlowStepBar;
class StepDetailPanel;
class DeviceStatusWidget;
class QLabel;

class ProductionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProductionWidget(QWidget *parent = nullptr);
    ~ProductionWidget();

    // ---- 暴露子组件访问 (供 MainWindow 连接信号) ----
    FlowStepBar*        flowStepBar() const;
    StepDetailPanel*    stepDetailPanel() const;
    DeviceStatusWidget* deviceStatusWidget() const;

public slots:
    // ---- 产量/统计 (向后兼容) ----
    void setTotalCount(int count);
    void setCycleTime(double seconds);
    void setRunningTime(double hours);

    // ---- 状态 (向后兼容) ----
    void setRunStatus(bool running);
    void setMode(int mode);
    void setConnectionStatus(bool connected);

    // ---- 轴状态 (向后兼容) ----
    void updateAxisStates(const QVector<int>& enabledAxes,
                          const QVector<int>& movingAxes,
                          const QVector<int>& alarmedAxes);

    // ---- 报警 (向后兼容) ----
    void onNewAlarm(const QString& level, const QString& msg);
    void onClearAlarms();

signals:
    void switchToDebugMode();

private:
    void setupUI();

    // 组件
    FlowStepBar*        m_flowStepBar;
    StepDetailPanel*    m_stepDetailPanel;
    DeviceStatusWidget* m_deviceStatus;

    // 状态标签 (产量等)
    QLabel* m_modeLabel;
    QLabel* m_countLabel;
    QLabel* m_cycleLabel;
    QLabel* m_runtimeLabel;

    // 报警
    QLabel* m_alarmBadge;
    QLabel* m_alarmList;
    int     m_alarmCount;
};

#endif // PRODUCTIONWIDGET_H
```

- [ ] **Step 2: 重写 widgets/ProductionWidget.cpp**

```cpp
/**
 * @file ProductionWidget.cpp
 * @brief 生产运行界面 — 工艺流程UI实现
 */

#include "ProductionWidget.h"
#include "widgets/FlowStepBar.h"
#include "widgets/StepDetailPanel.h"
#include "widgets/DeviceStatusWidget.h"
#include "core/HardwareConfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>

ProductionWidget::ProductionWidget(QWidget *parent)
    : QWidget(parent)
    , m_alarmCount(0)
{
    setupUI();
}

ProductionWidget::~ProductionWidget() {}

FlowStepBar* ProductionWidget::flowStepBar() const
{ return m_flowStepBar; }

StepDetailPanel* ProductionWidget::stepDetailPanel() const
{ return m_stepDetailPanel; }

DeviceStatusWidget* ProductionWidget::deviceStatusWidget() const
{ return m_deviceStatus; }

// ============================================================
// 构建UI
// ============================================================

void ProductionWidget::setupUI()
{
    setObjectName("ProductionWidget");
    setStyleSheet("QWidget#ProductionWidget { background-color:#1a1a2e; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ====== 顶部标题栏 ======
    QHBoxLayout* titleRow = new QHBoxLayout();

    QLabel* title = new QLabel("◆ 固晶机控制系统", this);
    title->setStyleSheet(
        "font-weight:bold; color:#00e676; font-size:14px; border:none;");
    titleRow->addWidget(title);
    titleRow->addStretch();

    m_modeLabel = new QLabel("手动模式", this);
    m_modeLabel->setStyleSheet(
        "background:#e65100; color:#fff; padding:3px 14px;"
        "border-radius:3px; font-weight:bold; font-size:11px;");
    titleRow->addWidget(m_modeLabel);

    // 切换到调试模式按钮
    QPushButton* dbgBtn = new QPushButton("调试", this);
    dbgBtn->setFixedSize(56, 26);
    dbgBtn->setStyleSheet(
        "QPushButton{background:#37474f;color:#90a4ae;"
        "border:1px solid #555;border-radius:3px;font-size:10px;}"
        "QPushButton:hover{background:#455a64;color:#fff;}");
    connect(dbgBtn, &QPushButton::clicked,
            this, &ProductionWidget::switchToDebugMode);
    titleRow->addWidget(dbgBtn);

    mainLayout->addLayout(titleRow);

    // ====== 主内容 ======
    QHBoxLayout* bodyRow = new QHBoxLayout();
    bodyRow->setSpacing(6);

    // ---- 左侧: 流程 + 详情 + 设备状态 ----
    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(6);

    // 2. 工艺流程
    QFrame* flowFrame = new QFrame(this);
    flowFrame->setStyleSheet(
        "QFrame{background:#1e1e30; border-radius:8px;}");
    QVBoxLayout* flowFrameLayout = new QVBoxLayout(flowFrame);
    QLabel* flowTitle = new QLabel("工艺流程", this);
    flowTitle->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    flowFrameLayout->addWidget(flowTitle);

    m_flowStepBar = new FlowStepBar(this);
    QStringList stepNames = {"初始化", "上料", "定位", "顶晶", "拾取",
                             "点胶", "贴装", "下料", "收尾"};
    m_flowStepBar->setSteps(stepNames);
    flowFrameLayout->addWidget(m_flowStepBar);

    leftCol->addWidget(flowFrame);

    // 3. 步骤详情
    QFrame* detailFrame = new QFrame(this);
    detailFrame->setStyleSheet(
        "QFrame{background:#151525; border:1px dashed #64b5f6; border-radius:8px;}");
    QVBoxLayout* detailFrameLayout = new QVBoxLayout(detailFrame);
    m_stepDetailPanel = new StepDetailPanel(this);
    detailFrameLayout->addWidget(m_stepDetailPanel);
    leftCol->addWidget(detailFrame, 1);

    // 4. 设备状态
    m_deviceStatus = new DeviceStatusWidget(this);
    leftCol->addWidget(m_deviceStatus);

    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftCol);
    bodyRow->addWidget(leftWidget, 3);

    // ---- 右侧: 报警列表 ----
    QFrame* rightFrame = new QFrame(this);
    rightFrame->setStyleSheet(
        "QFrame{background:#1a1a2e; border:1px solid #333; border-radius:8px;}");
    QVBoxLayout* rightCol = new QVBoxLayout(rightFrame);
    rightCol->setContentsMargins(8, 8, 8, 8);
    rightCol->setSpacing(6);

    QLabel* alarmTitle = new QLabel("报警列表", this);
    alarmTitle->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    rightCol->addWidget(alarmTitle);

    m_alarmBadge = new QLabel("0", this);
    m_alarmBadge->setStyleSheet(
        "background:#37474f; color:#90a4ae; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");
    rightCol->addWidget(m_alarmBadge);

    m_alarmList = new QLabel("暂无报警", this);
    m_alarmList->setStyleSheet(
        "color:#78909c; font-size:9px;"
        "background:#1e1e30; border-radius:4px; padding:8px;");
    m_alarmList->setWordWrap(true);
    m_alarmList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    rightCol->addWidget(m_alarmList, 1);

    QPushButton* clearAlarmBtn = new QPushButton("清除报警", this);
    clearAlarmBtn->setFixedHeight(26);
    clearAlarmBtn->setStyleSheet(
        "QPushButton{background:#333;color:#ccc;border:none;"
        "border-radius:3px;font-size:9px;}"
        "QPushButton:hover{background:#444;}");
    connect(clearAlarmBtn, &QPushButton::clicked,
            this, &ProductionWidget::onClearAlarms);
    rightCol->addWidget(clearAlarmBtn);

    bodyRow->addWidget(rightFrame, 1);

    mainLayout->addLayout(bodyRow, 1);
}

// ============================================================
// 槽函数 (向后兼容)
// ============================================================

void ProductionWidget::setTotalCount(int count)
{
    // 产量由MainWindow的状态栏处理, 这里不需要
    Q_UNUSED(count);
}

void ProductionWidget::setCycleTime(double seconds)
{
    Q_UNUSED(seconds);
}

void ProductionWidget::setRunningTime(double hours)
{
    Q_UNUSED(hours);
}

void ProductionWidget::setRunStatus(bool running)
{
    Q_UNUSED(running);
}

void ProductionWidget::setMode(int mode)
{
    if (mode == 0) {
        m_modeLabel->setText("手动模式");
        m_modeLabel->setStyleSheet(
            "background:#e65100; color:#fff; padding:3px 14px;"
            "border-radius:3px; font-weight:bold; font-size:11px;");
    } else {
        m_modeLabel->setText("自动模式");
        m_modeLabel->setStyleSheet(
            "background:#00e676; color:#000; padding:3px 14px;"
            "border-radius:3px; font-weight:bold; font-size:11px;");
    }
}

void ProductionWidget::setConnectionStatus(bool connected)
{
    m_deviceStatus->setConnectionStatus(connected);
}

void ProductionWidget::updateAxisStates(const QVector<int>& enabledAxes,
                                         const QVector<int>& movingAxes,
                                         const QVector<int>& alarmedAxes)
{
    m_deviceStatus->updateAxisStates(enabledAxes, movingAxes, alarmedAxes);
}

void ProductionWidget::onNewAlarm(const QString& level, const QString& msg)
{
    m_alarmCount++;
    m_alarmBadge->setText(QString::number(m_alarmCount));
    m_alarmBadge->setStyleSheet(
        "background:#b71c1c; color:#fff; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");

    QStringList lines = m_alarmList->text().split('\n', Qt::SkipEmptyParts);
    if (lines.size() == 1 && lines[0] == "暂无报警") lines.clear();

    QString icon = (level == "FATAL") ? "!" : "?";
    QString color = (level == "FATAL") ? "#ef9a9a" : "#ffe0b2";
    lines.prepend(QString("<span style='color:%1'>%2 %3</span>")
                  .arg(color, icon, msg));
    if (lines.size() > 8) lines = lines.mid(0, 8);

    m_alarmList->setText(lines.join('\n'));
}

void ProductionWidget::onClearAlarms()
{
    m_alarmCount = 0;
    m_alarmBadge->setText("0");
    m_alarmBadge->setStyleSheet(
        "background:#37474f; color:#90a4ae; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");
    m_alarmList->setText("暂无报警");
}
```

- [ ] **Step 3: 提交**

```bash
git add widgets/ProductionWidget.h widgets/ProductionWidget.cpp
git commit -m "feat: rewrite ProductionWidget — compose FlowStepBar + StepDetailPanel + DeviceStatus

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 8: 更新 MainWindow 连线

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] **Step 1: 修改 mainwindow.h — 新增 ProcessManager 成员**

找到 `mainwindow.h` 中的 `#include "core/StatsCollector.h"` 行之后添加:

```cpp
#include "core/ProcessManager.h"
```

在 `StatsCollector* m_statsCollector;` 行之后添加:

```cpp
    ProcessManager*     m_processManager;
```

- [ ] **Step 2: 修改 mainwindow.cpp — createCoreModules() 中创建 ProcessManager**

在 `createCoreModules()` 函数末尾 ( `m_statsCollector = new StatsCollector(this);` 之后) 添加:

```cpp
    m_processManager = new ProcessManager(this);
```

- [ ] **Step 3: 修改 mainwindow.cpp — connectSignals() 添加新信号连接**

在 `connectSignals()` 函数中的"生产界面信号"部分之后, 添加新的连接块:

```cpp
    // ===== 工艺流程信号 =====

    // 流程步骤块点击 → 显示详情
    connect(m_production->flowStepBar(), &FlowStepBar::stepClicked,
            this, [this](int stepIndex) {
        const ProcessManager::StepDef& def = m_processManager->stepDef(stepIndex);
        m_production->stepDetailPanel()->showStepDetail(
            stepIndex,
            def.name,
            def.substeps,
            m_processManager->substepStates(stepIndex),
            m_processManager->stepParams(stepIndex),
            m_processManager->realtimeData(stepIndex));
    });

    // ProcessManager 步骤状态变化 → FlowStepBar
    connect(m_processManager, &ProcessManager::stepStateChanged,
            m_production->flowStepBar(), &FlowStepBar::setStepState);

    // ProcessManager 当前步骤变化 → FlowStepBar
    connect(m_processManager, &ProcessManager::currentStepChanged,
            m_production->flowStepBar(), &FlowStepBar::setCurrentStep);

    // ProcessManager 子步骤变化 → StepDetailPanel
    connect(m_processManager, &ProcessManager::substepStateChanged,
            m_production->stepDetailPanel(), &StepDetailPanel::updateSubstepState);

    // ProcessManager 实时数据 → StepDetailPanel
    connect(m_processManager, &ProcessManager::realtimeDataUpdated,
            m_production->stepDetailPanel(), &StepDetailPanel::updateRealtimeData);

    // ProcessManager 循环完成 → StatsCollector 产量+1
    connect(m_processManager, &ProcessManager::cycleCompleted,
            this, [this](int total) {
        m_statsCollector->incrementCount();
        Q_UNUSED(total);
    });

    // 底部操作栏启动/暂停 → ProcessManager
    // 注意: 需要修改原有 startClicked/pauseClicked 的连接
    // 替换原有的 startClicked 连接为:
    disconnect(m_bottomBar, &BottomBarWidget::startClicked, nullptr, nullptr);
    connect(m_bottomBar, &BottomBarWidget::startClicked,
            this, [this]() {
        m_statusBar->setRunStatus(true);
        m_production->setRunStatus(true);
        m_statsCollector->start();
        m_processManager->startCycle();   // 新增: 启动流程
    });

    disconnect(m_bottomBar, &BottomBarWidget::pauseClicked, nullptr, nullptr);
    connect(m_bottomBar, &BottomBarWidget::pauseClicked,
            this, [this]() {
        m_statusBar->setRunStatus(false);
        m_production->setRunStatus(false);
        m_statsCollector->pause();
        m_processManager->pauseCycle();   // 新增: 暂停流程
    });

    disconnect(m_bottomBar, &BottomBarWidget::resetClicked, nullptr, nullptr);
    connect(m_bottomBar, &BottomBarWidget::resetClicked,
            this, [this]() {
        m_alarmLogger->clearAll();
        m_statsCollector->reset();
        m_production->onClearAlarms();
        m_processManager->finishCycle();  // 新增: 触发收尾
    });

    // 急停 → ProcessManager
    // 在原有急停连接中添加:
    // (修改现有的 emergencyStopClicked 连接)
    disconnect(m_bottomBar, &BottomBarWidget::emergencyStopClicked, nullptr, nullptr);
    connect(m_bottomBar, &BottomBarWidget::emergencyStopClicked,
            this, [this]() {
        m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "操作", "操作员触发急停");
        m_processManager->emergencyStop();  // 新增: 流程急停
    });
```

- [ ] **Step 4: 修改 mainwindow.cpp — initSystem() 中初始化 ProcessManager**

在 `initSystem()` 中, 在 `m_production->setTotalCount(0);` 之后添加:

```cpp
    // 初始化流程步骤名
    m_production->flowStepBar()->setSteps(
        {"初始化", "上料", "定位", "顶晶", "拾取", "点胶", "贴装", "下料", "收尾"}
    );
```

- [ ] **Step 5: 更新 chipSetter.pro 添加新源文件**

在 `SOURCES` 中添加:
```
    core/ProcessManager.cpp \
    widgets/FlowStepBar.cpp \
    widgets/StepDetailPanel.cpp \
    widgets/DeviceStatusWidget.cpp
```

在 `HEADERS` 中添加:
```
    core/ProcessManager.h \
    widgets/FlowStepBar.h \
    widgets/StepDetailPanel.h \
    widgets/DeviceStatusWidget.h
```

- [ ] **Step 6: 编译验证**

```bash
cd d:/my_information/qtProject/chipSetter
qmake chipSetter.pro && make
```
Expected: 编译通过, 链接成功

- [ ] **Step 7: 提交**

```bash
git add mainwindow.h mainwindow.cpp chipSetter.pro
git commit -m "feat: wire ProcessManager + new widgets into MainWindow

- Add ProcessManager to core modules
- Connect FlowStepBar/StepDetailPanel/DeviceStatus signals
- Update BottomBar button connections to drive process flow
- Add new files to chipSetter.pro

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 9: ProcessManager 单元测试

**Files:**
- Create: `tests/test_process_manager.pro`
- Create: `tests/test_process_manager.cpp`
- Modify: `tests/tests.pro`

- [ ] **Step 1: 创建 tests/test_process_manager.pro**

```qmake
QT += core testlib
CONFIG += c++11 console
TARGET = test_process_manager
INCLUDEPATH += ..

SOURCES += test_process_manager.cpp \
           ../core/ProcessManager.cpp
```

- [ ] **Step 2: 创建 tests/test_process_manager.cpp**

```cpp
/**
 * @file test_process_manager.cpp
 * @brief ProcessManager 单元测试
 */
#include <QtTest>
#include <QSignalSpy>
#include "core/ProcessManager.h"

class TestProcessManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testInitialState();
    void testStartCycle();
    void testStepProgression();
    void testLoopBehavior();
    void testPauseAndResume();
    void testFinishCycle();
    void testEmergencyStop();
    void testStepDefinitions();
};

void TestProcessManager::initTestCase()
{
    qRegisterMetaType<ProcessManager::StepState>("ProcessManager::StepState");
}

void TestProcessManager::testInitialState()
{
    ProcessManager pm;
    QCOMPARE(pm.currentStep(), -1);
    QCOMPARE(pm.isRunning(), false);
    QCOMPARE(pm.isPaused(), false);
    QCOMPARE(pm.cycleCount(), 0);

    // 所有步骤初始状态为 PENDING
    for (int i = 0; i < 9; ++i) {
        QCOMPARE(pm.stepState(i), ProcessManager::PENDING);
    }
}

void TestProcessManager::testStartCycle()
{
    ProcessManager pm;
    QSignalSpy spyStep(&pm, &ProcessManager::stepStateChanged);
    QSignalSpy spyCurrent(&pm, &ProcessManager::currentStepChanged);

    pm.startCycle();

    QCOMPARE(pm.isRunning(), true);
    QCOMPARE(pm.isPaused(), false);
    QCOMPARE(pm.currentStep(), 0); // 从初始化开始

    // 步骤0应变为 RUNNING
    QCOMPARE(pm.stepState(0), ProcessManager::RUNNING);
    QVERIFY(spyStep.count() >= 1);
    QVERIFY(spyCurrent.count() >= 1);
}

void TestProcessManager::testStepProgression()
{
    ProcessManager pm;
    QSignalSpy spyStep(&pm, &ProcessManager::stepStateChanged);

    pm.startCycle();

    // 等待模拟定时器推进 (500ms per substep)
    // 初始化有5个子步骤, 需要约 5×500ms = 2.5s
    QTest::qWait(3000);

    // 初始化应该完成, 正在执行上料(步骤1)
    QCOMPARE(pm.stepState(0), ProcessManager::COMPLETED);
    QCOMPARE(pm.currentStep(), 1);
    QCOMPARE(pm.stepState(1), ProcessManager::RUNNING);
}

void TestProcessManager::testLoopBehavior()
{
    ProcessManager pm;
    pm.startCycle();

    // 等待完成初始化 + 7个循环步骤
    // 每个步骤约0.5n秒 (n=子步骤数), 总计约 15~20秒
    QTest::qWait(20000);

    // 至少完成了一个循环 (cycleCount >= 1)
    QVERIFY(pm.cycleCount() >= 1);
    // 当前应该在上料(步骤1) — 循环回来的
    QVERIFY(pm.currentStep() == 1 || pm.currentStep() == 2);
}

void TestProcessManager::testPauseAndResume()
{
    ProcessManager pm;
    pm.startCycle();

    // 等一小会
    QTest::qWait(800);

    // 暂停
    pm.pauseCycle();
    QCOMPARE(pm.isPaused(), true);

    // 记录当前步骤
    int stepBeforePause = pm.currentStep();

    // 等一会确保没有前进
    QTest::qWait(2000);
    QCOMPARE(pm.currentStep(), stepBeforePause);

    // 继续
    pm.resumeCycle();
    QCOMPARE(pm.isPaused(), false);
}

void TestProcessManager::testFinishCycle()
{
    ProcessManager pm;
    QSignalSpy spyAllFinished(&pm, &ProcessManager::allFinished);

    pm.startCycle();
    QTest::qWait(1000); // 至少过了初始化

    pm.finishCycle();

    // 等待收尾完成 (2个子步骤 ≈ 1s)
    QTest::qWait(2000);

    // allFinished 信号应该被触发
    QVERIFY(spyAllFinished.count() >= 1);
    QCOMPARE(pm.isRunning(), false);
}

void TestProcessManager::testEmergencyStop()
{
    ProcessManager pm;
    pm.startCycle();
    QTest::qWait(800);

    pm.emergencyStop();

    QCOMPARE(pm.isRunning(), false);
    QCOMPARE(pm.currentStep(), -1);

    // 所有步骤应回到 PENDING
    for (int i = 0; i < 9; ++i) {
        QCOMPARE(pm.stepState(i), ProcessManager::PENDING);
    }
}

void TestProcessManager::testStepDefinitions()
{
    ProcessManager pm;

    // 验证9步骤定义
    QVector<ProcessManager::StepDef> steps = pm.allSteps();
    QCOMPARE(steps.size(), 9);

    QCOMPARE(steps[0].name, QString("初始化"));
    QCOMPARE(steps[1].name, QString("上料"));
    QCOMPARE(steps[8].name, QString("收尾"));

    // 验证子步骤
    QVERIFY(steps[0].substeps.size() > 0);
    QVERIFY(steps[4].substeps.size() > 0);

    // 验证参数
    QVERIFY(steps[4].defaultParams.contains("拾取高度"));
}

QTEST_MAIN(TestProcessManager)
#include "test_process_manager.moc"
```

- [ ] **Step 3: 更新 tests/tests.pro 添加子项目**

在 `tests.pro` 的 `SUBDIRS` 中添加 `test_process_manager`, 并在文件末尾添加:

```qmake
test_process_manager.file = test_process_manager.pro
```

- [ ] **Step 4: 编译并运行测试**

```bash
cd d:/my_information/qtProject/chipSetter/tests
qmake tests.pro && make
./release/test_process_manager
```
Expected: 8 tests passing

- [ ] **Step 5: 提交**

```bash
git add tests/test_process_manager.cpp tests/test_process_manager.pro tests/tests.pro
git commit -m "test: add ProcessManager unit tests — state machine, loop, pause, finish, estop

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## 实现顺序总结

```
Task 1: ProcessManager.h      (接口定义)
Task 2: ProcessManager.cpp    (状态机实现)
Task 3: FlowStepBar.h         (接口定义)
Task 4: FlowStepBar.cpp       (UI实现)
Task 5: StepDetailPanel.h+cpp  (UI实现)
Task 6: DeviceStatusWidget.h+cpp (UI实现)
Task 7: ProductionWidget 重写  (组件组合)
Task 8: MainWindow 连线       (系统集成)
Task 9: 单元测试              (验证)
```
