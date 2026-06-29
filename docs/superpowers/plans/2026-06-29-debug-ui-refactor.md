# Debug UI Refactor — MotorControlWidget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace MotorPtpWidget + MotorParamWidget with a single MotorControlWidget

**Architecture:** New widget merges PTP motion controls and parameter grid, sharing one 13-axis button bar. Left side PTP (position, speed/accel/decel, run/stop/home/jog, clearm/enable), right side param grid (10 rows with checkboxes for lead screw, soft limit pos, soft limit neg). All existing signals preserved with same names.

**Tech Stack:** Qt 5.15.2 C++, QGridLayout, QCheckBox, QDoubleSpinBox

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Modify | `models/MotorAxis.h:26-28,47-48` | Replace hasSoftLimit with hasSoftLimitPositive + hasSoftLimitNegative |
| Modify | `core/MotorManager.cpp:239-247,270-271,289-290,473,488,495` | Update all hasSoftLimit references |
| Create | `widgets/MotorControlWidget.h` | Combined widget header — all signals/slots |
| Create | `widgets/MotorControlWidget.cpp` | Full implementation |
| Delete | `widgets/MotorPtpWidget.h` | Replaced |
| Delete | `widgets/MotorPtpWidget.cpp` | Replaced |
| Delete | `widgets/MotorParamWidget.h` | Replaced |
| Delete | `widgets/MotorParamWidget.cpp` | Replaced |
| Modify | `mainwindow.cpp:57-60,143-144,257-310` | Replace widget references |
| Modify | `chipSetter.pro` | Replace file lists |

---

### Task 1: Update MotorAxis flags

**Files:**
- Modify: `models/MotorAxis.h:26-28,47-48`

- [ ] **Step 1: Replace `hasSoftLimit` with `hasSoftLimitPositive` + `hasSoftLimitNegative`**

```cpp
// Before (line 26-27):
    bool    hasLeadScrew;           // 是否有导程 (旋转轴/曲柄=false)
    bool    hasSoftLimit;           // 是否启用软限位

// After:
    bool    hasLeadScrew;           // 是否有导程 (旋转轴/曲柄=false)
    bool    hasSoftLimitPositive;   // 是否启用正向软限位
    bool    hasSoftLimitNegative;   // 是否启用负向软限位
```

```cpp
// Before (line 47-48):
        , homeVelocity(10.0), homeOffset(0.0)
        , hasLeadScrew(true), hasSoftLimit(true)

// After:
        , homeVelocity(10.0), homeOffset(0.0)
        , hasLeadScrew(true), hasSoftLimitPositive(true), hasSoftLimitNegative(true)
```

- [ ] **Step 2: Commit**

```bash
git add models/MotorAxis.h
git commit -m "refactor: split hasSoftLimit into hasSoftLimitPositive/Negative"
```

---

### Task 2: Update MotorManager for split soft limit flags

**Files:**
- Modify: `core/MotorManager.cpp:239-247,270-271,289-290,473,488,495`

- [ ] **Step 1: Update `updateAxisParams`**

```cpp
// Replace lines 239-247:
    ax.hasLeadScrew       = params.hasLeadScrew;
    ax.hasSoftLimitPositive = params.hasSoftLimitPositive;
    ax.hasSoftLimitNegative = params.hasSoftLimitNegative;

    // 同步到GNC软限位 (仅当轴有对应软限位时)
    if (ax.hasSoftLimitPositive || ax.hasSoftLimitNegative) {
        m_controller->setSoftLimit(GNC_CORE_NUM, static_cast<short>(axisId),
                                   mmToPulse(axisId, ax.softLimitPositive),
                                   mmToPulse(axisId, ax.softLimitNegative));
    }
```

- [ ] **Step 2: Update `axisToJson`**

Replace lines 270-271:
```cpp
    obj["hasLeadScrew"]           = ax.hasLeadScrew;
    obj["hasSoftLimitPositive"]   = ax.hasSoftLimitPositive;
    obj["hasSoftLimitNegative"]   = ax.hasSoftLimitNegative;
```

- [ ] **Step 3: Update `jsonToAxis`**

Replace lines 289-290:
```cpp
    ax.hasLeadScrew            = obj["hasLeadScrew"].toBool(true);
    ax.hasSoftLimitPositive    = obj["hasSoftLimitPositive"].toBool(true);
    ax.hasSoftLimitNegative    = obj["hasSoftLimitNegative"].toBool(true);
```

- [ ] **Step 4: Update `validateMove`**

