/**
 * @file mainwindow.h
 * @brief 主窗口 — 双模式: 生产界面 + 调试界面
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

// Core
#include "core/GncController.h"
#include "core/MotorManager.h"
#include "core/IoManager.h"
#include "core/AlarmLogger.h"
#include "core/StatsCollector.h"
#include "core/ProcessManager.h"

// Widgets (调试模式)
#include "widgets/StatusBarWidget.h"
#include "widgets/MotorPtpWidget.h"
#include "widgets/MotorParamWidget.h"
#include "widgets/IoMonitorWidget.h"
#include "widgets/AlarmListWidget.h"
#include "widgets/StatsWidget.h"
#include "widgets/BottomBarWidget.h"

// Widgets (生产模式)
#include "widgets/ProductionWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void createCoreModules();
    void createWidgets();
    void setupLayout();
    void connectSignals();
    void initSystem();

    void switchToProductionMode();
    void switchToDebugMode();

    // Core
    MockGncController*  m_gncController;
    MotorManager*       m_motorManager;
    IoManager*          m_ioManager;
    AlarmLogger*        m_alarmLogger;
    StatsCollector*     m_statsCollector;
    ProcessManager*     m_processManager;

    // Layout
    QStackedWidget*     m_stack;

    // 生产模式
    ProductionWidget*   m_production;

    // 调试模式面板
    StatusBarWidget*    m_statusBar;
    MotorPtpWidget*     m_motorPtp;
    MotorParamWidget*   m_motorParam;
    IoMonitorWidget*    m_ioMonitor;
    AlarmListWidget*    m_alarmList;
    StatsWidget*        m_stats;
    BottomBarWidget*    m_bottomBar;
    QWidget*            m_placeholderVision;
    QWidget*            m_debugPage;

    int m_currentMode;  // 0=手动, 1=自动
};

#endif // MAINWINDOW_H
