/**
 * @file FlowStepBar.cpp
 * @brief FlowStepBar 实现 — 流程步骤条绘制与交互
 */

#include "FlowStepBar.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVariant>

FlowStepBar::FlowStepBar(QWidget *parent)
    : QWidget(parent)
    , m_loopFrame(nullptr)
    , m_currentStep(-1)
{
    // setSteps will be called externally to populate
}

FlowStepBar::~FlowStepBar() {}

int FlowStepBar::currentStep() const
{
    return m_currentStep;
}

void FlowStepBar::setSteps(const QStringList& names)
{
    // Clear old layout and buttons
    QLayout* oldLayout = layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete oldLayout;
    }
    m_stepButtons.clear();

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);

    auto addArrow = [&]() {
        QLabel* arrow = new QLabel(QString::fromUtf8("→"), this);
        arrow->setStyleSheet("color:#455a64; font-size:18px; font-weight:bold; border:none;");
        arrow->setFixedWidth(28);
        arrow->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(arrow);
    };

    // ===== Step 0: Init (outside loop) =====
    QPushButton* btn0 = new QPushButton(this);
    btn0->setText(QString("%1\n%2").arg(QString::fromUtf8("①"), names.value(0, QString::fromUtf8("初始化"))));
    btn0->setFixedSize(72, 60);
    btn0->setCursor(Qt::PointingHandCursor);
    btn0->setProperty("stepIndex", 0);
    connect(btn0, &QPushButton::clicked, this, [this]() {
        QPushButton* btn = qobject_cast<QPushButton*>(sender());
        if (btn) {
            int idx = btn->property("stepIndex").toInt();
            emit stepClicked(idx);
        }
    });
    m_stepButtons.append(btn0);
    mainLayout->addWidget(btn0);

    addArrow();

    // ===== Steps 1-7: Loop frame =====
    m_loopFrame = new QFrame(this);
    m_loopFrame->setObjectName("LoopFrame");
    m_loopFrame->setStyleSheet(
        "QFrame#LoopFrame {"
        "  background:#0d1a0d;"
        "  border:2px solid #2e7d32;"
        "  border-radius:10px;"
        "}");

    QVBoxLayout* loopOuterLayout = new QVBoxLayout(m_loopFrame);
    loopOuterLayout->setContentsMargins(10, 6, 10, 4);
    loopOuterLayout->setSpacing(0);

    // Buttons row inside loop
    QHBoxLayout* loopInnerLayout = new QHBoxLayout();
    loopInnerLayout->setContentsMargins(0, 0, 0, 0);
    loopInnerLayout->setSpacing(0);

    for (int i = LOOP_START; i <= LOOP_END; ++i) {
        if (i > LOOP_START) {
            QLabel* innerArrow = new QLabel(QString::fromUtf8("→"), this);
            innerArrow->setStyleSheet("color:#455a64; font-size:16px; font-weight:bold; border:none;");
            innerArrow->setFixedWidth(24);
            innerArrow->setAlignment(Qt::AlignCenter);
            loopInnerLayout->addWidget(innerArrow);
        }

        QPushButton* btn = new QPushButton(this);
        // Unicode circled numbers: ①=0x2460, ②=0x2461, ..., ⑨=0x2468
        QChar stepChar(0x2460 + i);
        btn->setText(QString("%1\n%2").arg(stepChar, names.value(i, "")));
        btn->setFixedSize(72, 60);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("stepIndex", i);
        connect(btn, &QPushButton::clicked, this, [this]() {
            QPushButton* b = qobject_cast<QPushButton*>(sender());
            if (b) {
                int idx = b->property("stepIndex").toInt();
                emit stepClicked(idx);
            }
        });
        m_stepButtons.append(btn);
        loopInnerLayout->addWidget(btn);
    }

    loopInnerLayout->addStretch();
    loopOuterLayout->addLayout(loopInnerLayout);

    // Return arrow (loop indicator)
    QLabel* loopArrow = new QLabel(QString::fromUtf8("⤴"), this);
    loopArrow->setStyleSheet("color:#ffd740; font-size:16px; border:none;");
    loopArrow->setAlignment(Qt::AlignCenter);
    loopArrow->setToolTip(QString::fromUtf8("下料完成后自动回到上料"));
    loopOuterLayout->addWidget(loopArrow);

    mainLayout->addWidget(m_loopFrame);

    addArrow();

    // ===== Step 8: Finish (outside loop) =====
    QPushButton* btn8 = new QPushButton(this);
    QChar lastChar(0x2468); // ⑨
    btn8->setText(QString("%1\n%2").arg(lastChar, names.value(8, QString::fromUtf8("收尾"))));
    btn8->setFixedSize(72, 60);
    btn8->setCursor(Qt::PointingHandCursor);
    btn8->setProperty("stepIndex", 8);
    connect(btn8, &QPushButton::clicked, this, [this]() {
        QPushButton* b = qobject_cast<QPushButton*>(sender());
        if (b) {
            int idx = b->property("stepIndex").toInt();
            emit stepClicked(idx);
        }
    });
    m_stepButtons.append(btn8);
    mainLayout->addWidget(btn8);

    mainLayout->addStretch();

    // Apply initial PENDING style to all
    for (int i = 0; i < m_stepButtons.size(); ++i) {
        updateButtonStyle(i, 0);
    }
}

void FlowStepBar::setStepState(int stepIndex, int state)
{
    if (stepIndex < 0 || stepIndex >= m_stepButtons.size()) return;
    updateButtonStyle(stepIndex, state);
}

void FlowStepBar::setCurrentStep(int stepIndex)
{
    m_currentStep = stepIndex;
}

void FlowStepBar::updateButtonStyle(int stepIndex, int state)
{
    if (stepIndex < 0 || stepIndex >= m_stepButtons.size()) return;
    QPushButton* btn = m_stepButtons[stepIndex];

    QString style;
    switch (state) {
    case 0: // PENDING
        style =
            "QPushButton {"
            "  background:#2a2a3a; color:#666;"
            "  border:1px solid #333; border-radius:5px;"
            "  font-size:11px; padding:8px 10px;"
            "}"
            "QPushButton:hover { background:#333; }";
        break;
    case 1: // RUNNING
        style =
            "QPushButton {"
            "  background:#1565c0; color:#fff;"
            "  border:2px solid #64b5f6; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    case 2: // COMPLETED
        style =
            "QPushButton {"
            "  background:#1b5e20; color:#81c784;"
            "  border:1px solid #388e3c; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    case 3: // FAILED
        style =
            "QPushButton {"
            "  background:#b71c1c; color:#ef9a9a;"
            "  border:1px solid #f44336; border-radius:5px;"
            "  font-size:11px; font-weight:bold; padding:8px 10px;"
            "}";
        break;
    default:
        return;
    }

    btn->setStyleSheet(style);
}
