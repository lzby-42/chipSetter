/**
 * @file AlarmRecord.h
 * @brief 报警记录数据模型 — 单条报警的完整信息
 */

#ifndef ALARMRECORD_H
#define ALARMRECORD_H

#include <QString>
#include <QDateTime>

/**
 * @enum AlarmLevel
 * @brief 报警级别
 */
enum AlarmLevel {
    ALARM_LEVEL_FATAL   = 0,    // 致命 (红色) — 急停类
    ALARM_LEVEL_WARNING = 1,    // 警告 (橙色) — 参数异常
    ALARM_LEVEL_INFO    = 2     // 信息 (灰色) — 历史恢复记录
};

/**
 * @struct AlarmRecord
 * @brief 单条报警记录
 */
struct AlarmRecord {
    int         id;             // 报警ID (自增)
    QDateTime   timestamp;      // 触发时间
    AlarmLevel  level;          // 级别
    QString     source;         // 来源 ("晶盘X轴", "IO系统" 等)
    QString     message;        // 报警内容 ("驱动器报警", "回零超时" 等)
    bool        resolved;       // 是否已恢复

    AlarmRecord()
        : id(0)
        , level(ALARM_LEVEL_INFO)
        , resolved(false)
    {
        timestamp = QDateTime::currentDateTime();
    }
};

Q_DECLARE_METATYPE(AlarmRecord)

#endif // ALARMRECORD_H
