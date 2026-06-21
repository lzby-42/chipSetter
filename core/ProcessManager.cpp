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
    , m_pendingFinish(false)
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
    for (const auto& s : m_substepStates[stepIndex])
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
        m_pendingFinish = false;
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
    } else {
        // Was paused at loop boundary — restart from loop start
        executeStep(LOOP_START);
    }
    qDebug() << "[ProcessManager] 继续循环";
}

void ProcessManager::finishCycle()
{
    m_paused = false;
    m_pendingFinish = false;
    // 跳过当前步骤, 直接进入收尾
    executeStep(STEP_FINISH);
    qDebug() << "[ProcessManager] 立刻收尾";
}

void ProcessManager::requestFinish()
{
    m_pendingFinish = true;
    qDebug() << "[ProcessManager] 出循环关机 — 当前循环完成后进入收尾";
}

bool ProcessManager::isPendingFinish() const
{
    return m_pendingFinish;
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

        // 收尾完成 → 全部重置, 下次从头开始
        for (int i = 0; i < STEP_COUNT; ++i) {
            m_stepStates[i] = PENDING;
            for (int j = 0; j < m_substepStates[i].size(); ++j)
                m_substepStates[i][j] = PENDING;
            emit stepStateChanged(i, static_cast<int>(PENDING));
        }

        emit allFinished();
        qDebug() << "[ProcessManager] 收尾完成, 全部重置";
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

        // 出循环关机 → 进收尾
        if (m_pendingFinish) {
            m_pendingFinish = false;
            executeStep(STEP_FINISH);
            qDebug() << "[ProcessManager] 出循环关机 → 进入收尾";
            return;
        }

        // 重置所有循环步骤+子步骤为待执行 (新循环开始)
        for (int i = LOOP_START; i <= LOOP_END; ++i) {
            m_stepStates[i] = PENDING;
            for (int j = 0; j < m_substepStates[i].size(); ++j)
                m_substepStates[i][j] = PENDING;
            emit stepStateChanged(i, static_cast<int>(PENDING));
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
        m_running = false;
        return;
    }

    executeStep(nextIdx);
}
