/**
 * @file AlarmListWidget.cpp
 * @brief 报警列表 — 紧凑表格实现
 */

#include "AlarmListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

AlarmListWidget::AlarmListWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(nullptr)
    , m_activeCount(0)
{
    setupUI();
}

AlarmListWidget::~AlarmListWidget() {}

void AlarmListWidget::setupUI()
{
    setObjectName("AlarmListWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // ---- 标题行 ----
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);

    QLabel* title = new QLabel("报警列表", this);
    title->setStyleSheet("font-weight:bold; color:#ef5350; font-size:11px;");
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

    // ---- 表格 ----
    m_table = new QTableWidget(0, 4, this);
    m_table->setObjectName("AlarmTable");
    m_table->setHorizontalHeaderLabels({"时间", "级别", "来源", "消息"});
    m_table->setColumnWidth(0, 60);   // 时间
    m_table->setColumnWidth(1, 36);   // 级别
    m_table->setColumnWidth(2, 70);   // 来源

    // 表头样式
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { background:#2a2a3a; color:#78909c; "
        "border:none; border-bottom:1px solid #333; padding:2px 4px; font-size:8px; }");
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(true);
    m_table->setTextElideMode(Qt::ElideNone);
    m_table->setStyleSheet(
        "QTableWidget { background:#1a1a2e; border:1px solid #333; border-radius:2px; "
        "gridline-color:#222; font-size:9px; }"
        "QTableWidget::item { padding:0 3px; color:#ccc; }");

    mainLayout->addWidget(m_table, 1);
}

void AlarmListWidget::onNewAlarm(AlarmRecord record)
{
    QString timeStr = record.timestamp.toString("hh:mm:ss");

    // 级别文字和颜色
    QString levelText;
    QString levelColor;
    if (record.level == ALARM_LEVEL_FATAL) {
        levelText = "致命";    // 致命
        levelColor = "#f44336";
    } else if (record.level == ALARM_LEVEL_WARNING) {
        levelText = "警告";    // 警告
        levelColor = "#ff9800";
    } else {
        levelText = "信息";    // 信息
        levelColor = "#78909c";
    }

    // 已恢复的整行灰色
    QString rowColor = record.resolved ? "#666" : levelColor;

    // 插入到第 0 行 (最新在上)
    m_table->insertRow(0);

    QTableWidgetItem* timeItem  = new QTableWidgetItem(timeStr);
    QTableWidgetItem* levelItem = new QTableWidgetItem(levelText);
    QTableWidgetItem* srcItem   = new QTableWidgetItem(record.source);
    QTableWidgetItem* msgItem   = new QTableWidgetItem(record.message);

    // 存储 alarmId 用于后续恢复查找
    timeItem->setData(Qt::UserRole, record.id);
    timeItem->setData(Qt::UserRole + 1, record.resolved);

    for (QTableWidgetItem* it : {timeItem, levelItem, srcItem, msgItem}) {
        if (record.resolved) {
            it->setForeground(QColor("#666"));
        }
    }
    levelItem->setForeground(QColor(record.resolved ? "#666" : levelColor));
    levelItem->setData(Qt::UserRole + 2, levelText);         // cache level text
    levelItem->setData(Qt::UserRole + 3, levelColor);        // cache level color
    levelItem->setData(Qt::UserRole + 1, record.resolved);   // resolved flag

    m_table->setItem(0, 0, timeItem);
    m_table->setItem(0, 1, levelItem);
    m_table->setItem(0, 2, srcItem);
    m_table->setItem(0, 3, msgItem);
    m_table->resizeRowToContents(0);

    // 限制最多 200 条
    while (m_table->rowCount() > 200) {
        m_table->removeRow(m_table->rowCount() - 1);
    }
    // 计数由 activeCountChanged 统一同步, 不自行累加
}

void AlarmListWidget::onAlarmResolved(int alarmId)
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* item = m_table->item(r, 0);
        if (!item || item->data(Qt::UserRole).toInt() != alarmId) continue;
        if (item->data(Qt::UserRole + 1).toBool()) continue; // already resolved

        // Mark resolved
        item->setData(Qt::UserRole + 1, true);
        for (int c = 0; c < 4; ++c) {
            QTableWidgetItem* cell = m_table->item(r, c);
            if (cell) cell->setForeground(QColor("#666"));
        }
        break;
    }
    // 计数由 activeCountChanged 统一同步, 不自行递减
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
