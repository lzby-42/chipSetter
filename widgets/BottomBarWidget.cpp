/**
 * @file BottomBarWidget.cpp
 * @brief 底部操作栏 实现
 */

#include "BottomBarWidget.h"
#include <QHBoxLayout>

BottomBarWidget::BottomBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

BottomBarWidget::~BottomBarWidget()
{
}

void BottomBarWidget::setupUI()
{
    setObjectName("BottomBarWidget");
    setFixedHeight(32);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(5);

    // 急停按钮
    m_emergencyBtn = new QPushButton("急停", this);
    m_emergencyBtn->setFixedSize(80, 28);
    m_emergencyBtn->setStyleSheet(
        "QPushButton { background:#c62828; color:#fff; border:none; "
        "border-radius:3px; font-weight:bold; font-size:11px; }"
        "QPushButton:hover { background:#e53935; }");
    layout->addWidget(m_emergencyBtn);

    // 启动
    m_startBtn = new QPushButton("启动", this);
    m_startBtn->setFixedSize(70, 28);
    m_startBtn->setStyleSheet(
        "QPushButton { background:#1565c0; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#1976d2; }");
    layout->addWidget(m_startBtn);

    // 暂停
    m_pauseBtn = new QPushButton("暂停", this);
    m_pauseBtn->setFixedSize(70, 28);
    m_pauseBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#455a64; }");
    layout->addWidget(m_pauseBtn);

    // 复位
    m_resetBtn = new QPushButton("复位", this);
    m_resetBtn->setFixedSize(70, 28);
    m_resetBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#455a64; }");
    layout->addWidget(m_resetBtn);

    layout->addStretch();

    // 模式切换 (生产 ↔ 调试)
    m_modeBtn = new QPushButton("⇄ 调试", this);
    m_modeBtn->setFixedSize(90, 28);
    m_modeBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#90a4ae; border:1px solid #555; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#455a64; color:#fff; }");
    layout->addWidget(m_modeBtn);

    // 通讯状态
    m_connLabel = new QLabel("GNC未连接", this);
    m_connLabel->setStyleSheet("color:#f44336; font-size:10px;");
    layout->addWidget(m_connLabel);

    // 连接信号
    connect(m_emergencyBtn, &QPushButton::clicked, this, &BottomBarWidget::emergencyStopClicked);
    connect(m_startBtn,      &QPushButton::clicked, this, &BottomBarWidget::startClicked);
    connect(m_pauseBtn,      &QPushButton::clicked, this, &BottomBarWidget::pauseClicked);
    connect(m_resetBtn,      &QPushButton::clicked, this, &BottomBarWidget::resetClicked);
    connect(m_modeBtn,       &QPushButton::clicked, this, &BottomBarWidget::modeSwitchClicked);
}

void BottomBarWidget::setConnectionStatus(bool connected)
{
    if (connected) {
        m_connLabel->setText("GNC已连接");
        m_connLabel->setStyleSheet("color:#4caf50; font-size:11px;");
    } else {
        m_connLabel->setText("GNC未连接");
        m_connLabel->setStyleSheet("color:#f44336; font-size:11px;");
    }
}

void BottomBarWidget::setMode(int mode)
{
    // mode: 0=调试(手动), 1=生产(自动)
    if (mode == 0) {
        m_modeBtn->setText("⇄ 生产");
        m_modeBtn->setStyleSheet(
            "QPushButton { background:#37474f; color:#90a4ae; border:1px solid #555; "
            "border-radius:3px; font-size:10px; }"
            "QPushButton:hover { background:#455a64; color:#fff; }");
    } else {
        m_modeBtn->setText("⇄ 调试");
        m_modeBtn->setStyleSheet(
            "QPushButton { background:#1b5e20; color:#81c784; border:1px solid #2e7d32; "
            "border-radius:3px; font-size:10px; }"
            "QPushButton:hover { background:#2e7d32; color:#a5d6a7; }");
    }
}
