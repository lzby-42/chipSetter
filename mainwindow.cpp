/**
 * @file mainwindow.cpp
 * @brief MainWindow — 生产/调试双模式
 */

#include "mainwindow.h"
#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QDebug>

#include "widgets/FlowStepBar.h"
#include "widgets/StepDetailPanel.h"
#include "widgets/DeviceStatusWidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentMode(0)
{
    createCoreModules();
    createWidgets();
    setupLayout();
    connectSignals();
    initSystem();
    switchToProductionMode();  // 默认启动到生产界面
}

MainWindow::~MainWindow() {}

// ============================================================
// 创建Core层
// ============================================================

void MainWindow::createCoreModules()
{
    m_gncController  = new GncController(this);
    m_motorManager   = new MotorManager(m_gncController, this);
    m_ioManager      = new IoManager(m_gncController, this);
    m_alarmLogger    = new AlarmLogger(this);
    m_statsCollector = new StatsCollector(this);
    m_processManager = new ProcessManager(this);
    m_dispensingPlatform = new DispensingPlatformController(m_motorManager, m_alarmLogger, this);
    m_pickupPlatform     = new PickupPlatformController(m_motorManager, m_alarmLogger, this);
}

// ============================================================
// 创建UI面板
// ============================================================

void MainWindow::createWidgets()
{
    // 生产界面
    m_production = new ProductionWidget(this);

    // 调试界面
    m_statusBar = new StatusBarWidget(this);
    m_motorPtp  = new MotorPtpWidget(this);
    m_motorParam = new MotorParamWidget(this);
    m_ioMonitor  = new IoMonitorWidget(this);
    m_stats      = new StatsWidget(this);
    m_alarmList  = new AlarmListWidget(this);

    m_placeholderVision = new QWidget(this);
    m_placeholderVision->setStyleSheet(
        "background:#1a1a2e; border:1px dashed #444; border-radius:4px;");
    QLabel* visLabel = new QLabel("视觉对位预览\n(预留区域)", m_placeholderVision);
    visLabel->setAlignment(Qt::AlignCenter);
    visLabel->setStyleSheet("color:#555; font-size:12px; background:transparent;");

    m_bottomBar = new BottomBarWidget(this);
}

// ============================================================
// 布局
// ============================================================

void MainWindow::setupLayout()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // 状态栏 — 双模式共享, 放在 QStackedWidget 上方
    mainLayout->addWidget(m_statusBar);

    // QStackedWidget: 0=生产, 1=调试
    m_stack = new QStackedWidget(this);

    // 第0页: 生产界面
    m_stack->addWidget(m_production);

    // 第1页: 调试界面
    m_debugPage = new QWidget(this);
    QVBoxLayout* debugLayout = new QVBoxLayout(m_debugPage);
    debugLayout->setContentsMargins(0, 0, 0, 0);
    debugLayout->setSpacing(2);

    // 调试工具栏
    QHBoxLayout* debugToolbar = new QHBoxLayout();
    debugToolbar->setSpacing(4);

    auto makeToolBtn = [this](const QString& text, const QString& style) {
        QPushButton* btn = new QPushButton(text, this);
        btn->setFixedSize(80, 24);
        btn->setStyleSheet(style);
        return btn;
    };

    QPushButton* enableAllBtn = makeToolBtn("全部使能",
        "QPushButton { background:#1565c0; color:#fff; border:none; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#1976d2; }");
    debugToolbar->addWidget(enableAllBtn);

    QPushButton* clearAlarmBtn = makeToolBtn("清除报警",
        "QPushButton { background:#333; color:#ccc; border:1px solid #555; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#444; }");
    debugToolbar->addWidget(clearAlarmBtn);

    // 连接调试工具按钮
    connect(enableAllBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 1; i <= AXIS_COUNT; ++i)
            m_motorManager->enableAxis(i);
    });
    connect(clearAlarmBtn, &QPushButton::clicked, this, [this]() {
        m_alarmLogger->clearAll();
    });

    debugToolbar->addStretch();
    debugLayout->addLayout(debugToolbar);

    QHBoxLayout* middleRow = new QHBoxLayout();
    middleRow->setSpacing(3);

    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(3);
    leftCol->addWidget(m_motorPtp, 4);
    leftCol->addWidget(m_motorParam, 5);
    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftCol);
    leftWidget->setFixedWidth(420);
    middleRow->addWidget(leftWidget);

    QVBoxLayout* centerCol = new QVBoxLayout();
    centerCol->setSpacing(3);
    centerCol->addWidget(m_ioMonitor, 5);
    centerCol->addWidget(m_stats, 3);
    QWidget* centerWidget = new QWidget(this);
    centerWidget->setLayout(centerCol);
    middleRow->addWidget(centerWidget, 1);

    QVBoxLayout* rightCol = new QVBoxLayout();
    rightCol->setSpacing(3);
    rightCol->addWidget(m_alarmList, 4);
    rightCol->addWidget(m_placeholderVision, 3);
    QWidget* rightWidget = new QWidget(this);
    rightWidget->setLayout(rightCol);
    rightWidget->setFixedWidth(220);
    middleRow->addWidget(rightWidget);

    debugLayout->addLayout(middleRow, 1);
    m_stack->addWidget(m_debugPage);

    mainLayout->addWidget(m_stack, 1);
    mainLayout->addWidget(m_bottomBar);
}

