/**
 * @file MotorParamWidget.cpp
 * @brief 电机参数管理面板 实现
 */

#include "MotorParamWidget.h"
#include "core/HardwareConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

MotorParamWidget::MotorParamWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedAxisId(1)
{
    setupUI();
}

MotorParamWidget::~MotorParamWidget()
{
}

void MotorParamWidget::setupUI()
{
    setObjectName("MotorParamWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ---- 标题 ----
    QLabel* title = new QLabel("电机参数管理", this);
    title->setStyleSheet("font-weight:bold; color:#64b5f6; font-size:13px;");
    mainLayout->addWidget(title);

    // ---- 轴选择 ----
    m_axisBtnGroup = new QButtonGroup(this);
    m_axisBtnGroup->setExclusive(true);

    // Two rows: row1=7 buttons, row2=6 buttons (13 total)
    QVBoxLayout* axisRowsLayout = new QVBoxLayout();
    axisRowsLayout->setSpacing(2);

    QStringList names = AXIS_NAMES;
    QHBoxLayout* row1 = new QHBoxLayout();
    row1->setSpacing(3);
    QHBoxLayout* row2 = new QHBoxLayout();
    row2->setSpacing(3);

    for (int i = 0; i < AXIS_COUNT; ++i) {
        QPushButton* btn = new QPushButton(names[i], this);
        btn->setCheckable(true);
        btn->setFixedHeight(22);
        btn->setStyleSheet(
            "QPushButton { background:#333; color:#ccc; border:none; "
            "border-radius:2px; font-size:9px; padding:0 5px; }"
            "QPushButton:checked { background:#0d47a1; color:#fff; }");
        btn->setProperty("axisId", i + 1);
        m_axisBtnGroup->addButton(btn, i + 1);
        if (i < 7) {
            row1->addWidget(btn);
        } else {
            row2->addWidget(btn);
        }
    }
    row1->addStretch();
    row2->addStretch();
    axisRowsLayout->addLayout(row1);
    axisRowsLayout->addLayout(row2);
    mainLayout->addLayout(axisRowsLayout);

    connect(m_axisBtnGroup,
            QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MotorParamWidget::onAxisButtonClicked);
    m_axisBtnGroup->button(1)->setChecked(true);

    // ---- 参数表格 ----
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);
    grid->setVerticalSpacing(6);

    auto addRow = [&](int row, const QString& label,
                      QWidget* editor, const QString& unit = "") {
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#90a4ae; font-size:10px;");
        grid->addWidget(lbl, row, 0);
        grid->addWidget(editor, row, 1);
        if (!unit.isEmpty()) {
            QLabel* unitLbl = new QLabel(unit, this);
            unitLbl->setStyleSheet("color:#78909c; font-size:9px;");
            grid->addWidget(unitLbl, row, 2);
        }
    };

    // 创建所有spinbox
    auto makeDSpin = [this](double min, double max, double val, int decimals) {
        QDoubleSpinBox* sp = new QDoubleSpinBox(this);
        sp->setRange(min, max);
        sp->setDecimals(decimals);
        sp->setValue(val);
        sp->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:3px; font-size:11px; font-family:monospace; max-width:100px; }");
        return sp;
    };
    auto makeISpin = [this](int min, int max, int val) {
        QSpinBox* sp = new QSpinBox(this);
        sp->setRange(min, max);
        sp->setValue(val);
        sp->setStyleSheet(
            "QSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:3px; font-size:11px; font-family:monospace; max-width:100px; }");
        return sp;
    };

    m_leadScrewSpin      = makeDSpin(0.1, 100.0, DEFAULT_LEAD_SCREW, 3);
    m_pulsePerRevSpin    = makeISpin(100, 100000, DEFAULT_PULSE_PER_REV);
    m_gearRatioSpin      = makeDSpin(0.01, 100.0, DEFAULT_GEAR_RATIO, 2);
    m_maxVelocitySpin    = makeDSpin(0.1, MAX_VELOCITY, DEFAULT_VELOCITY, 1);
    m_accelSpin          = makeDSpin(0.1, MAX_ACCEL, DEFAULT_ACCEL, 1);
    m_decelSpin          = makeDSpin(0.1, MAX_ACCEL, DEFAULT_DECEL, 1);
    m_softLimitPosSpin   = makeDSpin(-9999.0, 9999.0, 300.0, 3);
    m_softLimitNegSpin   = makeDSpin(-9999.0, 9999.0, 0.0, 3);
    m_homeVelSpin        = makeDSpin(0.1, 200.0, 50.0, 1);
    m_homeOffsetSpin     = makeDSpin(-999.0, 999.0, 2.0, 3);

    int r = 0;
    addRow(r++, "导程",           m_leadScrewSpin,    "mm/rev");
    addRow(r++, "每转脉冲",        m_pulsePerRevSpin,  "pulse");
    addRow(r++, "电子齿轮比",      m_gearRatioSpin,    "");
    addRow(r++, "最大速度",        m_maxVelocitySpin,  "mm/s");
    addRow(r++, "加速度",          m_accelSpin,        "mm/s2");
    addRow(r++, "减速度",          m_decelSpin,        "mm/s2");
    addRow(r++, "软限位+",         m_softLimitPosSpin, "mm");
    addRow(r++, "软限位-",         m_softLimitNegSpin, "mm");
    addRow(r++, "回零速度",        m_homeVelSpin,      "mm/s");
    addRow(r++, "回零偏移",        m_homeOffsetSpin,   "mm");

    mainLayout->addLayout(grid);

    // ---- 操作按钮: 应用 / 保存 / 加载 ----
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(6);

    QPushButton* applyBtn = new QPushButton("应用", this);
    applyBtn->setStyleSheet(
        "QPushButton { background:#1b5e20; color:#a5d6a7; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#2e7d32; }");
    actionLayout->addWidget(applyBtn);

    QPushButton* saveBtn = new QPushButton("保存", this);
    saveBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#455a64; }");
    actionLayout->addWidget(saveBtn);

    QPushButton* loadBtn = new QPushButton("加载", this);
    loadBtn->setStyleSheet(
        "QPushButton { background:#b71c1c; color:#ef9a9a; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#d32f2f; }");
    actionLayout->addWidget(loadBtn);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    connect(applyBtn, &QPushButton::clicked, this, &MotorParamWidget::onApplyClicked);
    connect(saveBtn,  &QPushButton::clicked, this, &MotorParamWidget::onSaveClicked);
    connect(loadBtn,  &QPushButton::clicked, this, &MotorParamWidget::onLoadClicked);

    mainLayout->addStretch();
}

