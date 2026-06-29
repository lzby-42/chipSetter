/**
 * @file ProductionWidget.h
 * @brief 生产运行界面 — 工艺流程UI (操作员视角)
 */

#ifndef PRODUCTIONWIDGET_H
#define PRODUCTIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>

class FlowStepBar;
class StepDetailPanel;
class DeviceStatusWidget;

class ProductionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProductionWidget(QWidget *parent = nullptr);
    ~ProductionWidget();

    // Sub-widget accessors (for MainWindow signal wiring)
    FlowStepBar*        flowStepBar() const;
    StepDetailPanel*    stepDetailPanel() const;
    DeviceStatusWidget* deviceStatusWidget() const;

public slots:
    // Stats (backward compatible)
    void setTotalCount(int count);
    void setCycleTime(double seconds);
    void setRunningTime(double hours);

    // Status (backward compatible)
    void setRunStatus(bool running);
    void setMode(int mode);
    void setConnectionStatus(bool connected);

    // Axis states (backward compatible)
    void updateAxisStates(const QVector<int>& enabledAxes,
                          const QVector<int>& movingAxes,
                          const QVector<int>& alarmedAxes);

    // Alarms (backward compatible)
    void onNewAlarm(const QString& level, const QString& source, const QString& msg);
    void onActiveCountChanged(int count);
    void onClearAlarms();

signals:
    void switchToDebugMode();
    void alarmsCleared();

private:
    void setupUI();

    // Sub-widgets
    FlowStepBar*        m_flowStepBar;
    StepDetailPanel*    m_stepDetailPanel;
    DeviceStatusWidget* m_deviceStatus;

    // Status labels
    QLabel* m_modeLabel;
    QLabel* m_countLabel;
    QLabel* m_cycleLabel;
    QLabel* m_runtimeLabel;

    // Alarm area
    QLabel* m_alarmBadge;
    QLabel* m_alarmList;
    int     m_alarmCount;

    // Run status
    bool m_running;
};

#endif // PRODUCTIONWIDGET_H
