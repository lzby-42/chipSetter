/**
 * @file IoManager.cpp
 * @brief IoManager 实现
 */

#include "IoManager.h"
#include <QDebug>

IoManager::IoManager(IGncController* controller, QObject *parent)
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
    // 读取一次初始值
    onPollTimer();
    startPolling(50);
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

    short val = static_cast<short>(value);
    bool ok = m_controller->writeDO(GNC_CORE_NUM, MC_GPO,
                                    static_cast<short>(doIndex), &val, 1);
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
    if (!m_controller || !m_controller->isConnected()) return;

    // 读取所有DI
    short diValues[16];
    m_controller->readDI(GNC_CORE_NUM, MC_GPI, 1, diValues, DI_COUNT);

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
    for (int i = 0; i < DI_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_diSignals[i].value;
        if (newVal != m_lastDiValues[idx]) {
            m_lastDiValues[idx] = newVal;
            emit diChanged(idx, newVal);

            // 急停信号检测
            if (idx == DI_EMERGENCY_STOP) {
                emit emergencyStopChanged(newVal == 0); // 低电平 = 急停触发
            }
        }
    }

    for (int i = 0; i < DO_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_doSignals[i].value;
        if (newVal != m_lastDoValues[idx]) {
            m_lastDoValues[idx] = newVal;
            emit doChanged(idx, newVal);
        }
    }
}
