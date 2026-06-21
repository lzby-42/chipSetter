/**
 * @file MotorPtpWidget.cpp
 * @brief 电机点位运动面板 实现 — compact layout
 */

#include "MotorPtpWidget.h"
#include "core/HardwareConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>

MotorPtpWidget::MotorPtpWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedAxisId(1)
    , m_currentPosition(0.0)
{
    setupUI();
}

MotorPtpWidget::~MotorPtpWidget()
{
}

void MotorPtpWidget::setupUI()
{
    setObjectName("MotorPtpWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 3, 4, 3);
    mainLayout->setSpacing(3);

    // ---- title + axis buttons in one row ----
    m_axisBtnGroup = new QButtonGroup(this);
    m_axisBtnGroup->setExclusive(true);

    QStringList names = AXIS_NAMES;
    QHBoxLayout* row1 = new QHBoxLayout();
    row1->setSpacing(2);
    QHBoxLayout* row2 = new QHBoxLayout();
    row2->setSpacing(2);

    for (int i = 0; i < AXIS_COUNT; ++i) {
        QPushButton* btn = new QPushButton(names[i], this);
        btn->setCheckable(true);
        btn->setFixedHeight(18);
        btn->setStyleSheet(
            "QPushButton { background:#333; color:#ccc; border:none; "
            "border-radius:2px; font-size:8px; padding:0 3px; }"
            "QPushButton:checked { background:#0d47a1; color:#fff; }");
        btn->setProperty("axisId", i + 1);
        m_axisBtnGroup->addButton(btn, i + 1);
        if (i < 7) row1->addWidget(btn);
        else       row2->addWidget(btn);
    }
    row1->addStretch();
    row2->addStretch();
    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);
    m_axisBtnGroup->button(1)->setChecked(true);

    connect(m_axisBtnGroup,
            QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MotorPtpWidget::onAxisButtonClicked);

    // ---- control row: position | buttons | jog ----
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(5);

    // current + target position
    QVBoxLayout* posLayout = new QVBoxLayout();
    posLayout->setSpacing(2);

    m_curPosLabel = new QLabel("+0.000 mm", this);
    m_curPosLabel->setStyleSheet("color:#ffd740; font-size:15px; font-family:monospace;");

    m_targetPosSpin = new QDoubleSpinBox(this);
    m_targetPosSpin->setRange(-9999.999, 9999.999);
    m_targetPosSpin->setDecimals(3);
    m_targetPosSpin->setValue(150.000);
    m_targetPosSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #64b5f6; "
        "border-radius:2px; padding:4px; font-size:14px; font-family:monospace; }");

    posLayout->addWidget(m_curPosLabel);
    posLayout->addWidget(m_targetPosSpin);
    controlLayout->addLayout(posLayout);

    // run / stop / home buttons
    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(2);

    m_runBtn = new QPushButton("运行", this);
    m_runBtn->setFixedSize(70, 26);
    m_runBtn->setStyleSheet("QPushButton{background:#2e7d32;color:#fff;border:none;border-radius:2px;font-weight:bold;font-size:11px;}QPushButton:hover{background:#388e3c;}");
    btnLayout->addWidget(m_runBtn);

    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setFixedSize(70, 26);
    m_stopBtn->setStyleSheet("QPushButton{background:#e65100;color:#fff;border:none;border-radius:2px;font-size:11px;}QPushButton:hover{background:#ff6d00;}");
    btnLayout->addWidget(m_stopBtn);

    m_homeBtn = new QPushButton("回零", this);
    m_homeBtn->setFixedSize(70, 26);
    m_homeBtn->setStyleSheet("QPushButton{background:#37474f;color:#fff;border:none;border-radius:2px;font-size:11px;}QPushButton:hover{background:#455a64;}");
    btnLayout->addWidget(m_homeBtn);

    controlLayout->addLayout(btnLayout);

    // JOG
    QVBoxLayout* jogLayout = new QVBoxLayout();
    jogLayout->setSpacing(2);

    QPushButton* jogPlusBtn = new QPushButton("JOG+", this);
    jogPlusBtn->setFixedSize(55, 22);
    jogPlusBtn->setStyleSheet("QPushButton{background:#333;color:#fff;border:none;border-radius:2px;font-size:9px;}QPushButton:hover{background:#444;}");
    jogLayout->addWidget(jogPlusBtn);

    m_jogStepSpin = new QDoubleSpinBox(this);
    m_jogStepSpin->setRange(0.001, 100.0);
    m_jogStepSpin->setDecimals(3);
    m_jogStepSpin->setValue(DEFAULT_JOG_STEP);
    m_jogStepSpin->setPrefix("+/-");
    m_jogStepSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #444; "
        "border-radius:2px; padding:1px; font-size:9px; }");
    jogLayout->addWidget(m_jogStepSpin);

    QPushButton* jogMinusBtn = new QPushButton("JOG-", this);
    jogMinusBtn->setFixedSize(55, 22);
    jogMinusBtn->setStyleSheet("QPushButton{background:#333;color:#fff;border:none;border-radius:2px;font-size:9px;}QPushButton:hover{background:#444;}");
    jogLayout->addWidget(jogMinusBtn);

    controlLayout->addLayout(jogLayout);
    mainLayout->addLayout(controlLayout);

    // ---- speed params: vel / acc / dec ----
    QHBoxLayout* paramLayout = new QHBoxLayout();
    paramLayout->setSpacing(8);

    auto makeParam = [this](const QString& label, QDoubleSpinBox*& spin,
                            double defVal, double maxVal) {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(2);
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#78909c; font-size:9px;");
        row->addWidget(lbl);
        spin = new QDoubleSpinBox(this);
        spin->setRange(0.001, maxVal);
        spin->setDecimals(1);
        spin->setValue(defVal);
        spin->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:2px; font-size:11px; font-family:monospace; min-width:70px; }");
        row->addWidget(spin);
        return row;
    };

    paramLayout->addLayout(makeParam("速度", m_velocitySpin, DEFAULT_VELOCITY, 500.0));
    paramLayout->addLayout(makeParam("加速度", m_accelSpin, DEFAULT_ACCEL, 2000.0));
    paramLayout->addLayout(makeParam("减速度", m_decelSpin, DEFAULT_DECEL, 2000.0));
    paramLayout->addStretch();
    mainLayout->addLayout(paramLayout);

    // ---- button connections ----
    connect(m_runBtn,  &QPushButton::clicked, this, &MotorPtpWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MotorPtpWidget::onStopClicked);
    connect(m_homeBtn, &QPushButton::clicked, this, &MotorPtpWidget::onHomeClicked);
    connect(jogPlusBtn,  &QPushButton::clicked, this, &MotorPtpWidget::onJogPlusClicked);
    connect(jogMinusBtn, &QPushButton::clicked, this, &MotorPtpWidget::onJogMinusClicked);
}

