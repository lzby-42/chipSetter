/**
 * @file MotorParamWidget.cpp
 * @brief 电机参数管理面板 实现
 */

#include "MotorParamWidget.h"
#include "core/HardwareConfig.h"
#include "core/MotorManager.h"
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

void MotorParamWidget::setMotorManager(MotorManager* mgr)
{
    m_motor = mgr;
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
                      QWidget* editor, const QString& unit = "",
                      QCheckBox* check = nullptr) {
        int col = 0;
        if (check) {
            grid->addWidget(check, row, col++);
        }
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#90a4ae; font-size:10px;");
        grid->addWidget(lbl, row, col++);
        grid->addWidget(editor, row, col++);
        if (!unit.isEmpty()) {
            QLabel* unitLbl = new QLabel(unit, this);
            unitLbl->setStyleSheet("color:#78909c; font-size:9px;");
            grid->addWidget(unitLbl, row, col);
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

    // 勾选框
    m_leadScrewCheck = new QCheckBox(this);
    m_leadScrewCheck->setChecked(true);
    m_leadScrewCheck->setStyleSheet("QCheckBox { spacing:0px; }");
    m_leadScrewCheck->setToolTip("导程适用 (旋转轴/曲柄请取消)");

    m_softLimitCheck = new QCheckBox(this);
    m_softLimitCheck->setChecked(true);
    m_softLimitCheck->setStyleSheet("QCheckBox { spacing:0px; }");
    m_softLimitCheck->setToolTip("软限位启用");

    int r = 0;
    addRow(r++, "导程",           m_leadScrewSpin,    "mm/rev",    m_leadScrewCheck);
    addRow(r++, "每转脉冲",        m_pulsePerRevSpin,  "pulse");
    addRow(r++, "电子齿轮比",      m_gearRatioSpin,    "");
    addRow(r++, "最大速度",        m_maxVelocitySpin,  "mm/s");
    addRow(r++, "加速度",          m_accelSpin,        "mm/s2");
    addRow(r++, "减速度",          m_decelSpin,        "mm/s2");
    addRow(r++, "软限位+",         m_softLimitPosSpin, "mm",       m_softLimitCheck);
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

    QPushButton* exportBtn = new QPushButton("导出", this);
    exportBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#455a64; }");
    actionLayout->addWidget(exportBtn);

    QPushButton* importBtn = new QPushButton("导入", this);
    importBtn->setStyleSheet(
        "QPushButton { background:#4a148c; color:#ce93d8; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#6a1b9a; }");
    actionLayout->addWidget(importBtn);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    connect(applyBtn,  &QPushButton::clicked, this, &MotorParamWidget::onApplyClicked);
    connect(exportBtn, &QPushButton::clicked, this, &MotorParamWidget::onExportClicked);
    connect(importBtn, &QPushButton::clicked, this, &MotorParamWidget::onImportClicked);

    // 勾选框联动: 禁用/启用相关spinbox
    auto connectCheck = [this](QCheckBox* check, std::initializer_list<QWidget*> sps) {
        connect(check, &QCheckBox::toggled, this, [sps](bool checked) {
            for (QWidget* w : sps) w->setEnabled(checked);
        });
    };
    connectCheck(m_leadScrewCheck, {m_leadScrewSpin, m_pulsePerRevSpin, m_gearRatioSpin});
    connectCheck(m_softLimitCheck, {m_softLimitPosSpin, m_softLimitNegSpin});

    mainLayout->addStretch();
}

void MotorParamWidget::onAxisButtonClicked(int axisId)
{
    // 切轴前先保存当前轴的修改 (自动"应用")
    if (m_selectedAxisId != axisId && m_motor) {
        const MotorAxis& cur = m_motor->axisState(m_selectedAxisId);
        if (m_softLimitPosSpin->value() != cur.softLimitPositive ||
            m_softLimitNegSpin->value() != cur.softLimitNegative ||
            m_maxVelocitySpin->value() != cur.velocity ||
            m_accelSpin->value() != cur.acceleration ||
            m_decelSpin->value() != cur.deceleration) {
            onApplyClicked();  // 有改动, 自动保存
        }
    }
    m_selectedAxisId = axisId;
    loadAxisFromManager(axisId);
}

void MotorParamWidget::setCurrentAxisId(int axisId)
{
    m_selectedAxisId = axisId;
    if (m_axisBtnGroup->button(axisId)) {
        m_axisBtnGroup->button(axisId)->setChecked(true);
    }
    loadAxisFromManager(axisId);
}

void MotorParamWidget::loadAxisFromManager(int axisId)
{
    if (!m_motor) {
        // 无MotorManager时用默认值
        m_leadScrewSpin->setValue(DEFAULT_LEAD_SCREW);
        m_pulsePerRevSpin->setValue(DEFAULT_PULSE_PER_REV);
        m_gearRatioSpin->setValue(DEFAULT_GEAR_RATIO);
        m_maxVelocitySpin->setValue(DEFAULT_VELOCITY);
        m_accelSpin->setValue(DEFAULT_ACCEL);
        m_decelSpin->setValue(DEFAULT_DECEL);
        m_softLimitPosSpin->setValue(9999.0);
        m_softLimitNegSpin->setValue(-9999.0);
        m_homeVelSpin->setValue(50.0);
        m_homeOffsetSpin->setValue(2.0);
        m_leadScrewCheck->setChecked(true);
        m_softLimitCheck->setChecked(true);
        return;
    }
    const MotorAxis& ax = m_motor->axisState(axisId);
    m_leadScrewSpin->setValue(ax.leadScrew);
    m_pulsePerRevSpin->setValue(ax.pulsePerRev);
    m_gearRatioSpin->setValue(ax.gearRatio);
    m_maxVelocitySpin->setValue(ax.velocity);
    m_accelSpin->setValue(ax.acceleration);
    m_decelSpin->setValue(ax.deceleration);
    m_softLimitPosSpin->setValue(ax.softLimitPositive);
    m_softLimitNegSpin->setValue(ax.softLimitNegative);
    m_homeVelSpin->setValue(ax.homeVelocity);
    m_homeOffsetSpin->setValue(ax.homeOffset);
    m_leadScrewCheck->setChecked(ax.hasLeadScrew);
    m_softLimitCheck->setChecked(ax.hasSoftLimit);
}

void MotorParamWidget::onApplyClicked()
{
    MotorAxis params;
    params.axisId           = m_selectedAxisId;
    params.velocity         = m_maxVelocitySpin->value();
    params.acceleration     = m_accelSpin->value();
    params.deceleration     = m_decelSpin->value();
    params.leadScrew        = m_leadScrewSpin->value();
    params.pulsePerRev      = m_pulsePerRevSpin->value();
    params.gearRatio        = m_gearRatioSpin->value();
    params.softLimitPositive = m_softLimitPosSpin->value();
    params.softLimitNegative = m_softLimitNegSpin->value();
    params.homeVelocity     = m_homeVelSpin->value();
    params.homeOffset       = m_homeOffsetSpin->value();
    params.hasLeadScrew     = m_leadScrewCheck->isChecked();
    params.hasSoftLimit     = m_softLimitCheck->isChecked();

    emit applyRequested(m_selectedAxisId, params);
}

void MotorParamWidget::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "导出电机参数",
        "motor_params.json",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit exportRequested(filePath);
    }
}

void MotorParamWidget::onImportClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "导入电机参数",
        "",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit importRequested(filePath);
    }
}

void MotorParamWidget::onParamsApplied(int axisId)
{
    if (axisId != m_selectedAxisId) return;
    loadAxisFromManager(axisId);  // 读回实际值, 确认写入生效
}
