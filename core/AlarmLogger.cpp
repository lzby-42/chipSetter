/**
 * @file AlarmLogger.cpp
 * @brief AlarmLogger 实现
 */

#include "AlarmLogger.h"
#include <QDebug>

AlarmLogger::AlarmLogger(QObject *parent)
    : QObject(parent)
    , m_nextId(1)
{
}

AlarmLogger::~AlarmLogger()
{
}

QVector<AlarmRecord> AlarmLogger::activeAlarms() const
{
    QVector<AlarmRecord> active;
    for (const AlarmRecord& rec : m_records) {
        if (!rec.resolved)
            active.append(rec);
    }
    return active;
}

QVector<AlarmRecord> AlarmLogger::allRecords() const
{
    return m_records;
}

int AlarmLogger::activeCount() const
{
    int cnt = 0;
    for (const AlarmRecord& rec : m_records) {
        if (!rec.resolved) ++cnt;
    }
    return cnt;
}

QVector<AlarmRecord> AlarmLogger::alarmsByLevel(AlarmLevel level) const
{
    QVector<AlarmRecord> result;
    for (const AlarmRecord& rec : m_records) {
        if (rec.level == level)
            result.append(rec);
    }
    return result;
}

AlarmRecord AlarmLogger::recordById(int alarmId) const
{
    for (const AlarmRecord& rec : m_records) {
        if (rec.id == alarmId)
            return rec;
    }
    return AlarmRecord();
}

void AlarmLogger::raiseAlarm(AlarmLevel level, const QString& source, const QString& message)
{
    AlarmRecord rec;
    rec.id        = m_nextId++;
    rec.timestamp = QDateTime::currentDateTime();
    rec.level     = level;
    rec.source    = source;
    rec.message   = message;
    rec.resolved  = false;

    m_records.prepend(rec);     // 新报警放在最前面
    emit newAlarm(rec);
    emit activeCountChanged(activeCount());

    qDebug() << "[AlarmLogger] 新报警 #" << rec.id
             << "[" << source << "] " << message;
}

void AlarmLogger::resolveAlarm(int alarmId)
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].id == alarmId && !m_records[i].resolved) {
            m_records[i].resolved = true;

            // 自动生成一条"已恢复"的记录
            AlarmRecord resolvedRec;
            resolvedRec.id        = m_nextId++;
            resolvedRec.timestamp = QDateTime::currentDateTime();
            resolvedRec.level     = ALARM_LEVEL_INFO;
            resolvedRec.source    = m_records[i].source;
            resolvedRec.message   = m_records[i].message + " [已恢复]";
            resolvedRec.resolved  = true;
            m_records.prepend(resolvedRec);

            emit alarmResolved(alarmId);
            emit activeCountChanged(activeCount());

            qDebug() << "[AlarmLogger] 报警 #" << alarmId << " 已恢复";
            break;
        }
    }
}

void AlarmLogger::clearAll()
{
    // 将所有活跃报警标记为恢复
    for (AlarmRecord& rec : m_records) {
        if (!rec.resolved) {
            rec.resolved = true;
        }
    }
    emit activeCountChanged(0);
}
