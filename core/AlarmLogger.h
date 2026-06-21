/**
 * @file AlarmLogger.h
 * @brief 报警管理器 — 报警触发、恢复、历史记录
 *
 * 职责:
 *   - 接收报警信号 (急停、限位、驱动错误等)
 *   - 维护报警列表 (活跃 + 历史)
 *   - 自动生成恢复记录
 *
 * 依赖: AlarmRecord.h, HardwareConfig.h
 */

#ifndef ALARMLOGGER_H
#define ALARMLOGGER_H

#include <QObject>
#include <QVector>
#include "models/AlarmRecord.h"

class AlarmLogger : public QObject
{
    Q_OBJECT

public:
    explicit AlarmLogger(QObject *parent = nullptr);
    ~AlarmLogger();

    // ---- 报警列表 ----
    QVector<AlarmRecord> activeAlarms() const;      // 当前活跃报警
    QVector<AlarmRecord> allRecords() const;         // 全部记录 (含历史)
    int activeCount() const;                         // 活跃报警数

    // ---- 查询 ----
    QVector<AlarmRecord> alarmsByLevel(AlarmLevel level) const;
    AlarmRecord recordById(int alarmId) const;

public slots:
    // ---- 接收报警信号 ----
    void raiseAlarm(AlarmLevel level, const QString& source, const QString& message);
    void resolveAlarm(int alarmId);
    void clearAll();                                // 清除所有报警

signals:
    void newAlarm(AlarmRecord record);              // 新报警 (通知UI)
    void alarmResolved(int alarmId);                // 报警已恢复
    void activeCountChanged(int count);             // 活跃数量变化

private:
    QVector<AlarmRecord> m_records;                 // 全部记录
    int m_nextId;
};

#endif // ALARMLOGGER_H
