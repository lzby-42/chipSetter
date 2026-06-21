/**
 * @file AlarmListWidget.h
 * @brief 报警列表面板 — 实时报警滚动列表
 *
 * 职责:
 *   - 显示活跃报警 (红色=致命, 橙色=警告, 灰色=已恢复)
 *   - 可滚动查看历史
 *   - 显示报警总数
 */

#ifndef ALARMLISTWIDGET_H
#define ALARMLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
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

    QListWidget*  m_listWidget;
    QLabel*       m_countLabel;
    int           m_activeCount;
};

#endif // ALARMLISTWIDGET_H