Replace line 473:
```cpp
    // Before: if (ax.hasSoftLimit) {
    // After:
    if (ax.hasSoftLimitPositive && targetPos > ax.softLimitPositive) {
        qWarning() << "MotorManager: 轴" << axisId << "目标位置" << targetPos
                   << "超出正向软限位" << ax.softLimitPositive;
        return false;
    }
    if (ax.hasSoftLimitNegative && targetPos < ax.softLimitNegative) {
        qWarning() << "MotorManager: 轴" << axisId << "目标位置" << targetPos
                   << "超出负向软限位" << ax.softLimitNegative;
        return false;
    }
```

- [ ] **Step 5: Commit**

```bash
git add core/MotorManager.cpp
git commit -m "refactor: update MotorManager for split soft limit flags"
```

---

### Task 3: Create MotorControlWidget header

**Files:**
- Create: `widgets/MotorControlWidget.h`

- [ ] **Step 1: Write header file**

```cpp
#ifndef MOTORCONTROLWIDGET_H
#define MOTORCONTROLWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QButtonGroup>
#include "models/MotorAxis.h"

class MotorManager;

class MotorControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorControlWidget(QWidget *parent = nullptr);
    ~MotorControlWidget();

    void setMotorManager(MotorManager* mgr);
    int currentAxisId() const;

public slots:
    void onPositionUpdated(int axisId, double position);
    void onMoveFinished(int axisId, bool success);
    void onHomeFinished(int axisId, bool success, int stage);
    void onAxisStatusChanged(int axisId, long status);
    void onAxisEnableChanged(int axisId, bool enabled);
    void onParamsApplied(int axisId);
    void setCurrentAxisId(int axisId);

signals:
    // Motion (from old MotorPtpWidget)
    void moveRequested(int axisId, double targetPos, double vel, double acc, double dec);
    void stopRequested(int axisId);
    void homeRequested(int axisId);
    void jogRequested(int axisId, bool positive, double step, double vel, double acc, double dec);
    void clearAlarmRequested(int axisId);
    void enableRequested(int axisId, bool enable);

    // Params (from old MotorParamWidget)
    void applyRequested(int axisId, const MotorAxis& params);
    void exportRequested(const QString& filePath);
    void importRequested(const QString& filePath);

private slots:
    void onAxisButtonClicked(int axisId);
    void onRunClicked();
    void onStopClicked();
    void onHomeClicked();
    void onJogPlusClicked();
    void onJogMinusClicked();
    void onClearAlarmClicked();
    void onEnableClicked();
    void onApplyClicked();
    void onExportClicked();
    void onImportClicked();

private:
    void setupUI();
    void loadAxisFromManager(int axisId);

    int  m_selectedAxisId;
    MotorManager* m_motor = nullptr;

    // Axis selection
    QButtonGroup*    m_axisBtnGroup;

    // PTP motion
    QLabel*          m_curPosLabel;
    QDoubleSpinBox*  m_targetPosSpin;
    QDoubleSpinBox*  m_velocitySpin;
    QDoubleSpinBox*  m_accelSpin;
    QDoubleSpinBox*  m_decelSpin;
    QDoubleSpinBox*  m_jogStepSpin;
    QPushButton*     m_runBtn;
    QPushButton*     m_stopBtn;
    QPushButton*     m_homeBtn;
    QPushButton*     m_clearAlarmBtn;
    QPushButton*     m_enableBtn;

    // Param grid
    QDoubleSpinBox*  m_leadScrewSpin;
    QSpinBox*        m_pulsePerRevSpin;
    QDoubleSpinBox*  m_gearRatioSpin;
    QDoubleSpinBox*  m_softLimitPosSpin;
    QDoubleSpinBox*  m_softLimitNegSpin;
    QDoubleSpinBox*  m_homeVelSpin;
    QDoubleSpinBox*  m_homeOffsetSpin;

    // Checkboxes
    QCheckBox*       m_leadScrewCheck;
    QCheckBox*       m_softLimitPosCheck;
    QCheckBox*       m_softLimitNegCheck;
};

#endif // MOTORCONTROLWIDGET_H
```

- [ ] **Step 2: Commit**

```bash
git add widgets/MotorControlWidget.h
git commit -m "feat: add MotorControlWidget header"
```

---

### Task 4: Create MotorControlWidget implementation

**Files:**
- Create: `widgets/MotorControlWidget.cpp`

- [ ] **Step 1: Write constructor and setupUI**

