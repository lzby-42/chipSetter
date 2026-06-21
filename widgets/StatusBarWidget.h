/**
 * @file StatusBarWidget.h
 * @brief 顶部状态栏 — 模式指示、运行状态、产量、时间
 *
 * 纯UI组件, 通过槽函数接收外部数据更新显示。
 * 不包含任何业务逻辑。
 */

#ifndef STATUSBARWIDGET_H
#define STATUSBARWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class StatusBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget *parent = nullptr);
    ~StatusBarWidget();

public slots:
    void setMode(int mode);                     // 0=手动, 1=自动
    void setRunStatus(bool running);            // 运行/停止
    void setCount(int count);                   // 产量
    void setRunningTime(double hours);          // 运行时长
    void setCycleTime(double seconds);          // 节拍
    void updateTimestamp();                     // 刷新时间戳

private:
    void setupUI();

    QLabel* m_modeLabel;        // "自动模式" / "手动模式"
    QLabel* m_statusLabel;      // "运行中" / "暂停"
    QLabel* m_countLabel;       // "产量: 1,248"
    QLabel* m_timeLabel;        // "时长: 03:52:17"
    QLabel* m_cycleLabel;       // "节拍: 2.8s"
    QLabel* m_timestampLabel;   // "2026-06-13 14:32:05"
};

#endif // STATUSBARWIDGET_H
