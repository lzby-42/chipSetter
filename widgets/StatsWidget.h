/**
 * @file StatsWidget.h
 * @brief 生产统计面板 — 产量、运行时长、节拍
 */

#ifndef STATSWIDGET_H
#define STATSWIDGET_H

#include <QWidget>
#include <QLabel>

class StatsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatsWidget(QWidget *parent = nullptr);
    ~StatsWidget();

public slots:
    void onStatsUpdated(int totalCount, double runningHours, double cycleTimeSec);

private:
    void setupUI();

    QLabel* m_totalCountLabel;
    QLabel* m_runningTimeLabel;
    QLabel* m_cycleTimeLabel;
};

#endif // STATSWIDGET_H