```cpp
#include "MotorControlWidget.h"
#include "core/HardwareConfig.h"
#include "core/MotorManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>

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

    // Speed/accel/decel (PTP area only, not in param grid)
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

    ptpLayout->addStretch();
    body->addLayout(ptpLayout);

    // === RIGHT: Param grid (no speed/accel/decel — those are PTP-only) ===
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

    // Checkboxes
    m_leadScrewCheck   = new QCheckBox(this);
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
    connect(m_runBtn,       &QPushButton::clicked, this, &MotorControlWidget::onRunClicked);
    connect(m_stopBtn,      &QPushButton::clicked, this, &MotorControlWidget::onStopClicked);
    connect(m_homeBtn,      &QPushButton::clicked, this, &MotorControlWidget::onHomeClicked);
    connect(jogPlusBtn,     &QPushButton::clicked, this, &MotorControlWidget::onJogPlusClicked);
    connect(jogMinusBtn,    &QPushButton::clicked, this, &MotorControlWidget::onJogMinusClicked);
    connect(m_clearAlarmBtn, &QPushButton::clicked, this, &MotorControlWidget::onClearAlarmClicked);
    connect(m_enableBtn,    &QPushButton::clicked, this, &MotorControlWidget::onEnableClicked);
    connect(applyBtn,       &QPushButton::clicked, this, &MotorControlWidget::onApplyClicked);
    connect(exportBtn,      &QPushButton::clicked, this, &MotorControlWidget::onExportClicked);
    connect(importBtn,      &QPushButton::clicked, this, &MotorControlWidget::onImportClicked);

    // Checkbox enable/disable spinbox
    connect(m_leadScrewCheck, &QCheckBox::toggled, m_leadScrewSpin, &QWidget::setEnabled);
    connect(m_softLimitPosCheck, &QCheckBox::toggled, m_softLimitPosSpin, &QWidget::setEnabled);
    connect(m_softLimitNegCheck, &QCheckBox::toggled, m_softLimitNegSpin, &QWidget::setEnabled);
}
```

- [ ] **Step 2: Write slot implementations**

```cpp
// === Axis selection ===
void MotorControlWidget::onAxisButtonClicked(int axisId)
{
    if (m_selectedAxisId != axisId && m_motor) {
        // Auto-apply on axis switch (preserve current behavior)
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
    params.axisId            = m_selectedAxisId;
    params.velocity          = m_velocitySpin->value();
    params.acceleration      = m_accelSpin->value();
    params.deceleration      = m_decelSpin->value();
    params.leadScrew         = m_leadScrewSpin->value();
    params.pulsePerRev       = m_pulsePerRevSpin->value();
    params.gearRatio         = m_gearRatioSpin->value();
    params.softLimitPositive = m_softLimitPosSpin->value();
    params.softLimitNegative = m_softLimitNegSpin->value();
    params.homeVelocity      = m_homeVelSpin->value();
    params.homeOffset        = m_homeOffsetSpin->value();
    params.hasLeadScrew      = m_leadScrewCheck->isChecked();
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

void MotorControlWidget::onMoveFinished(int axisId, bool success)    { Q_UNUSED(axisId); Q_UNUSED(success); }
void MotorControlWidget::onHomeFinished(int axisId, bool success, int stage) { Q_UNUSED(axisId); Q_UNUSED(success); Q_UNUSED(stage); }
void MotorControlWidget::onAxisStatusChanged(int axisId, long status) { Q_UNUSED(axisId); Q_UNUSED(status); }

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
```

- [ ] **Step 3: Commit**

```bash
git add widgets/MotorControlWidget.cpp
git commit -m "feat: add MotorControlWidget implementation"
```

---

### Task 5: Update mainwindow.cpp

**Files:**
- Modify: `mainwindow.cpp:57-60,143-144,257-310`

- [ ] **Step 1: Replace widget creation (lines 57-60)**

```cpp
// Before:
    m_motorPtp  = new MotorPtpWidget(this);
    m_motorPtp->setMotorManager(m_motorManager);
    m_motorParam = new MotorParamWidget(this);
    m_motorParam->setMotorManager(m_motorManager);

// After:
    m_motorControl = new MotorControlWidget(this);
    m_motorControl->setMotorManager(m_motorManager);
```

- [ ] **Step 2: Replace layout placement (lines 143-144)**

```cpp
// Before:
    leftCol->addWidget(m_motorPtp, 4);
    leftCol->addWidget(m_motorParam, 5);

// After:
    leftCol->addWidget(m_motorControl, 1);
```

- [ ] **Step 3: Replace all signal connections (lines 257-310)**

Replace `m_motorPtp` with `m_motorControl` and `m_motorParam` with `m_motorControl`:

