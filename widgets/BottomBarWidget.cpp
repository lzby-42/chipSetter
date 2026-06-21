/**
 * @file BottomBarWidget.cpp
 * @brief 底部操作栏实现 — 6操作按钮 + 模式切换 + 通讯
 */

#include "BottomBarWidget.h"
#include <QHBoxLayout>

BottomBarWidget::BottomBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

BottomBarWidget::~BottomBarWidget() {}

void BottomBarWidget::setupUI()
{
    setObjectName("BottomBarWidget");
    setFixedHeight(34);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(6);

    auto makeBtn = [this](const QString& text, int w, const QString& style) {
        QPushButton* btn = new QPushButton(text, this);
        btn->setFixedSize(w, 28);
        btn->setStyleSheet(style);
        return btn;
    };

    // ===== 运行控制组 =====

    // 启动 (蓝色)
    m_startBtn = makeBtn("▶ 启动", 72,
        "QPushButton { background:#2e7d32; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; font-weight:bold; }"
        "QPushButton:hover { background:#388e3c; }");
    layout->addWidget(m_startBtn);

    // 暂停 (深灰)
    m_pauseBtn = makeBtn("⏸ 暂停", 72,
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#455a64; }");
    layout->addWidget(m_pauseBtn);

    // 收尾 (橙色 — 循环跑完进收尾)
    m_softFinishBtn = makeBtn("收尾", 62,
        "QPushButton { background:#e65100; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#ef6c00; }");
    m_softFinishBtn->setToolTip("当前循环完成后进入收尾");
    layout->addWidget(m_softFinishBtn);

    // 立即收尾 (深橙 — 立刻跳收尾)
    m_immediateFinishBtn = makeBtn("立即收尾", 78,
        "QPushButton { background:#bf360c; color:#fff; border:none; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#d84315; }");
    m_immediateFinishBtn->setToolTip("跳过当前循环, 立刻进入收尾");
    layout->addWidget(m_immediateFinishBtn);

    // 复位 (浅灰)
    m_resetBtn = makeBtn("↺ 复位", 72,
        "QPushButton { background:#546e7a; color:#fff; border:none; "
        "border-radius:3px; font-size:11px; }"
        "QPushButton:hover { background:#607d8b; }");
    m_resetBtn->setToolTip("急停+清除报警+重置统计");
    layout->addWidget(m_resetBtn);

    layout->addStretch();

    // ===== 急停 (红色, 独立在右侧) =====
    m_emergencyBtn = makeBtn("⏻ 急停", 80,
        "QPushButton { background:#c62828; color:#fff; border:none; "
        "border-radius:3px; font-weight:bold; font-size:12px; }"
        "QPushButton:hover { background:#e53935; }");
    layout->addWidget(m_emergencyBtn);

    // 模式切换
    m_modeBtn = new QPushButton("⇄ 调试", this);
    m_modeBtn->setFixedSize(88, 28);
    m_modeBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#90a4ae; border:1px solid #555; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#455a64; color:#fff; }");
    layout->addWidget(m_modeBtn);

    // 通讯状态
    m_connLabel = new QLabel("GNC未连接", this);
    m_connLabel->setStyleSheet("color:#f44336; font-size:10px;");
    layout->addWidget(m_connLabel);

    // ===== 信号连接 =====
    connect(m_startBtn,            &QPushButton::clicked, this, &BottomBarWidget::startClicked);
    connect(m_pauseBtn,            &QPushButton::clicked, this, &BottomBarWidget::pauseClicked);
    connect(m_softFinishBtn,       &QPushButton::clicked, this, &BottomBarWidget::softFinishClicked);
    connect(m_immediateFinishBtn,  &QPushButton::clicked, this, &BottomBarWidget::immediateFinishClicked);
    connect(m_resetBtn,            &QPushButton::clicked, this, &BottomBarWidget::resetClicked);
    connect(m_emergencyBtn,        &QPushButton::clicked, this, &BottomBarWidget::emergencyStopClicked);
    connect(m_modeBtn,             &QPushButton::clicked, this, &BottomBarWidget::modeSwitchClicked);
}

void BottomBarWidget::setConnectionStatus(bool connected)
{
    if (connected) {
        m_connLabel->setText("GNC已连接");
        m_connLabel->setStyleSheet("color:#4caf50; font-size:10px;");
    } else {
        m_connLabel->setText("GNC未连接");
        m_connLabel->setStyleSheet("color:#f44336; font-size:10px;");
    }
}

void BottomBarWidget::setMode(int mode)
{
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