int MotorPtpWidget::currentAxisId() const
{
    return m_selectedAxisId;
}

void MotorPtpWidget::onAxisButtonClicked(int axisId)
{
    m_selectedAxisId = axisId;
}

void MotorPtpWidget::onRunClicked()
{
    emit moveRequested(m_selectedAxisId,
        m_targetPosSpin->value(), m_velocitySpin->value(),
        m_accelSpin->value(), m_decelSpin->value());
}

void MotorPtpWidget::onStopClicked()
{
    emit stopRequested(m_selectedAxisId);
}

void MotorPtpWidget::onHomeClicked()
{
    emit homeRequested(m_selectedAxisId);
}

void MotorPtpWidget::onJogPlusClicked()
{
    emit jogRequested(m_selectedAxisId, true, m_jogStepSpin->value(),
        m_velocitySpin->value(), m_accelSpin->value(), m_decelSpin->value());
}

void MotorPtpWidget::onJogMinusClicked()
{
    emit jogRequested(m_selectedAxisId, false, m_jogStepSpin->value(),
        m_velocitySpin->value(), m_accelSpin->value(), m_decelSpin->value());
}

void MotorPtpWidget::onPositionUpdated(int axisId, double position)
{
    if (axisId == m_selectedAxisId) {
        m_currentPosition = position;
        m_curPosLabel->setText(QString("%1 mm").arg(position, 0, 'f', 3));
    }
}

void MotorPtpWidget::onMoveFinished(int axisId, bool success)
{
    Q_UNUSED(axisId); Q_UNUSED(success);
}

void MotorPtpWidget::onHomeFinished(int axisId, bool success, int stage)
{
    Q_UNUSED(axisId); Q_UNUSED(success); Q_UNUSED(stage);
}

void MotorPtpWidget::onAxisStatusChanged(int axisId, long status)
{
    Q_UNUSED(axisId); Q_UNUSED(status);
}