```cpp
    // PTP signals
    connect(m_motorControl, &MotorControlWidget::moveRequested,
            m_motorManager, &MotorManager::moveRequest);
    connect(m_motorControl, &MotorControlWidget::stopRequested,
            m_motorManager, &MotorManager::stopMove);
    connect(m_motorControl, &MotorControlWidget::homeRequested,
            m_motorManager, &MotorManager::homeRequest);
    connect(m_motorControl, &MotorControlWidget::jogRequested,
            m_motorManager, &MotorManager::jogRequest);

    // Clear alarm
    connect(m_motorControl, &MotorControlWidget::clearAlarmRequested,
            this, [this](int axisId) {
        m_gncController->clearStatus(GNC_CORE_NUM, static_cast<short>(axisId), 1);
    });

    // Enable/disable with feedback
    connect(m_motorControl, &MotorControlWidget::enableRequested,
            this, [this](int axisId, bool enable) {
        bool ok;
        if (enable) ok = m_motorManager->enableAxis(axisId);
        else        ok = m_motorManager->disableAxis(axisId);
        m_motorControl->onAxisEnableChanged(axisId, enable && ok);
    });

    // MotorManager feedback
    connect(m_motorManager, &MotorManager::positionUpdated,
            m_motorControl, &MotorControlWidget::onPositionUpdated);
    connect(m_motorManager, &MotorManager::moveFinished,
            m_motorControl, &MotorControlWidget::onMoveFinished);
    connect(m_motorManager, &MotorManager::homeFinished,
            m_motorControl, &MotorControlWidget::onHomeFinished);

    // Params
    connect(m_motorControl, &MotorControlWidget::applyRequested,
            this, [this](int axisId, const MotorAxis& params) {
        m_motorManager->updateAxisParams(axisId, params);
        m_motorManager->autoSave();
    });
    connect(m_motorControl, &MotorControlWidget::exportRequested,
            m_motorManager, &MotorManager::exportToFile);
    connect(m_motorControl, &MotorControlWidget::importRequested,
            m_motorManager, &MotorManager::importFromFile);
    connect(m_motorManager, &MotorManager::axisParamChanged,
            m_motorControl, &MotorControlWidget::onParamsApplied);

    // Move triggers param axis sync
    connect(m_motorControl, &MotorControlWidget::moveRequested,
            this, [this](int axisId, double, double, double, double) {
        m_motorControl->setCurrentAxisId(axisId);
    });
```

- [ ] **Step 4: Update header includes**

In `mainwindow.h`:
```cpp
// Before:
#include "widgets/MotorPtpWidget.h"
#include "widgets/MotorParamWidget.h"

// After:
#include "widgets/MotorControlWidget.h"
```

In `mainwindow.h` member variables:
```cpp
// Before:
    MotorPtpWidget*    m_motorPtp;
    MotorParamWidget*  m_motorParam;

// After:
    MotorControlWidget* m_motorControl;
```

- [ ] **Step 5: Commit**

```bash
git add mainwindow.h mainwindow.cpp
git commit -m "refactor: replace MotorPtpWidget+MotorParamWidget with MotorControlWidget"
```

---

### Task 6: Update chipSetter.pro and delete old files

**Files:**
- Modify: `chipSetter.pro`
- Delete: `widgets/MotorPtpWidget.h`, `widgets/MotorPtpWidget.cpp`, `widgets/MotorParamWidget.h`, `widgets/MotorParamWidget.cpp`

- [ ] **Step 1: Update chipSetter.pro SOURCES**

```qmake
# Replace:
    widgets/MotorPtpWidget.cpp \
    widgets/MotorParamWidget.cpp \

# With:
    widgets/MotorControlWidget.cpp \
```

- [ ] **Step 2: Update chipSetter.pro HEADERS**

```qmake
# Replace:
    widgets/MotorPtpWidget.h \
    widgets/MotorParamWidget.h \

# With:
    widgets/MotorControlWidget.h \
```

- [ ] **Step 3: Delete old files**

```bash
rm widgets/MotorPtpWidget.h widgets/MotorPtpWidget.cpp
rm widgets/MotorParamWidget.h widgets/MotorParamWidget.cpp
```

- [ ] **Step 4: Clean and rebuild**

```bash
rm -f debug/moc_MotorPtpWidget.cpp debug/moc_MotorPtpWidget.o
rm -f debug/moc_MotorParamWidget.cpp debug/moc_MotorParamWidget.o
rm -f debug/MotorPtpWidget.o debug/MotorParamWidget.o
cd .. && export QT_PATH="/d/tool/qt/5.15.2/mingw81_32" && \
  export PATH="$QT_PATH/bin:/mingw32/bin:/d/Code_Languages/C/msys64/mingw32/bin:/usr/bin:$PATH" && \
  $QT_PATH/bin/qmake chipSetter.pro CONFIG+=debug CONFIG+=console && \
  make -f Makefile.Debug -j4
```

Expected: compile with 0 errors.

- [ ] **Step 5: Commit**

```bash
git add chipSetter.pro
git rm widgets/MotorPtpWidget.h widgets/MotorPtpWidget.cpp
git rm widgets/MotorParamWidget.h widgets/MotorParamWidget.cpp
git commit -m "refactor: remove old widgets, update .pro for MotorControlWidget"
```
