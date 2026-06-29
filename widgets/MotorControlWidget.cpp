#include "MotorControlWidget.h"
#include "core/HardwareConfig.h"
#include "core/MotorManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QStringList>

MotorControlWidget::MotorControlWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedAxisId(1)
{
    setupUI();
}

MotorControlWidget::~MotorControlWidget() {}

void MotorControlWidget::setMotorManager(MotorManager* mgr)
{
    m_motor = mgr;
}

int MotorControlWidget::currentAxisId() const
{
    return m_selectedAxisId;
}

void MotorControlWidget::setupUI()
{
    setObjectName("MotorControlWidget");
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 3, 4, 3);
    mainLayout->setSpacing(3);

    // ---- 13 axis buttons (2 rows) ----
    m_axisBtnGroup = new QButtonGroup(this);
    m_axisBtnGroup->setExclusive(true);
    QStringList names = AXIS_NAMES;
    QHBoxLayout* row1 = new QHBoxLayout(); row1->setSpacing(2);
    QHBoxLayout* row2 = new QHBoxLayout(); row2->setSpacing(2);
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
    row1->addStretch(); row2->addStretch();
    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);
    m_axisBtnGroup->button(1)->setChecked(true);
    connect(m_axisBtnGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MotorControlWidget::onAxisButtonClicked);

    // ---- Main area: left PTP | right params ----
    QHBoxLayout* body = new QHBoxLayout();
    body->setSpacing(6);

    // === LEFT: PTP motion control ===
    QVBoxLayout* ptpLayout = new QVBoxLayout();
    ptpLayout->setSpacing(3);

    m_curPosLabel = new QLabel("+0.000 mm", this);
    m_curPosLabel->setStyleSheet("color:#ffd740; font-size:14px; font-family:monospace;");
    ptpLayout->addWidget(m_curPosLabel);

    m_targetPosSpin = new QDoubleSpinBox(this);
    m_targetPosSpin->setRange(-9999.999, 9999.999);
    m_targetPosSpin->setDecimals(3);
    m_targetPosSpin->setValue(150.000);
    m_targetPosSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #64b5f6; "
        "border-radius:2px; padding:3px; font-size:12px; font-family:monospace; max-width:105px; }");
    ptpLayout->addWidget(m_targetPosSpin);

    // Speed/accel/decel (PTP only, persisted to JSON)
    auto makePtpSpin = [this](double min, double max, double val, const QString& prefix) {
        QDoubleSpinBox* sp = new QDoubleSpinBox(this);
        sp->setRange(min, max);
        sp->setDecimals(1);
        sp->setValue(val);
        sp->setPrefix(prefix);
        sp->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:1px; font-size:9px; font-family:monospace; max-width:105px; }");
        return sp;
    };
    m_velocitySpin = makePtpSpin(0.1, MAX_VELOCITY, DEFAULT_VELOCITY, "v:");
    m_accelSpin    = makePtpSpin(0.1, MAX_ACCEL, DEFAULT_ACCEL, "a:");
    m_decelSpin    = makePtpSpin(0.1, MAX_ACCEL, DEFAULT_DECEL, "d:");
    ptpLayout->addWidget(m_velocitySpin);
    ptpLayout->addWidget(m_accelSpin);
    ptpLayout->addWidget(m_decelSpin);

    // Motion buttons
    QHBoxLayout* runStopRow = new QHBoxLayout(); runStopRow->setSpacing(3);
    m_runBtn = new QPushButton("运行", this);
    m_runBtn->setFixedSize(60, 24);
    m_runBtn->setStyleSheet("QPushButton{background:#2e7d32;color:#fff;border:none;border-radius:2px;font-weight:bold;font-size:10px;}QPushButton:hover{background:#388e3c;}");
    runStopRow->addWidget(m_runBtn);
    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setFixedSize(60, 24);
    m_stopBtn->setStyleSheet("QPushButton{background:#e65100;color:#fff;border:none;border-radius:2px;font-size:10px;}QPushButton:hover{background:#ff6d00;}");
    runStopRow->addWidget(m_stopBtn);
    ptpLayout->addLayout(runStopRow);

    m_homeBtn = new QPushButton("回零", this);
    m_homeBtn->setFixedSize(123, 24);
    m_homeBtn->setStyleSheet("QPushButton{background:#37474f;color:#fff;border:none;border-radius:2px;font-size:10px;}QPushButton:hover{background:#455a64;}");
    ptpLayout->addWidget(m_homeBtn);

    // JOG
    QHBoxLayout* jogRow = new QHBoxLayout(); jogRow->setSpacing(2);
    QPushButton* jogPlusBtn = new QPushButton("JOG+", this);
    jogPlusBtn->setFixedSize(50, 22);
    jogPlusBtn->setStyleSheet("QPushButton{background:#333;color:#fff;border:none;border-radius:2px;font-size:8px;}QPushButton:hover{background:#444;}");
    jogRow->addWidget(jogPlusBtn);
    m_jogStepSpin = new QDoubleSpinBox(this);
    m_jogStepSpin->setRange(0.001, 100.0);
    m_jogStepSpin->setDecimals(3);
    m_jogStepSpin->setValue(DEFAULT_JOG_STEP);
    m_jogStepSpin->setPrefix("+/-");
    m_jogStepSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #444; "
        "border-radius:2px; padding:1px; font-size:8px; max-width:55px; }");
    jogRow->addWidget(m_jogStepSpin);
    QPushButton* jogMinusBtn = new QPushButton("JOG-", this);
    jogMinusBtn->setFixedSize(50, 22);
    jogMinusBtn->setStyleSheet("QPushButton{background:#333;color:#fff;border:none;border-radius:2px;font-size:8px;}QPushButton:hover{background:#444;}");
    jogRow->addWidget(jogMinusBtn);
    ptpLayout->addLayout(jogRow);

    // Clear alarm + Enable
    QHBoxLayout* auxRow = new QHBoxLayout(); auxRow->setSpacing(3);
    m_clearAlarmBtn = new QPushButton("清报警", this);
    m_clearAlarmBtn->setFixedSize(55, 22);
    m_clearAlarmBtn->setStyleSheet(
        "QPushButton{background:#4a148c;color:#fff;border:none;border-radius:2px;font-size:8px;}"
        "QPushButton:hover{background:#6a1b9a;}");
    auxRow->addWidget(m_clearAlarmBtn);
    m_enableBtn = new QPushButton("使能", this);
    m_enableBtn->setFixedSize(55, 22);
    m_enableBtn->setStyleSheet(
        "QPushButton{background:#1b5e20;color:#fff;border:none;border-radius:2px;font-size:8px;}"
        "QPushButton:hover{background:#2e7d32;}");
    auxRow->addWidget(m_enableBtn);
    ptpLayout->addLayout(auxRow);

    // Status indicators
    auto makeStsLabel = [this](const QString& text) {
        QLabel* lbl = new QLabel(text, this);
        lbl->setStyleSheet("color:#555; font-size:7px; padding:1px 3px;");
        lbl->setFixedHeight(14);
        return lbl;
    };
    QHBoxLayout* stsRow = new QHBoxLayout(); stsRow->setSpacing(3);
    m_stsAlarmLabel  = makeStsLabel("急停");
    m_stsDriveLabel  = makeStsLabel("驱动");
    m_stsLimitPLabel = makeStsLabel("限位+");
    m_stsLimitNLabel = makeStsLabel("限位-");
    m_stsMovingLabel = makeStsLabel("运行");
    m_stsDoneLabel   = makeStsLabel("到位");
    stsRow->addWidget(m_stsAlarmLabel);
    stsRow->addWidget(m_stsDriveLabel);
    stsRow->addWidget(m_stsLimitPLabel);
    stsRow->addWidget(m_stsLimitNLabel);
    stsRow->addWidget(m_stsMovingLabel);
    stsRow->addWidget(m_stsDoneLabel);
    stsRow->addStretch();
    ptpLayout->addLayout(stsRow);

    ptpLayout->addStretch();
    body->addLayout(ptpLayout);

    // === RIGHT: Param grid ===
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(3);
    grid->setVerticalSpacing(4);
    auto addRow = [&](int row, const QString& label, QWidget* editor,
                      const QString& unit, QCheckBox* check = nullptr) {
        int col = 0;
        if (check) grid->addWidget(check, row, col++);
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#90a4ae; font-size:9px;");
        grid->addWidget(lbl, row, col++);
        grid->addWidget(editor, row, col++);
        if (!unit.isEmpty()) {
            QLabel* ul = new QLabel(unit, this);
            ul->setStyleSheet("color:#78909c; font-size:7px;");
            grid->addWidget(ul, row, col);
        }
    };

    auto makeDSpin = [this](double min, double max, double val, int dec) {
        QDoubleSpinBox* sp = new QDoubleSpinBox(this);
        sp->setRange(min, max); sp->setDecimals(dec); sp->setValue(val);
        sp->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:2px; font-size:10px; font-family:monospace; max-width:80px; }");
        return sp;
    };
    auto makeISpin = [this](int min, int max, int val) {
        QSpinBox* sp = new QSpinBox(this);
        sp->setRange(min, max); sp->setValue(val);
        sp->setStyleSheet(
            "QSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:2px; font-size:10px; font-family:monospace; max-width:80px; }");
        return sp;
    };

    m_leadScrewSpin    = makeDSpin(0.1, 100.0, DEFAULT_LEAD_SCREW, 3);
    m_pulsePerRevSpin  = makeISpin(100, 100000, DEFAULT_PULSE_PER_REV);
    m_gearRatioSpin    = makeDSpin(0.01, 100.0, DEFAULT_GEAR_RATIO, 2);
    m_softLimitPosSpin = makeDSpin(-9999.0, 9999.0, 9999.0, 3);
    m_softLimitNegSpin = makeDSpin(-9999.0, 9999.0, -9999.0, 3);
    m_homeVelSpin      = makeDSpin(0.1, 200.0, 50.0, 1);
    m_homeOffsetSpin   = makeDSpin(-999.0, 999.0, 2.0, 3);

    m_leadScrewCheck    = new QCheckBox(this);
    m_softLimitPosCheck = new QCheckBox(this);
    m_softLimitNegCheck = new QCheckBox(this);
    for (auto* c : {m_leadScrewCheck, m_softLimitPosCheck, m_softLimitNegCheck}) {
        c->setChecked(true);
        c->setStyleSheet("QCheckBox { spacing:0px; }");
    }

    int r = 0;
    addRow(r++, "导程",    m_leadScrewSpin,    "rev", m_leadScrewCheck);
    addRow(r++, "每转脉冲", m_pulsePerRevSpin,  "pls");
    addRow(r++, "齿轮比",  m_gearRatioSpin,    "");
    addRow(r++, "软限位+", m_softLimitPosSpin, "mm",  m_softLimitPosCheck);
    addRow(r++, "软限位-", m_softLimitNegSpin, "mm",  m_softLimitNegCheck);
    addRow(r++, "回零速度", m_homeVelSpin,      "s");
    addRow(r++, "回零偏移", m_homeOffsetSpin,   "mm");

    // Apply/Export/Import
    QHBoxLayout* actionRow = new QHBoxLayout(); actionRow->setSpacing(4);
    QPushButton* applyBtn = new QPushButton("应用", this);
    applyBtn->setStyleSheet("QPushButton{background:#1b5e20;color:#a5d6a7;border:none;border-radius:3px;padding:3px 10px;font-size:9px;}QPushButton:hover{background:#2e7d32;}");
    actionRow->addWidget(applyBtn);
    QPushButton* exportBtn = new QPushButton("导出", this);
    exportBtn->setStyleSheet("QPushButton{background:#37474f;color:#fff;border:none;border-radius:3px;padding:3px 10px;font-size:9px;}QPushButton:hover{background:#455a64;}");
    actionRow->addWidget(exportBtn);
    QPushButton* importBtn = new QPushButton("导入", this);
    importBtn->setStyleSheet("QPushButton{background:#4a148c;color:#ce93d8;border:none;border-radius:3px;padding:3px 10px;font-size:9px;}QPushButton:hover{background:#6a1b9a;}");
    actionRow->addWidget(importBtn);
    actionRow->addStretch();

    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(4);
    rightLayout->addLayout(grid);
    rightLayout->addLayout(actionRow);
    rightLayout->addStretch();
    body->addLayout(rightLayout);

    mainLayout->addLayout(body);

    // === Connections ===
    connect(m_runBtn,        &QPushButton::clicked, this, &MotorControlWidget::onRunClicked);
    connect(m_stopBtn,       &QPushButton::clicked, this, &MotorControlWidget::onStopClicked);
    connect(m_homeBtn,       &QPushButton::clicked, this, &MotorControlWidget::onHomeClicked);
    connect(jogPlusBtn,      &QPushButton::clicked, this, &MotorControlWidget::onJogPlusClicked);
    connect(jogMinusBtn,     &QPushButton::clicked, this, &MotorControlWidget::onJogMinusClicked);
    connect(m_clearAlarmBtn, &QPushButton::clicked, this, &MotorControlWidget::onClearAlarmClicked);
    connect(m_enableBtn,     &QPushButton::clicked, this, &MotorControlWidget::onEnableClicked);
    connect(applyBtn,        &QPushButton::clicked, this, &MotorControlWidget::onApplyClicked);
    connect(exportBtn,       &QPushButton::clicked, this, &MotorControlWidget::onExportClicked);
    connect(importBtn,       &QPushButton::clicked, this, &MotorControlWidget::onImportClicked);

    connect(m_leadScrewCheck,    &QCheckBox::toggled, m_leadScrewSpin,    &QWidget::setEnabled);
    connect(m_softLimitPosCheck, &QCheckBox::toggled, m_softLimitPosSpin, &QWidget::setEnabled);
    connect(m_softLimitNegCheck, &QCheckBox::toggled, m_softLimitNegSpin, &QWidget::setEnabled);
}

