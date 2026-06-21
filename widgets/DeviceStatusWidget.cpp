/**
 * @file DeviceStatusWidget.cpp
 * @brief DeviceStatusWidget 实现
 */

#include "DeviceStatusWidget.h"
#include "core/HardwareConfig.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

DeviceStatusWidget::DeviceStatusWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

DeviceStatusWidget::~DeviceStatusWidget() {}

void DeviceStatusWidget::setupUI()
{
    setObjectName("DeviceStatusWidget");
    setStyleSheet(
        "QWidget#DeviceStatusWidget {"
        "  background:#1e1e30; border-radius:8px;"
        "}");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(6);

    // Title
    QLabel* title = new QLabel("设备状态", this);
    title->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    mainLayout->addWidget(title);

    // Axis status row (13 lights + names)
    QHBoxLayout* axisLayout = new QHBoxLayout();
    axisLayout->setSpacing(8);

    QStringList axisNames = AXIS_NAMES;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        QString shortName = (i < axisNames.size())
            ? axisNames[i] : QString("轴%1").arg(i + 1);

        QLabel* light = new QLabel("●", this);
        light->setStyleSheet("color:#555; font-size:11px; border:none;");
        light->setAlignment(Qt::AlignCenter);

        QLabel* name = new QLabel(shortName, this);
        name->setStyleSheet("color:#78909c; font-size:8px; border:none;");
        name->setAlignment(Qt::AlignCenter);

        QVBoxLayout* cell = new QVBoxLayout();
        cell->setSpacing(2);
        cell->addWidget(light, 0, Qt::AlignCenter);
        cell->addWidget(name, 0, Qt::AlignCenter);

        axisLayout->addLayout(cell);
        m_axisLights.append(light);
        m_axisNames.append(name);
    }

    axisLayout->addStretch();
    mainLayout->addLayout(axisLayout);

    // Info row: IO signals + connection
    QHBoxLayout* infoRow = new QHBoxLayout();
    infoRow->setSpacing(16);

    m_emStopLabel = new QLabel("急停: 正常", this);
    m_emStopLabel->setStyleSheet("color:#4caf50; font-size:9px; border:none;");
    infoRow->addWidget(m_emStopLabel);

    m_ioSummaryLabel = new QLabel("", this);
    m_ioSummaryLabel->setStyleSheet("color:#78909c; font-size:9px; border:none;");
    infoRow->addWidget(m_ioSummaryLabel);

    infoRow->addStretch();

    m_connLabel = new QLabel("GNC未连接", this);
    m_connLabel->setStyleSheet("color:#f44336; font-size:9px; border:none;");
    infoRow->addWidget(m_connLabel);

    mainLayout->addLayout(infoRow);
}

// ============================================================
// Slots
// ============================================================

void DeviceStatusWidget::updateAxisStates(const QVector<int>& enabled,
                                           const QVector<int>& moving,
                                           const QVector<int>& alarmed)
{
    for (int i = 0; i < m_axisLights.size() && i < AXIS_COUNT; ++i) {
        int axisId = i + 1;
        QString color = "#555"; // Grey: not enabled

        if (alarmed.contains(axisId))
            color = "#f44336";  // Red: alarm
        else if (moving.contains(axisId))
            color = "#2196f3";  // Blue: moving
        else if (enabled.contains(axisId))
            color = "#4caf50";  // Green: ready

        m_axisLights[i]->setStyleSheet(
            QString("color:%1; font-size:11px; border:none;").arg(color));
    }
}

void DeviceStatusWidget::setEmStopStatus(bool triggered)
{
    if (triggered) {
        m_emStopLabel->setText("急停: 触发!");
        m_emStopLabel->setStyleSheet(
            "color:#f44336; font-size:11px; font-weight:bold; border:none;");
    } else {
        m_emStopLabel->setText("急停: 正常");
        m_emStopLabel->setStyleSheet(
            "color:#4caf50; font-size:9px; border:none;");
    }
}

void DeviceStatusWidget::setConnectionStatus(bool connected)
{
    if (connected) {
        m_connLabel->setText("GNC已连接");
        m_connLabel->setStyleSheet("color:#4caf50; font-size:9px; border:none;");
    } else {
        m_connLabel->setText("GNC未连接");
        m_connLabel->setStyleSheet("color:#f44336; font-size:9px; border:none;");
    }
}

void DeviceStatusWidget::updateActiveIoSignals(const QStringList& activeSignals)
{
    if (activeSignals.isEmpty()) {
        m_ioSummaryLabel->setText("");
        return;
    }
    // Show first few active signals
    QStringList shown = activeSignals.mid(0, 3);
    m_ioSummaryLabel->setText(QString("IO: %1").arg(shown.join(", ")));
}
