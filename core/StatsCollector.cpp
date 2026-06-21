/**
 * @file StatsCollector.cpp
 * @brief StatsCollector 实现
 */

#include "StatsCollector.h"
#include <QDebug>

StatsCollector::StatsCollector(QObject *parent)
    : QObject(parent)
    , m_totalCount(0)
    , m_elapsedMs(0)
    , m_running(false)
{
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &StatsCollector::onTimerTick);
    m_updateTimer->start(1000);  // 每秒更新一次
}

StatsCollector::~StatsCollector()
{
    m_updateTimer->stop();
}

int StatsCollector::totalCount() const
{
    return m_totalCount;
}

double StatsCollector::runningHours() const
{
    return m_elapsedMs / 3600000.0;
}

double StatsCollector::cycleTimeSec() const
{
    if (m_totalCount == 0) return 0.0;
    return m_elapsedMs / 1000.0 / m_totalCount;
}

void StatsCollector::start()
{
    if (!m_running) {
        m_running   = true;
        m_startTime = QDateTime::currentDateTime();
    }
}

void StatsCollector::pause()
{
    if (m_running) {
        m_running    = false;
        m_elapsedMs += m_startTime.msecsTo(QDateTime::currentDateTime());
    }
}

void StatsCollector::reset()
{
    m_totalCount = 0;
    m_elapsedMs  = 0;
    m_running    = false;
    emit statsUpdated(m_totalCount, runningHours(), cycleTimeSec());
}

void StatsCollector::incrementCount()
{
    m_totalCount++;
    emit statsUpdated(m_totalCount, runningHours(), cycleTimeSec());
}

void StatsCollector::onTimerTick()
{
    if (m_running) {
        qint64 currentElapsed = m_elapsedMs + m_startTime.msecsTo(QDateTime::currentDateTime());
        double hours  = currentElapsed / 3600000.0;
        double cycle  = (m_totalCount > 0) ? (currentElapsed / 1000.0 / m_totalCount) : 0.0;
        emit statsUpdated(m_totalCount, hours, cycle);
    }
}