// === Axis selection ===
void MotorControlWidget::onAxisButtonClicked(int axisId)
{
    if (m_selectedAxisId != axisId && m_motor) {
        const MotorAxis& cur = m_motor->axisState(m_selectedAxisId);
        if (m_softLimitPosSpin->value() != cur.softLimitPositive ||
            m_softLimitNegSpin->value() != cur.softLimitNegative ||
            m_velocitySpin->value() != cur.velocity ||
            m_accelSpin->value() != cur.acceleration ||
            m_decelSpin->value() != cur.deceleration) {
            onApplyClicked();
        }
    }
    m_selectedAxisId = axisId;
    loadAxisFromManager(axisId);
}

void MotorControlWidget::setCurrentAxisId(int axisId)
{
    m_selectedAxisId = axisId;
    if (m_axisBtnGroup->button(axisId))
        m_axisBtnGroup->button(axisId)->setChecked(true);
    loadAxisFromManager(axisId);
}

void MotorControlWidget::loadAxisFromManager(int axisId)
{
    if (!m_motor) return;
    const MotorAxis& ax = m_motor->axisState(axisId);
    m_leadScrewSpin->setValue(ax.leadScrew);
    m_pulsePerRevSpin->setValue(ax.pulsePerRev);
    m_gearRatioSpin->setValue(ax.gearRatio);
    m_softLimitPosSpin->setValue(ax.softLimitPositive);
    m_softLimitNegSpin->setValue(ax.softLimitNegative);
    m_homeVelSpin->setValue(ax.homeVelocity);
    m_homeOffsetSpin->setValue(ax.homeOffset);
    m_velocitySpin->setValue(ax.velocity);
    m_accelSpin->setValue(ax.acceleration);
    m_decelSpin->setValue(ax.deceleration);
    m_jogStepSpin->setValue(ax.jogStep);
    m_leadScrewCheck->setChecked(ax.hasLeadScrew);
    m_softLimitPosCheck->setChecked(ax.hasSoftLimitPositive);
    m_softLimitNegCheck->setChecked(ax.hasSoftLimitNegative);
}

