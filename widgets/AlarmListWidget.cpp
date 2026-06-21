/**
 * @file AlarmListWidget.cpp
 * @brief 报警列表面板 实现
 */

#include "AlarmListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>

AlarmListWidget::AlarmListWidget(QWidget *parent)
    : QWidget(parent)
    , m_activeCount(0)
{
    setupUI();
}

AlarmListWidget::~AlarmListWidget()
{
}

void AlarmListWidget::setupUI()
{
    setObjectName("AlarmListWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // ---- 标题行 (含数量badge) ----
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);

    QLabel* title = new QLabel("报警列表", this);
    title->setStyleSheet("font-weight:bold; color:#ef5350; font-size:13px;");
    titleRow->addWidget(title);

    m_countLabel = new QLabel("0", this);
    m_countLabel->setStyleSheet(
        "background:#b71c1c; color:#fff; padding:1px 8px; "
        "border-radius:8px; font-size:10px; font-weight:bold;");
    titleRow->addWidget(m_countLabel);

    titleRow->addStretch();
    mainLayout->addLayout(titleRow);

    // ---- 报警列表 ----
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget { background:#1e1e30; border:1px solid #333; border-radius:3px; }"
        "QListWidget::item { padding:3px 6px; border-bottom:1px solid #2a2a3e; }");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(m_listWidget);
}

void AlarmListWidget::onNewAlarm(AlarmRecord record)
{
    QString timeStr = record.timestamp.toString("hh:mm:ss");

    QString color, bgColor, icon;
    if (record.resolved) {
        color   = "#78909c";
        bgColor = "#1e1e30";
        icon    = "O";
    } else if (record.level == ALARM_LEVEL_FATAL) {
        color   = "#ef9a9a";
        bgColor = "#4e1a1a";
        icon    = "!";
    } else if (record.level == ALARM_LEVEL_WARNING) {
        color   = "#ffcc80";
        bgColor = "#4e342e";
        icon    = "?";
    } else {
        color   = "#78909c";
        bgColor = "#1e1e30";
        icon    = "O";
    }

    QString itemText = QString("%1 %2  %3")
        .arg(icon, timeStr, record.message);

    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, record.id);
    item->setForeground(QColor(color));
    item->setBackground(QColor(bgColor));

    // 插入到列表顶部
    m_listWidget->insertItem(0, item);

    // 限制显示条数 (最多保留200条)
    while (m_listWidget->count() > 200) {
        delete m_listWidget->takeItem(m_listWidget->count() - 1);
    }

    if (!record.resolved) {
        m_activeCount++;
        updateCountBadge();
    }
}

void AlarmListWidget::onAlarmResolved(int alarmId)
{
    // 在列表中标记该报警为已恢复
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == alarmId) {
            item->setForeground(QColor("#78909c"));
            item->setBackground(QColor("#1e1e30"));
            QString text = item->text();
            text.replace("!", "O");
            text.replace("?", "O");
            item->setText(text);
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
            "background:#37474f; color:#90a4ae; padding:1px 8px; "
            "border-radius:8px; font-size:10px; font-weight:bold;");
    } else {
        m_countLabel->setStyleSheet(
            "background:#b71c1c; color:#fff; padding:1px 8px; "
            "border-radius:8px; font-size:10px; font-weight:bold;");
    }
}
