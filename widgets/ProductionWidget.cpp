/**
 * @file ProductionWidget.cpp
 * @brief 生产运行界面 — 工艺流程UI实现
 */

#include "ProductionWidget.h"
#include "widgets/FlowStepBar.h"
#include "widgets/StepDetailPanel.h"
#include "widgets/DeviceStatusWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>

ProductionWidget::ProductionWidget(QWidget *parent)
    : QWidget(parent)
    , m_alarmCount(0)
    , m_running(false)
{
    setupUI();
}

ProductionWidget::~ProductionWidget() {}

FlowStepBar* ProductionWidget::flowStepBar() const
{ return m_flowStepBar; }

StepDetailPanel* ProductionWidget::stepDetailPanel() const
{ return m_stepDetailPanel; }

DeviceStatusWidget* ProductionWidget::deviceStatusWidget() const
{ return m_deviceStatus; }

// ============================================================
// UI Setup
// ============================================================

void ProductionWidget::setupUI()
{
    setObjectName("ProductionWidget");
    setStyleSheet("QWidget#ProductionWidget { background-color:#1a1a2e; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ====== Top bar: Title + Mode + Debug button ======
    QHBoxLayout* titleRow = new QHBoxLayout();

    QLabel* title = new QLabel("◆ 固晶机控制系统", this);
    title->setStyleSheet(
        "font-weight:bold; color:#00e676; font-size:14px; border:none;");
    titleRow->addWidget(title);
    titleRow->addStretch();

    m_modeLabel = new QLabel("手动模式", this);
    m_modeLabel->setStyleSheet(
        "background:#e65100; color:#fff; padding:3px 14px;"
        "border-radius:3px; font-weight:bold; font-size:11px;");
    titleRow->addWidget(m_modeLabel);

    QPushButton* dbgBtn = new QPushButton("调试", this);
    dbgBtn->setFixedSize(56, 26);
    dbgBtn->setStyleSheet(
        "QPushButton{background:#37474f;color:#90a4ae;"
        "border:1px solid #555;border-radius:3px;font-size:10px;}"
        "QPushButton:hover{background:#455a64;color:#fff;}");
    connect(dbgBtn, &QPushButton::clicked,
            this, &ProductionWidget::switchToDebugMode);
    titleRow->addWidget(dbgBtn);

    mainLayout->addLayout(titleRow);

    // ====== Body: Left (flow+detail+device) | Right (alarms) ======
    QHBoxLayout* bodyRow = new QHBoxLayout();
    bodyRow->setSpacing(6);

    // --- Left column ---
    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(6);

    // 2. Process Flow Bar
    QFrame* flowFrame = new QFrame(this);
    flowFrame->setStyleSheet("QFrame{background:#1e1e30; border-radius:8px;}");
    QVBoxLayout* flowFrameLayout = new QVBoxLayout(flowFrame);
    flowFrameLayout->setContentsMargins(10, 8, 10, 8);

    QLabel* flowTitle = new QLabel("工艺流程", this);
    flowTitle->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    flowFrameLayout->addWidget(flowTitle);

    m_flowStepBar = new FlowStepBar(this);
    QStringList stepNames = {"初始化", "上料", "定位", "顶晶", "拾取",
                             "点胶", "贴装", "下料", "收尾"};
    m_flowStepBar->setSteps(stepNames);
    flowFrameLayout->addWidget(m_flowStepBar);

    leftCol->addWidget(flowFrame);

    // 3. Step Detail Panel
    m_stepDetailPanel = new StepDetailPanel(this);
    leftCol->addWidget(m_stepDetailPanel, 1);

    // 4. Device Status
    m_deviceStatus = new DeviceStatusWidget(this);
    leftCol->addWidget(m_deviceStatus);

    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftCol);
    bodyRow->addWidget(leftWidget, 3);

    // --- Right column: Alarms ---
    QFrame* rightFrame = new QFrame(this);
    rightFrame->setStyleSheet(
        "QFrame{background:#1a1a2e; border:1px solid #333; border-radius:8px;}");
    QVBoxLayout* rightCol = new QVBoxLayout(rightFrame);
    rightCol->setContentsMargins(10, 8, 10, 8);
    rightCol->setSpacing(6);

    QHBoxLayout* alarmTitleRow = new QHBoxLayout();
    QLabel* alarmTitle = new QLabel("报警列表", this);
    alarmTitle->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    alarmTitleRow->addWidget(alarmTitle);

    m_alarmBadge = new QLabel("0", this);
    m_alarmBadge->setStyleSheet(
        "background:#37474f; color:#90a4ae; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");
    alarmTitleRow->addWidget(m_alarmBadge);
    alarmTitleRow->addStretch();
    rightCol->addLayout(alarmTitleRow);

    m_alarmList = new QLabel("暂无报警", this);
    m_alarmList->setStyleSheet(
        "color:#78909c; font-size:9px;"
        "background:#1e1e30; border-radius:4px; padding:8px;");
    m_alarmList->setWordWrap(true);
    m_alarmList->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    rightCol->addWidget(m_alarmList, 1);

    QPushButton* clearAlarmBtn = new QPushButton("清除报警", this);
    clearAlarmBtn->setFixedHeight(28);
    clearAlarmBtn->setStyleSheet(
        "QPushButton{background:#333;color:#ccc;border:none;"
        "border-radius:3px;font-size:10px;}"
        "QPushButton:hover{background:#444;}");
    connect(clearAlarmBtn, &QPushButton::clicked, this, [this]() {
        onClearAlarms();
        emit alarmsCleared();
    });
    rightCol->addWidget(clearAlarmBtn);

    bodyRow->addWidget(rightFrame, 1);

    mainLayout->addLayout(bodyRow, 1);

    // Stats labels (hidden — data goes to status bar)
    m_countLabel = new QLabel("0", this);
    m_countLabel->hide();
    m_cycleLabel = new QLabel("0.0", this);
    m_cycleLabel->hide();
    m_runtimeLabel = new QLabel("00:00:00", this);
    m_runtimeLabel->hide();
}

