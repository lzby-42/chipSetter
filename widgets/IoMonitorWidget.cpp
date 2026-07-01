/**
 * @file IoMonitorWidget.cpp
 * @brief IO信号监视面板 实现
 */

#include "IoMonitorWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QGridLayout>

IoMonitorWidget::IoMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    memset(m_doValues, 0, sizeof(m_doValues));
    setupUI();
}

IoMonitorWidget::~IoMonitorWidget()
{
}

void IoMonitorWidget::setupUI()
{
    setObjectName("IoMonitorWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ---- 标题 ----
    QLabel* title = new QLabel("IO信号状态", this);
    title->setStyleSheet("font-weight:bold; color:#81c784; font-size:13px;");
    mainLayout->addWidget(title);

    // ---- DI区域 ----
    QLabel* diTitle = new QLabel("输入 DI (限位/Home)", this);
    diTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    mainLayout->addWidget(diTitle);

    // DI网格: 5列 x 4行 (19个DI: 5+5+5+4)
    QGridLayout* diGrid = new QGridLayout();
    diGrid->setSpacing(2);

    QStringList diNames = DI_NAMES;
    for (int i = 0; i < DI_COUNT; ++i) {
        QLabel* lbl = new QLabel("O " + (i < diNames.size() ? diNames[i] : QString("DI_%1").arg(i + 1)), this);
        lbl->setStyleSheet(
            "background:#333; color:#888; padding:2px 4px; border-radius:2px; font-size:7px;");
        lbl->setMinimumHeight(20);
        diGrid->addWidget(lbl, i / 5, i % 5);
        m_diLabels.append(lbl);
    }
    mainLayout->addLayout(diGrid);

    // ---- DO区域 ----
    QLabel* doTitle = new QLabel("输出 DO (点击标签切换)", this);
    doTitle->setStyleSheet("color:#90a4ae; font-size:10px; margin-top:4px;");
    mainLayout->addWidget(doTitle);

    QHBoxLayout* doLayout = new QHBoxLayout();
    doLayout->setSpacing(6);

    QStringList doNames = DO_NAMES;
    for (int i = 0; i < DO_COUNT; ++i) {
        QLabel* lbl = new QLabel("O " + (i < doNames.size() ? doNames[i] : QString("DO_%1").arg(i + 1)), this);
        lbl->setMinimumHeight(26);
        lbl->setCursor(Qt::PointingHandCursor);
        lbl->setToolTip("点击切换DO输出状态");
        lbl->setStyleSheet(
            "background:#333; color:#888; padding:3px 14px; border-radius:2px; font-size:10px;");
        lbl->installEventFilter(this);   // 拦截鼠标点击
        doLayout->addWidget(lbl);
        m_doLabels.append(lbl);
    }
    doLayout->addStretch();
    mainLayout->addLayout(doLayout);

    mainLayout->addStretch();
}

void IoMonitorWidget::onDiChanged(int id, int value)
{
    if (id < 1 || id > DI_COUNT) return;
    updateLabelStyle(m_diLabels[id - 1], IO_TYPE_DI, value);
}

void IoMonitorWidget::onDoChanged(int id, int value)
{
    if (id < 1 || id > DO_COUNT) return;
    m_doValues[id] = static_cast<short>(value);
    updateLabelStyle(m_doLabels[id - 1], IO_TYPE_DO, value);
}

bool IoMonitorWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < m_doLabels.size(); ++i) {
            if (obj == m_doLabels[i]) {
                int doId = i + 1;
                int newVal = m_doValues[doId] ? 0 : 1;
                m_doValues[doId] = newVal;
                updateLabelStyle(m_doLabels[i], IO_TYPE_DO, newVal);
                emit doToggleRequested(doId, newVal);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void IoMonitorWidget::refreshAll(const QVector<IoSignal>& diSignals, const QVector<IoSignal>& doSignals)
{
    for (const IoSignal& sig : diSignals) {
        onDiChanged(sig.id, sig.value);
    }
    for (const IoSignal& sig : doSignals) {
        onDoChanged(sig.id, sig.value);
    }
}

void IoMonitorWidget::updateLabelStyle(QLabel* label, IoType type, int value)
{
    if (!label) return;

    QString text = label->text();
    // 提取信号名 (去掉前缀符号)
    int spaceIdx = text.indexOf(' ');
    QString name = (spaceIdx >= 0) ? text.mid(spaceIdx + 1) : text;

    QString prefix, bgColor, textColor;
    if (type == IO_TYPE_DI) {
        if (value == 1) {
            prefix = "* "; bgColor = "#1b5e20"; textColor = "#a5d6a7";
        } else {
            // DI=0 且是Home信号 → 红色警告, 其余灰色
            if (name.contains("Home") || name.contains("home")) {
                prefix = "! "; bgColor = "#4e1a1a"; textColor = "#ef9a9a";
            } else {
                prefix = "O "; bgColor = "#333"; textColor = "#888";
            }
        }
    } else { // DO
        if (value == 1) {
            prefix = "* "; bgColor = "#0d3b66"; textColor = "#90caf9";
        } else {
            prefix = "O "; bgColor = "#4e342e"; textColor = "#bcaaa4";
        }
    }

    label->setText(prefix + name);
    label->setStyleSheet(
        QString("background:%1; color:%2; padding:2px 7px; border-radius:2px; font-size:8px;")
            .arg(bgColor, textColor));
}
