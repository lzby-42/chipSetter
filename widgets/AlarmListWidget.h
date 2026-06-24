/**
 * @file AlarmListWidget.h
 * @brief 报警列表 — 紧凑表格 (时间|级别|来源|消息)
 *
 * 级别颜色: 致命=红, 警告=橙, 信息=灰, 已恢复=整行灰
 */

#ifndef ALARMLISTWIDGET_H
#define ALARMLISTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include "models/AlarmRecord.h"

class AlarmListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmListWidget(QWidget *parent = nullptr);
    ~AlarmListWidget();

public slots:
    void onNewAlarm(AlarmRecord record);
    void onAlarmResolved(int alarmId);
    void onActiveCountChanged(int count);

private:
    void setupUI();
    void updateCountBadge();

    QTableWidget*   m_table;
    QLabel*         m_countLabel;
    int             m_activeCount;
};

#endif // ALARMLISTWIDGET_H
