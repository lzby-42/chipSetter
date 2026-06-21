/**
 * @file StatsWidget.cpp
 * @brief 生产统计面板 实现
 */

#include "StatsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

StatsWidget::StatsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

StatsWidget::~StatsWidget()
{
}

void StatsWidget::setupUI()
{
    setObjectName("StatsWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    QLabel* title = new QLabel("生产统计", this);
    title->setStyleSheet("font-weight:bold; color:#ffd740; font-size:13px;");
    mainLayout->addWidget(title);

    QHBoxLayout* cardLayout = new QHBoxLayout();
    cardLayout->setSpacing(10);

    // 总产量卡片
    QVBoxLayout* totalCard = new QVBoxLayout();
    totalCard->setSpacing(2);

    QLabel* totalTitle = new QLabel("总产量", this);
    totalTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    totalTitle->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(totalTitle);

    m_totalCountLabel = new QLabel("0", this);
    m_totalCountLabel->setStyleSheet("color:#ffd740; font-size:22px; font-weight:bold;");
    m_totalCountLabel->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(m_totalCountLabel);

    QLabel* totalUnit = new QLabel("件", this);
    totalUnit->setStyleSheet("color:#78909c; font-size:9px;");
    totalUnit->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(totalUnit);

    QWidget* totalCardWidget = new QWidget(this);
    totalCardWidget->setLayout(totalCard);
    totalCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(totalCardWidget);

    // 运行时长卡片
    QVBoxLayout* timeCard = new QVBoxLayout();
    timeCard->setSpacing(2);

    QLabel* timeTitle = new QLabel("运行时长", this);
    timeTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    timeTitle->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(timeTitle);

    m_runningTimeLabel = new QLabel("00:00", this);
    m_runningTimeLabel->setStyleSheet("color:#64b5f6; font-size:22px; font-weight:bold;");
    m_runningTimeLabel->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(m_runningTimeLabel);

    QLabel* timeUnit = new QLabel("时:分", this);
    timeUnit->setStyleSheet("color:#78909c; font-size:9px;");
    timeUnit->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(timeUnit);

    QWidget* timeCardWidget = new QWidget(this);
    timeCardWidget->setLayout(timeCard);
    timeCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(timeCardWidget);

    // 节拍卡片
    QVBoxLayout* cycleCard = new QVBoxLayout();
    cycleCard->setSpacing(2);

    QLabel* cycleTitle = new QLabel("平均节拍", this);
    cycleTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    cycleTitle->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(cycleTitle);

    m_cycleTimeLabel = new QLabel("0.0", this);
    m_cycleTimeLabel->setStyleSheet("color:#81c784; font-size:22px; font-weight:bold;");
    m_cycleTimeLabel->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(m_cycleTimeLabel);

    QLabel* cycleUnit = new QLabel("秒/件", this);
    cycleUnit->setStyleSheet("color:#78909c; font-size:9px;");
    cycleUnit->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(cycleUnit);

    QWidget* cycleCardWidget = new QWidget(this);
    cycleCardWidget->setLayout(cycleCard);
    cycleCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(cycleCardWidget);

    mainLayout->addLayout(cardLayout);
    mainLayout->addStretch();
}

void StatsWidget::onStatsUpdated(int totalCount, double runningHours, double cycleTimeSec)
{
    m_totalCountLabel->setText(QString::number(totalCount));

    int hours   = static_cast<int>(runningHours);
    int minutes = static_cast<int>((runningHours - hours) * 60);
    m_runningTimeLabel->setText(QString("%1:%2")
                                .arg(hours, 2, 10, QLatin1Char('0'))
                                .arg(minutes, 2, 10, QLatin1Char('0')));

    m_cycleTimeLabel->setText(QString::number(cycleTimeSec, 'f', 1));
}
