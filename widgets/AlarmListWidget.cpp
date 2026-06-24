/**
 * @file AlarmListWidget.cpp
 * @brief 报警列表面板 — 每条独立显示/自动折行/彩色级别标志
 */

#include "AlarmListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPushButton>
#include <QVariant>

AlarmListWidget::AlarmListWidget(QWidget *parent)
    : QWidget(parent)
    , m_alarmContainer(nullptr)
    , m_activeCount(0)
{
    setupUI();
}

AlarmListWidget::~AlarmListWidget() {}

void AlarmListWidget::setupUI()
{
    setObjectName("AlarmListWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- 标题行 ----
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);

    QLabel* title = new QLabel("报警列表", this);
    title->setStyleSheet("font-weight:bold; color:#ef5350; font-size:12px;");
    titleRow->addWidget(title);

    m_countLabel = new QLabel("0", this);
    m_countLabel->setFixedSize(22, 18);
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setStyleSheet(
        "background:#37474f; color:#90a4ae; "
        "border-radius:9px; font-size:10px; font-weight:bold;");
    titleRow->addWidget(m_countLabel);
    titleRow->addStretch();
    mainLayout->addLayout(titleRow);

    // ---- 报警滚动区 ----
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { background:#1e1e30; border:1px solid #333; border-radius:3px; }");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_alarmContainer = new QWidget();
    m_alarmContainer->setStyleSheet("background:#1e1e30;");
    m_alarmLayout = new QVBoxLayout(m_alarmContainer);
    m_alarmLayout->setContentsMargins(2, 2, 2, 2);
    m_alarmLayout->setSpacing(1);
    m_alarmLayout->addStretch();

    scroll->setWidget(m_alarmContainer);
    mainLayout->addWidget(scroll, 1);
}

void AlarmListWidget::onNewAlarm(AlarmRecord record)
{
    QString timeStr = record.timestamp.toString("hh:mm:ss");

    // 级别标志: 彩色圆点 + 文字
    QString badge;
    QString badgeColor;
    QString borderColor;

    if (record.resolved) {
        badge = "已恢复";
        badgeColor = "#78909c";
        borderColor = "#444";
    } else if (record.level == ALARM_LEVEL_FATAL) {
        badge = "致命";
        badgeColor = "#fff";
        borderColor = "#f44336";
    } else if (record.level == ALARM_LEVEL_WARNING) {
        badge = "警告";
        badgeColor = "#fff";
        borderColor = "#ff9800";
    } else {
        badge = "信息";
        badgeColor = "#fff";
        borderColor = "#607d8b";
    }

    // 颜色点
    QString dotColor;
    if (record.resolved)        dotColor = "#78909c";
    else if (record.level == ALARM_LEVEL_FATAL)  dotColor = "#f44336";
    else if (record.level == ALARM_LEVEL_WARNING) dotColor = "#ff9800";
    else                        dotColor = "#607d8b";

    // 每条报警一个 Frame
    QFrame* frame = new QFrame(m_alarmContainer);
    frame->setStyleSheet(
        QString("QFrame { background:#1a1a2e; border-left:3px solid %1; "
                "border-radius:2px; margin:0px; }").arg(borderColor));
    frame->setProperty("alarmId", QVariant(record.id));
    frame->setProperty("resolved", QVariant(record.resolved));

    QVBoxLayout* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(4, 3, 4, 3);
    frameLayout->setSpacing(1);

    // 第一行: ● 级别标志 + 时间
    QHBoxLayout* headerRow = new QHBoxLayout();
    headerRow->setSpacing(6);

    QLabel* dot = new QLabel(QString::fromUtf8("●"), frame);
    dot->setStyleSheet(QString("color:%1; font-size:10px;").arg(dotColor));
    headerRow->addWidget(dot);

    QLabel* badgeLabel = new QLabel(badge, frame);
    badgeLabel->setStyleSheet(
        QString("color:%1; font-weight:bold; font-size:9px;").arg(badgeColor));
    headerRow->addWidget(badgeLabel);

    headerRow->addStretch();

    QLabel* timeLabel = new QLabel(timeStr, frame);
    timeLabel->setStyleSheet("color:#666; font-size:8px;");
    headerRow->addWidget(timeLabel);

    frameLayout->addLayout(headerRow);

    // 第二行: 报警消息 (自动折行)
    QLabel* msgLabel = new QLabel(record.message, frame);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet(
        QString("color:%1; font-size:9px; padding-left:4px;").arg(
            record.resolved ? "#78909c" : "#ccc"));
    frameLayout->addWidget(msgLabel);

    // 插入到顶部
    m_alarmLayout->insertWidget(0, frame);
    m_alarmFrames.prepend(frame);

    // 限制最多 200 条
    while (m_alarmFrames.size() > 200) {
        QFrame* last = m_alarmFrames.takeLast();
        m_alarmLayout->removeWidget(last);
        delete last;
    }

    if (!record.resolved) {
        m_activeCount++;
        updateCountBadge();
    }
}

void AlarmListWidget::onAlarmResolved(int alarmId)
{
    for (QFrame* frame : m_alarmFrames) {
        if (frame->property("alarmId").toInt() == alarmId) {
            frame->setProperty("resolved", true);
            frame->setStyleSheet(
                "QFrame { background:#1a1a2e; border-left:3px solid #444; "
                "border-radius:2px; }");

            // 更新子控件的颜色
            QList<QLabel*> labels = frame->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                label->setStyleSheet(label->styleSheet().replace(
                    QString::fromUtf8("#f44336"), "#78909c").replace(
                    QString::fromUtf8("#ff9800"), "#78909c").replace(
                    QString::fromUtf8("#fff"), "#78909c"));
            }
            break;
        }
    }
    if (m_activeCount > 0) {
        m_activeCount--;
        updateCountBadge();
    }
}

void AlarmListWidget::onActiveCountChanged(int count)
{
    m_activeCount = count;
    updateCountBadge();
}

void AlarmListWidget::updateCountBadge()
{
    m_countLabel->setText(QString::number(m_activeCount));
    if (m_activeCount == 0) {
        m_countLabel->setStyleSheet(
            "background:#37474f; color:#90a4ae; "
            "border-radius:9px; font-size:10px; font-weight:bold;");
    } else {
        m_countLabel->setStyleSheet(
            "background:#b71c1c; color:#fff; "
            "border-radius:9px; font-size:10px; font-weight:bold;");
    }
}