// ============================================================
// 信号/槽连线
// ============================================================

void MainWindow::connectSignals()
{
    // ===== 生产界面信号 =====
    connect(m_production, &ProductionWidget::switchToDebugMode,
            this, &MainWindow::switchToDebugMode);

    connect(m_statsCollector, &StatsCollector::statsUpdated,
            m_production, [this](int count, double hours, double cycle) {
                m_production->setTotalCount(count);
                m_production->setRunningTime(hours);
                m_production->setCycleTime(cycle);
            });

    connect(m_alarmLogger, &AlarmLogger::newAlarm,
            m_production, [this](AlarmRecord rec) {
                QString level = (rec.level == ALARM_LEVEL_FATAL) ? "FATAL" :
                                (rec.level == ALARM_LEVEL_WARNING) ? "WARN" : "INFO";
                m_production->onNewAlarm(level, rec.source, rec.message);
            });

    connect(m_alarmLogger, &AlarmLogger::activeCountChanged,
            m_production, &ProductionWidget::onActiveCountChanged);

    connect(m_production, &ProductionWidget::alarmsCleared,
            m_alarmLogger, &AlarmLogger::clearAll);

    // 定期更新轴状态到生产界面
    QTimer* axisUpdateTimer = new QTimer(this);
    connect(axisUpdateTimer, &QTimer::timeout, this, [this]() {
        QVector<int> enabled, moving, alarmed;
        for (int i = 1; i <= AXIS_COUNT; ++i) {
            const MotorAxis& ax = m_motorManager->axisState(i);
            if (ax.isEnabled) enabled.append(i);
            if (ax.isMoving)  moving.append(i);
            if (ax.isAlarm)   alarmed.append(i);
        }
        m_production->updateAxisStates(enabled, moving, alarmed);
    });
    axisUpdateTimer->start(500);

    // ===== 工艺流程信号 (ProcessManager ↔ Flow UI) =====

    // Step block clicked → show detail in StepDetailPanel
    connect(m_production->flowStepBar(), &FlowStepBar::stepClicked,
            this, [this](int stepIndex) {
        const ProcessManager::StepDef& def = m_processManager->stepDef(stepIndex);
        m_production->stepDetailPanel()->showStepDetail(
            stepIndex,
            def.name,
            def.substeps,
            m_processManager->substepStates(stepIndex),
            m_processManager->stepParams(stepIndex),
            m_processManager->realtimeData(stepIndex));
    });

    // ProcessManager step state → FlowStepBar
    connect(m_processManager, &ProcessManager::stepStateChanged,
            m_production->flowStepBar(), &FlowStepBar::setStepState);

    // ProcessManager current step → FlowStepBar
    connect(m_processManager, &ProcessManager::currentStepChanged,
            m_production->flowStepBar(), &FlowStepBar::setCurrentStep);

    // ProcessManager substep changes → StepDetailPanel
    connect(m_processManager, &ProcessManager::substepStateChanged,
            m_production->stepDetailPanel(), &StepDetailPanel::updateSubstepState);

    // ProcessManager realtime data → StepDetailPanel
    connect(m_processManager, &ProcessManager::realtimeDataUpdated,
            this, [this](int stepIndex, const QVariantMap& data) {
        m_production->stepDetailPanel()->updateRealtimeData(data);
        Q_UNUSED(stepIndex);
    });

    // ProcessManager cycle completed → StatsCollector (使用权威值, 不自行累加)
    connect(m_processManager, &ProcessManager::cycleCompleted,
            m_statsCollector, &StatsCollector::setCount);

    // ===== 电机 (调试界面) =====
    connect(m_motorPtp, &MotorPtpWidget::moveRequested,
            m_motorManager, &MotorManager::moveRequest);
    connect(m_motorPtp, &MotorPtpWidget::stopRequested,
            m_motorManager, &MotorManager::stopMove);
    connect(m_motorPtp, &MotorPtpWidget::homeRequested,
            m_motorManager, &MotorManager::homeRequest);
    connect(m_motorPtp, &MotorPtpWidget::jogRequested,
            m_motorManager, &MotorManager::jogRequest);

    // 清报警 — 直接调 GncController
    connect(m_motorPtp, &MotorPtpWidget::clearAlarmRequested,
            this, [this](int axisId) {
        m_gncController->clearStatus(GNC_CORE_NUM, static_cast<short>(axisId), 1);
    });

    // 使能/失能 — 调 MotorManager + 反馈按钮状态
    connect(m_motorPtp, &MotorPtpWidget::enableRequested,
            this, [this](int axisId, bool enable) {
        bool ok;
        if (enable) {
            ok = m_motorManager->enableAxis(axisId);
        } else {
            ok = m_motorManager->disableAxis(axisId);
        }
        // 按实际结果更新按钮
        m_motorPtp->onAxisEnableChanged(axisId, enable && ok);
    });

    connect(m_motorManager, &MotorManager::positionUpdated,
            m_motorPtp, &MotorPtpWidget::onPositionUpdated);
    connect(m_motorManager, &MotorManager::moveFinished,
            m_motorPtp, &MotorPtpWidget::onMoveFinished);
    connect(m_motorManager, &MotorManager::homeFinished,
            m_motorPtp, &MotorPtpWidget::onHomeFinished);

    // 应用 → 更新参数 + 自动保存
    connect(m_motorParam, &MotorParamWidget::applyRequested,
            this, [this](int axisId, const MotorAxis& params) {
        m_motorManager->updateAxisParams(axisId, params);
        m_motorManager->autoSave();
        // 刷新 MotorPtpWidget 的运动参数 spinbox
    });
    // 导出
    connect(m_motorParam, &MotorParamWidget::exportRequested,
            m_motorManager, &MotorManager::exportToFile);
    // 导入
    connect(m_motorParam, &MotorParamWidget::importRequested,
            m_motorManager, &MotorManager::importFromFile);
    // 参数更新后通知 UI (刷新spinbox)
    connect(m_motorManager, &MotorManager::axisParamChanged,
            m_motorParam, &MotorParamWidget::onParamsApplied);

    connect(m_motorPtp, &MotorPtpWidget::moveRequested,
            this, [this](int axisId, double, double, double, double) {
                m_motorParam->setCurrentAxisId(axisId);
            });

    // ===== IO =====
    connect(m_ioManager, &IoManager::diChanged,
            m_ioMonitor, &IoMonitorWidget::onDiChanged);
    connect(m_ioManager, &IoManager::doChanged,
            m_ioMonitor, &IoMonitorWidget::onDoChanged);

    // DI → 轴限位映射
    connect(m_ioManager, &IoManager::diChanged,
            this, [this](int diId, int value) {
        static const DiToAxisMapping map[] = DI_AXIS_MAP;
        static const int mapSize = sizeof(map) / sizeof(map[0]);
        for (int i = 0; i < mapSize; ++i) {
            if (map[i].diId == diId && map[i].axisId > 0) {
                int axisId = map[i].axisId;
                const MotorAxis& ax = m_motorManager->axisState(axisId);
                bool home = ax.homeSignal, limP = ax.limitPositiveSignal, limN = ax.limitNegativeSignal;
                switch (map[i].signalType) {
                case 0: home = (value == 1); break;
                case 1: limP = (value == 0); break;
                case 2: limN = (value == 0); break;
                }
                m_motorManager->updateAxisSignals(axisId, home, limP, limN);
                if ((map[i].signalType == 1 && limP) || (map[i].signalType == 2 && limN)) {
                    m_alarmLogger->raiseAlarm(ALARM_LEVEL_WARNING,
                        QString("轴%1").arg(axisId),
                        map[i].signalType == 1 ? "正限位触发" : "负限位触发");
                }
                break;
            }
        }
    });

    connect(m_ioManager, &IoManager::emergencyStopChanged,
            this, [this](bool triggered) {
                if (triggered) {
                    m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "IO系统", "急停按钮被按下");
                    for (int i = 1; i <= AXIS_COUNT; ++i)
                        m_motorManager->stopMove(i);
                }
            });

    // ===== GTS 硬件错误 → 报警栏 =====
    connect(m_gncController, &GncController::hardwareError,
            this, [this](const QString& source, const QString& message) {
                m_alarmLogger->raiseAlarm(ALARM_LEVEL_WARNING, source, message);
            });

    // ===== 报警 =====
    connect(m_alarmLogger, &AlarmLogger::newAlarm,
            m_alarmList, &AlarmListWidget::onNewAlarm);
    connect(m_alarmLogger, &AlarmLogger::alarmResolved,
            m_alarmList, &AlarmListWidget::onAlarmResolved);
    connect(m_alarmLogger, &AlarmLogger::activeCountChanged,
            m_alarmList, &AlarmListWidget::onActiveCountChanged);

    connect(m_motorManager, &MotorManager::motorError,
            this, [this](int axisId, const QString& msg) {
                m_alarmLogger->raiseAlarm(ALARM_LEVEL_WARNING,
                                          QString("轴%1").arg(axisId), msg);
            });

    // ===== 统计 =====
    connect(m_statsCollector, &StatsCollector::statsUpdated,
            m_stats, &StatsWidget::onStatsUpdated);
    connect(m_statsCollector, &StatsCollector::statsUpdated,
            this, [this](int count, double hours, double cycle) {
                m_statusBar->setCount(count);
                m_statusBar->setRunningTime(hours);
                m_statusBar->setCycleTime(cycle);
            });

    // ===== 底部操作栏 =====
    // 启动
    connect(m_bottomBar, &BottomBarWidget::startClicked,
            this, [this]() {
        m_statusBar->setRunStatus(true);
        m_production->setRunStatus(true);
        m_statsCollector->start();
        m_processManager->startCycle();
    });

    // 暂停
    connect(m_bottomBar, &BottomBarWidget::pauseClicked,
            this, [this]() {
        m_statusBar->setRunStatus(false);
        m_production->setRunStatus(false);
        m_statsCollector->pause();
        m_processManager->pauseCycle();
    });

    // 急停
    connect(m_bottomBar, &BottomBarWidget::emergencyStopClicked,
            this, [this]() {
                m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "操作", "操作员触发急停");
                m_processManager->emergencyStop();
            });

    // 收尾 — 当前循环跑完后自动进收尾
    connect(m_bottomBar, &BottomBarWidget::softFinishClicked,
            this, [this]() {
                m_processManager->requestFinish();
            });

    // 立即收尾 — 立刻跳收尾步骤
    connect(m_bottomBar, &BottomBarWidget::immediateFinishClicked,
            this, [this]() {
                m_processManager->finishCycle();
            });

    // 复位 — 急停 + 清除报警 + 重置统计
    connect(m_bottomBar, &BottomBarWidget::resetClicked,
            this, [this]() {
        m_alarmLogger->clearAll();
        m_statsCollector->reset();
        m_production->onClearAlarms();
        m_processManager->emergencyStop();
    });

    // 模式切换 (操作栏按钮)
    connect(m_bottomBar, &BottomBarWidget::modeSwitchClicked,
            this, [this]() {
                if (m_stack->currentIndex() == 0)
                    switchToDebugMode();
                else
                    switchToProductionMode();
            });

    // 模式切换 (状态栏模式灯)
    connect(m_statusBar, &StatusBarWidget::modeClicked,
            this, [this]() {
                if (m_stack->currentIndex() == 0)
                    switchToDebugMode();
                else
                    switchToProductionMode();
            });

    // 步骤参数编辑 → 更新 ProcessManager
    connect(m_production->stepDetailPanel(), &StepDetailPanel::paramEdited,
            this, [this](int stepIndex, const QString& name, double value) {
        m_processManager->setStepParam(stepIndex, name, value);
    });
}

