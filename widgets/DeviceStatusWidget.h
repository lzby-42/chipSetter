/**
 * @file DeviceStatusWidget.h
 * @brief 设备状态概览 — 13轴灯 + IO关键信号 + 通讯状态
 */

#ifndef DEVICESTATUSWIDGET_H
#define DEVICESTATUSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>

class DeviceStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceStatusWidget(QWidget *parent = nullptr);
    ~DeviceStatusWidget();

public slots:
    void updateAxisStates(const QVector<int>& enabled,
                          const QVector<int>& moving,
                          const QVector<int>& alarmed);
    void setEmStopStatus(bool triggered);
    void setConnectionStatus(bool connected);
    void updateActiveIoSignals(const QStringList& activeSignals);

private:
    void setupUI();

    QVector<QLabel*> m_axisLights;
    QVector<QLabel*> m_axisNames;
    QLabel* m_emStopLabel;
    QLabel* m_connLabel;
    QLabel* m_ioSummaryLabel;

    static const int AXIS_COUNT = 13;
};

#endif // DEVICESTATUSWIDGET_H