// === Motion ===
void MotorControlWidget::onRunClicked()
{
    emit moveRequested(m_selectedAxisId,
        m_targetPosSpin->value(), m_velocitySpin->value(),
        m_accelSpin->value(), m_decelSpin->value());
}

void MotorControlWidget::onStopClicked()
{
    emit stopRequested(m_selectedAxisId);
}

void MotorControlWidget::onHomeClicked()
{
    emit homeRequested(m_selectedAxisId);
}

void MotorControlWidget::onJogPlusClicked()
{
    emit jogRequested(m_selectedAxisId, true, m_jogStepSpin->value(),
        m_velocitySpin->value(), m_accelSpin->value(), m_decelSpin->value());
}

void MotorControlWidget::onJogMinusClicked()
{
    emit jogRequested(m_selectedAxisId, false, m_jogStepSpin->value(),
        m_velocitySpin->value(), m_accelSpin->value(), m_decelSpin->value());
}

void MotorControlWidget::onClearAlarmClicked()
{
    emit clearAlarmRequested(m_selectedAxisId);
}

void MotorControlWidget::onEnableClicked()
{
    bool targetState = !m_enableBtn->text().contains("失");
    emit enableRequested(m_selectedAxisId, targetState);
}

// === Params ===
void MotorControlWidget::onApplyClicked()
{
    MotorAxis params;
    params.axisId              = m_selectedAxisId;
    params.velocity            = m_velocitySpin->value();
    params.acceleration        = m_accelSpin->value();
    params.deceleration        = m_decelSpin->value();
    params.leadScrew           = m_leadScrewSpin->value();
    params.pulsePerRev         = m_pulsePerRevSpin->value();
    params.gearRatio           = m_gearRatioSpin->value();
    params.softLimitPositive   = m_softLimitPosSpin->value();
    params.softLimitNegative   = m_softLimitNegSpin->value();
    params.homeVelocity        = m_homeVelSpin->value();
    params.homeOffset          = m_homeOffsetSpin->value();
    params.hasLeadScrew        = m_leadScrewCheck->isChecked();
    params.hasSoftLimitPositive = m_softLimitPosCheck->isChecked();
    params.hasSoftLimitNegative = m_softLimitNegCheck->isChecked();
    emit applyRequested(m_selectedAxisId, params);
}