// ============================================================
// Slots (backward compatible)
// ============================================================

void ProductionWidget::setTotalCount(int count)
{
    m_countLabel->setText(QString::number(count));
}

void ProductionWidget::setCycleTime(double seconds)
{
    m_cycleLabel->setText(QString::number(seconds, 'f', 1));
}

void ProductionWidget::setRunningTime(double hours)
{
    int totalSec = static_cast<int>(hours * 3600);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    m_runtimeLabel->setText(
        QString("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0')));
}

void ProductionWidget::setRunStatus(bool running)
{
    m_running = running;
}

void ProductionWidget::setMode(int mode)
{
    if (mode == 0) {
        m_modeLabel->setText("手动模式");
        m_modeLabel->setStyleSheet(
            "background:#e65100; color:#fff; padding:3px 14px;"
            "border-radius:3px; font-weight:bold; font-size:11px;");
    } else {
        m_modeLabel->setText("自动模式");
        m_modeLabel->setStyleSheet(
            "background:#00e676; color:#000; padding:3px 14px;"
            "border-radius:3px; font-weight:bold; font-size:11px;");
    }
}

void ProductionWidget::setConnectionStatus(bool connected)
{
    m_deviceStatus->setConnectionStatus(connected);
}

void ProductionWidget::updateAxisStates(const QVector<int>& enabledAxes,
                                         const QVector<int>& movingAxes,
                                         const QVector<int>& alarmedAxes)
{
    m_deviceStatus->updateAxisStates(enabledAxes, movingAxes, alarmedAxes);
}

void ProductionWidget::onNewAlarm(const QString& level, const QString& msg)
{
    m_alarmCount++;
    m_alarmBadge->setText(QString::number(m_alarmCount));
    m_alarmBadge->setStyleSheet(
        "background:#b71c1c; color:#fff; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");

    QStringList lines = m_alarmList->text().split('\n', Qt::SkipEmptyParts);
    if (lines.size() == 1 && lines[0] == "暂无报警") lines.clear();

    QString icon = (level == "FATAL") ? "⚠" : "?";
    lines.prepend(QString("%1 %2").arg(icon, msg));
    if (lines.size() > 8) lines = lines.mid(0, 8);

    m_alarmList->setText(lines.join('\n'));
}

void ProductionWidget::onClearAlarms()
{
    m_alarmCount = 0;
    m_alarmBadge->setText("0");
    m_alarmBadge->setStyleSheet(
        "background:#37474f; color:#90a4ae; padding:2px 10px;"
        "border-radius:10px; font-size:12px; font-weight:bold;");
    m_alarmList->setText("暂无报警");
}
