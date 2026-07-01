/**
 * @file IoManager.cpp
 * @brief IoManager 实现
 */

#include "IoManager.h"
#include <QDebug>

IoManager::IoManager(GncController* controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    // 初始化DI信号列表
    QStringList diNames = DI_NAMES;
    for (int i = 0; i < DI_COUNT; ++i) {
        IoSignal sig;
        sig.id   = i + 1;
        sig.type = IO_TYPE_DI;
        sig.name = (i < diNames.size()) ? diNames[i] : QString("DI_%1").arg(i + 1);
        sig.value = 0;
        m_diSignals.append(sig);
    }

    // 初始化DO信号列表
    QStringList doNames = DO_NAMES;
    for (int i = 0; i < DO_COUNT; ++i) {
        IoSignal sig;
        sig.id   = i + 1;
        sig.type = IO_TYPE_DO;
        sig.name = (i < doNames.size()) ? doNames[i] : QString("DO_%1").arg(i + 1);
        sig.value = 0;
        m_doSignals.append(sig);
    }

    m_lastDiValues.resize(DI_COUNT + 1);
    m_lastDoValues.resize(DO_COUNT + 1);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &IoManager::onPollTimer);
}

IoManager::~IoManager()
{
    stopPolling();
}

void IoManager::initialize()
{
    qDebug() << "[IoManager] initialize() — 开始IO轮询, DI_COUNT=" << DI_COUNT
             << "DO_COUNT=" << DO_COUNT << "DO_INDEX_BASE=" << DO_INDEX_BASE;
    qDebug() << "[IoManager] 控制器连接状态:" << (m_controller ? (m_controller->isConnected() ? "已连接" : "未连接") : "NULL");
    // 读取一次初始值
    onPollTimer();
    startPolling(50);
    qDebug() << "[IoManager] 轮询定时器已启动, 间隔50ms";
}

const IoSignal& IoManager::diState(int index) const
{
    if (index >= 1 && index <= DI_COUNT) return m_diSignals[index - 1];
    static IoSignal dummy;
    return dummy;
}

const IoSignal& IoManager::doState(int index) const
{
    if (index >= 1 && index <= DO_COUNT) return m_doSignals[index - 1];
    static IoSignal dummy;
    return dummy;
}

QVector<IoSignal> IoManager::allDiStates() const { return m_diSignals; }

QVector<IoSignal> IoManager::allDoStates() const { return m_doSignals; }

bool IoManager::setDO(int doIndex, int value)
{
    if (doIndex < 1 || doIndex > DO_COUNT) return false;
    if (!m_controller || !m_controller->isConnected()) return false;

    // 硬件逻辑翻转: 1=关, 0=开 (GPO输出低电平有效)
    short val = static_cast<short>(value ? 0 : 1);
    // GTN_WriteDigitalOutputBit 1-indexed: 逻辑1-4 → doIndex 10-13 → Y端子Y9-Y12
    short physIndex = static_cast<short>(doIndex + DO_INDEX_BASE - 1);
    bool ok = m_controller->writeDO(GNC_CORE_NUM, MC_GPO,
                                    physIndex, &val, 1);
    if (ok) {
        m_doSignals[doIndex - 1].value = value;
        emit doChanged(doIndex, value);
    }
    return ok;
}

void IoManager::startPolling(int intervalMs)
{
    m_pollTimer->start(intervalMs);
}

void IoManager::stopPolling()
{
    m_pollTimer->stop();
}

void IoManager::onPollTimer()
{
    if (!m_controller || !m_controller->isConnected()) {
        static int disconnectedLogCount = 0;
        if (++disconnectedLogCount <= 3)
            qDebug() << "[IoManager] onPollTimer — 跳过(控制器未连接)";
        return;
    }

    // 读取所有DI (19路 GPI1-19 / X0-X18)
    short diValues[DI_COUNT];
    bool ok = m_controller->readDI(GNC_CORE_NUM, MC_GPI, 1, diValues, DI_COUNT);

    static int pollCount = 0;
    static int diFailCount = 0;
    pollCount++;
    if (!ok) {
        diFailCount++;
        if (diFailCount == 1 || diFailCount % 50 == 0) {
            emit m_controller->hardwareError("IO轮询",
                QString("GT_GetDi读取失败(第%1次), IO数据可能过期").arg(diFailCount));
        }
        return;  // 读取失败时不更新数据, 避免用过期值覆盖
    } else {
        diFailCount = 0;
    }

    if (pollCount <= 3) {
        QString diStr;
        for (int i = 0; i < DI_COUNT; ++i) diStr += QString::number(diValues[i]);
        qDebug() << "[IoManager] 第" << pollCount << "次轮询 readDI ok=" << ok
                 << " values=[" << diStr << "]";
    }

    for (int i = 0; i < DI_COUNT; ++i) {
        m_diSignals[i].value = diValues[i];
    }

    // 读取所有DO (通过读回功能, 或直接用缓存 — Mock用缓存)
    // 实际GNC SDK: 用 readDI(MC_GPO) 读回输出状态
    // 这里暂用内部缓存

    detectChanges();
}

void IoManager::detectChanges()
{
    static int detectCount = 0;
    detectCount++;

    int diChanges = 0;
    for (int i = 0; i < DI_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_diSignals[i].value;
        if (newVal != m_lastDiValues[idx]) {
            m_lastDiValues[idx] = newVal;
            emit diChanged(idx, newVal);
            diChanges++;
            if (detectCount <= 3)
                qDebug() << "[IoManager] DI变化: id=" << idx
                         << " name=" << m_diSignals[i].name
                         << " val=" << newVal;

            // 急停信号检测 (仅当 DI_EMERGENCY_STOP > 0 时启用)
#if DI_EMERGENCY_STOP > 0
            if (idx == DI_EMERGENCY_STOP) {
                emit emergencyStopChanged(newVal == 0); // 低电平 = 急停触发
            }
#endif
        }
    }
    if (detectCount <= 3)
        qDebug() << "[IoManager] detectChanges #" << detectCount
                 << " DI变化数=" << diChanges;

    for (int i = 0; i < DO_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_doSignals[i].value;
        if (newVal != m_lastDoValues[idx]) {
            m_lastDoValues[idx] = newVal;
            emit doChanged(idx, newVal);
        }
    }
}