void MotorControlWidget::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "导出电机参数", "motor_params.json", "JSON (*.json)");
    if (!path.isEmpty()) emit exportRequested(path);
}

void MotorControlWidget::onImportClicked()
{
    QString path = QFileDialog::getOpenFileName(this, "导入电机参数", "", "JSON (*.json)");
    if (!path.isEmpty()) emit importRequested(path);
}

// === Feedback slots ===
void MotorControlWidget::onPositionUpdated(int axisId, double position)
{
    if (axisId == m_selectedAxisId)
        m_curPosLabel->setText(QString("%1 mm").arg(position, 0, 'f', 3));
}

void MotorControlWidget::onMoveFinished(int axisId, bool success)
    { Q_UNUSED(axisId); Q_UNUSED(success); }
void MotorControlWidget::onHomeFinished(int axisId, bool success, int stage)
    { Q_UNUSED(axisId); Q_UNUSED(success); Q_UNUSED(stage); }
void MotorControlWidget::onAxisStatusChanged(int axisId, long status)
{
    if (axisId != m_selectedAxisId) return;
    auto setLabel = [](QLabel* lbl, bool active, const QString& color) {
        lbl->setStyleSheet(QString("color:%1; font-size:7px; padding:1px 3px;"
            "background:%2; border-radius:2px;")
            .arg(active ? "#fff" : "#555")
            .arg(active ? color : "#222"));
    };
    setLabel(m_stsAlarmLabel,  status & 0x001, "#b71c1c");   // bit 0 急停报警
    setLabel(m_stsDriveLabel,  status & 0x002, "#d32f2f");   // bit 1 驱动报警
    setLabel(m_stsLimitPLabel, status & 0x020, "#e65100");   // bit 5 正限位
    setLabel(m_stsLimitNLabel, status & 0x040, "#ff9800");   // bit 6 负限位
    setLabel(m_stsMovingLabel, status & 0x400, "#1565c0");   // bit 10 运行中
    setLabel(m_stsDoneLabel,   status & 0x800, "#1b5e20");   // bit 11 已到位
}

void MotorControlWidget::onAxisEnableChanged(int axisId, bool enabled)
{
    if (axisId != m_selectedAxisId) return;
    if (enabled) {
        m_enableBtn->setText("失能");
        m_enableBtn->setStyleSheet(
            "QPushButton{background:#b71c1c;color:#fff;border:none;border-radius:2px;font-size:8px;}"
            "QPushButton:hover{background:#d32f2f;}");
    } else {
        m_enableBtn->setText("使能");
        m_enableBtn->setStyleSheet(
            "QPushButton{background:#1b5e20;color:#fff;border:none;border-radius:2px;font-size:8px;}"
            "QPushButton:hover{background:#2e7d32;}");
    }
}

void MotorControlWidget::onParamsApplied(int axisId)
{
    if (axisId != m_selectedAxisId) return;
    loadAxisFromManager(axisId);
}
