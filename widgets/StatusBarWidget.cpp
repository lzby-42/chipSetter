/**
 * @file StatusBarWidget.cpp
 * @brief 顶部状态栏实现
 */

#include "StatusBarWidget.h"
#include <QDateTime>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    updateTimestamp();
}

StatusBarWidget::~StatusBarWidget()
{
}

void StatusBarWidget::setupUI()
{
    setObjectName("StatusBarWidget");
    setFixedHeight(26);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 1, 8, 1);
    layout->setSpacing(10);

    // 应用标题
    QLabel* titleLabel = new QLabel("固晶机控制系统 V1.0", this);
    titleLabel->setStyleSheet("font-weight:bold; color:#00e676; font-size:11px;");
    layout->addWidget(titleLabel);

    layout->addStretch();

    // 模式标签 (可点击切换)
    m_modeBtn = new QPushButton("自动模式", this);
    m_modeBtn->setStyleSheet(
        "QPushButton { background:#00e676; color:#000000; padding:2px 12px; "
        "border:none; border-radius:3px; font-weight:bold; font-size:10px; }"
        "QPushButton:hover { background:#00c853; }");
    m_modeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_modeBtn, &QPushButton::clicked, this, &StatusBarWidget::modeClicked);
    layout->addWidget(m_modeBtn);

    // 运行状态
    m_statusLabel = new QLabel("运行中", this);
    m_statusLabel->setStyleSheet(
        "background:#1565c0; color:#ffffff; padding:2px 12px; "
        "border-radius:3px; font-size:10px;");
    layout->addWidget(m_statusLabel);

    // 产量
    m_countLabel = new QLabel("产量: 0", this);
    m_countLabel->setStyleSheet("color:#ffd740; font-size:10px;");
    layout->addWidget(m_countLabel);

    // 节拍
    m_cycleLabel = new QLabel("节拍: 0.0s", this);
    m_cycleLabel->setStyleSheet("color:#81c784; font-size:10px;");
    layout->addWidget(m_cycleLabel);

    // 运行时长
    m_timeLabel = new QLabel("时长: 00:00:00", this);
    m_timeLabel->setStyleSheet("color:#b0bec5; font-size:10px;");
    layout->addWidget(m_timeLabel);

    // 时间戳
    m_timestampLabel = new QLabel(this);
    m_timestampLabel->setStyleSheet("color:#78909c; font-size:10px;");
    layout->addWidget(m_timestampLabel);
}

void StatusBarWidget::setMode(int mode)
{
    if (mode == 0) {
        m_modeBtn->setText("手动模式");
        m_modeBtn->setStyleSheet(
            "QPushButton { background:#e65100; color:#ffffff; padding:2px 12px; "
            "border:none; border-radius:3px; font-weight:bold; font-size:10px; }"
            "QPushButton:hover { background:#ef6c00; }");
    } else {
        m_modeBtn->setText("自动模式");
        m_modeBtn->setStyleSheet(
            "QPushButton { background:#00e676; color:#000000; padding:2px 12px; "
            "border:none; border-radius:3px; font-weight:bold; font-size:10px; }"
            "QPushButton:hover { background:#00c853; }");
    }
}

void StatusBarWidget::setRunStatus(bool running)
{
    if (running) {
        m_statusLabel->setText("运行中");
        m_statusLabel->setStyleSheet(
            "background:#1565c0; color:#ffffff; padding:2px 12px; "
            "border-radius:3px; font-size:10px;");
    } else {
        m_statusLabel->setText("暂停");
        m_statusLabel->setStyleSheet(
            "background:#37474f; color:#90a4ae; padding:2px 12px; "
            "border-radius:3px; font-size:10px;");
    }
}

void StatusBarWidget::setCount(int count)
{
    m_countLabel->setText(QString("产量: %1").arg(count));
}

void StatusBarWidget::setRunningTime(double hours)
{
    int totalSec = static_cast<int>(hours * 3600);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    m_timeLabel->setText(QString("时长: %1:%2:%3")
                         .arg(h, 2, 10, QLatin1Char('0'))
                         .arg(m, 2, 10, QLatin1Char('0'))
                         .arg(s, 2, 10, QLatin1Char('0')));
}

void StatusBarWidget::setCycleTime(double seconds)
{
    m_cycleLabel->setText(QString("节拍: %1s").arg(seconds, 0, 'f', 1));
}

void StatusBarWidget::updateTimestamp()
{
    m_timestampLabel->setText(
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}