void MotorParamWidget::onAxisButtonClicked(int axisId)
{
    m_selectedAxisId = axisId;
    // 加载新轴的参数
}

void MotorParamWidget::setCurrentAxisId(int axisId)
{
    m_selectedAxisId = axisId;
    if (m_axisBtnGroup->button(axisId)) {
        m_axisBtnGroup->button(axisId)->setChecked(true);
    }
    loadAxisDefaults(axisId);
}

void MotorParamWidget::loadAxisDefaults(int axisId)
{
    Q_UNUSED(axisId);
    // 加载默认值 (外部通过 onAxisParamChanged slot 填充实际值)
    m_leadScrewSpin->setValue(DEFAULT_LEAD_SCREW);
    m_pulsePerRevSpin->setValue(DEFAULT_PULSE_PER_REV);
    m_gearRatioSpin->setValue(DEFAULT_GEAR_RATIO);
    m_maxVelocitySpin->setValue(DEFAULT_VELOCITY);
    m_accelSpin->setValue(DEFAULT_ACCEL);
    m_decelSpin->setValue(DEFAULT_DECEL);
    m_softLimitPosSpin->setValue(300.0);
    m_softLimitNegSpin->setValue(0.0);
    m_homeVelSpin->setValue(50.0);
    m_homeOffsetSpin->setValue(2.0);
}

void MotorParamWidget::onApplyClicked()
{
    MotorAxis params;
    params.axisId          = m_selectedAxisId;
    params.velocity        = m_maxVelocitySpin->value();
    params.acceleration    = m_accelSpin->value();
    params.deceleration    = m_decelSpin->value();
    params.leadScrew       = m_leadScrewSpin->value();
    params.pulsePerRev     = m_pulsePerRevSpin->value();
    params.gearRatio       = m_gearRatioSpin->value();
    params.softLimitPositive = m_softLimitPosSpin->value();
    params.softLimitNegative = m_softLimitNegSpin->value();

    emit paramsUpdateRequested(m_selectedAxisId, params);
}

void MotorParamWidget::onSaveClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "保存电机参数",
        "motor_params.json",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit saveRequested(filePath);
    }
}

void MotorParamWidget::onLoadClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "加载电机参数",
        "",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit loadRequested(filePath);
    }
}

void MotorParamWidget::onAxisParamChanged(int axisId)
{
    if (axisId != m_selectedAxisId) return;
    // 由MainWindow连接实际参数值到此slot
}