// ============================================================
// 生产/调试模式切换
// ============================================================

void MainWindow::switchToProductionMode()
{
    m_stack->setCurrentIndex(0);
    m_bottomBar->setVisible(true);
    m_statusBar->setMode(1);       // 自动模式
    m_bottomBar->setMode(1);
    m_production->setMode(1);
}

void MainWindow::switchToDebugMode()
{
    m_stack->setCurrentIndex(1);
    m_bottomBar->setVisible(true);
    m_statusBar->setMode(0);       // 手动模式
    m_bottomBar->setMode(0);
    m_production->setMode(0);
}

// ============================================================
// 初始化
// ============================================================

void MainWindow::initSystem()
{
    qDebug() << "[MainWindow] 初始化GTS硬件...";
    bool cardOk = m_gncController->openCard();
    if (cardOk) {
        m_gncController->netInit("", 120);
        m_gncController->loadConfig(GNC_CORE_NUM, "googol/core1_20260625.cfg");
        m_gncController->clearStatus(GNC_CORE_NUM, GNC_AXIS_START, AXIS_COUNT);
    } else {
        qCritical() << "[MainWindow] 开卡失败! 请检查硬件连接和驱动";
    }

    m_bottomBar->setConnectionStatus(m_gncController->isConnected());
    m_production->setConnectionStatus(m_gncController->isConnected());

    m_motorManager->initialize();
    m_motorManager->autoLoad();   // 加载上次保存的电机参数
    m_dispensingPlatform->initialize();
    m_pickupPlatform->initialize();
    m_processManager->setDispensingPlatform(m_dispensingPlatform);
    m_processManager->setPickupPlatform(m_pickupPlatform);
    m_ioManager->initialize();

    m_statusBar->setMode(m_currentMode);
    m_statusBar->setRunStatus(false);
    m_statusBar->setCount(0);
    m_statusBar->setRunningTime(0);
    m_statusBar->setCycleTime(0);

    m_production->setMode(m_currentMode);
    m_production->setRunStatus(false);
    m_production->setTotalCount(0);

    m_alarmLogger->raiseAlarm(ALARM_LEVEL_INFO, "系统", "系统启动完成");

    // 轴诊断: 打印所有轴的详细参数到日志 (排查编码器/步进配置)
    qDebug() << "========== AXIS DIAGNOSTICS ==========";
    for (int i = 1; i <= AXIS_COUNT; ++i) {
        qDebug().noquote() << m_motorManager->diagnoseAxis(i);
    }
    qDebug() << "======================================";

    // Initialize process flow bar with step names
    QStringList stepNames = {"初始化", "上料", "定位", "顶晶", "拾取",
                             "点胶", "贴装", "下料", "收尾"};
    m_production->flowStepBar()->setSteps(stepNames);
}
