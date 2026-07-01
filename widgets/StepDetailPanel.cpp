/**
 * @file StepDetailPanel.cpp
 * @brief StepDetailPanel 实现
 */

#include "StepDetailPanel.h"
#include <QHBoxLayout>
#include <QFrame>

StepDetailPanel::StepDetailPanel(QWidget *parent)
    : QWidget(parent)
    , m_currentStepIndex(-1)
{
    setupUI();
}

StepDetailPanel::~StepDetailPanel() {}

void StepDetailPanel::setupUI()
{
    setObjectName("StepDetailPanel");
    setStyleSheet(
        "QWidget#StepDetailPanel {"
        "  background:#151525;"
        "  border:1px dashed #64b5f6;"
        "  border-radius:8px;"
        "}");

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(10, 8, 10, 8);
    outerLayout->setSpacing(6);

    // Title label
    m_titleLabel = new QLabel("步骤详情", this);
    m_titleLabel->setStyleSheet(
        "color:#64b5f6; font-size:11px; font-weight:bold; border:none;");
    outerLayout->addWidget(m_titleLabel);

    // Three columns
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(8);

    // --- Left column: Substep list ---
    {
        QFrame* col = new QFrame(this);
        col->setStyleSheet("QFrame{background:#1a1a30; border-radius:4px; border:none;}");
        QVBoxLayout* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(8, 8, 8, 8);
        colLayout->setSpacing(4);

        m_substepTitle = new QLabel("子步骤列表", this);
        m_substepTitle->setStyleSheet("color:#78909c; font-size:10px; border:none;");
        colLayout->addWidget(m_substepTitle);

        m_substepLayout = new QVBoxLayout();
        m_substepLayout->setSpacing(4);
        colLayout->addLayout(m_substepLayout);
        colLayout->addStretch();

        columnsLayout->addWidget(col, 1);
    }

    // --- Middle column: Step params ---
    {
        QFrame* col = new QFrame(this);
        col->setStyleSheet("QFrame{background:#1a1a30; border-radius:4px; border:none;}");
        QVBoxLayout* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(8, 8, 8, 8);
        colLayout->setSpacing(4);

        m_paramTitle = new QLabel("步骤参数", this);
        m_paramTitle->setStyleSheet("color:#78909c; font-size:10px; border:none;");
        colLayout->addWidget(m_paramTitle);

        m_paramLayout = new QGridLayout();
        m_paramLayout->setSpacing(2);
        m_paramLayout->setColumnStretch(1, 1);
        colLayout->addLayout(m_paramLayout);
        colLayout->addStretch();

        columnsLayout->addWidget(col, 1);
    }

    // --- Right column: Realtime status ---
    {
        QFrame* col = new QFrame(this);
        col->setStyleSheet("QFrame{background:#1a1a30; border-radius:4px; border:none;}");
        QVBoxLayout* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(8, 8, 8, 8);
        colLayout->setSpacing(4);

        m_realtimeTitle = new QLabel("实时状态", this);
        m_realtimeTitle->setStyleSheet("color:#78909c; font-size:10px; border:none;");
        colLayout->addWidget(m_realtimeTitle);

        m_realtimeLayout = new QGridLayout();
        m_realtimeLayout->setSpacing(2);
        m_realtimeLayout->setColumnStretch(1, 1);
        colLayout->addLayout(m_realtimeLayout);
        colLayout->addStretch();

        columnsLayout->addWidget(col, 1);
    }

    outerLayout->addLayout(columnsLayout, 1);
}

// ============================================================
// Full refresh: show detail for a step
// ============================================================

void StepDetailPanel::showStepDetail(int stepIndex,
                                      const QString& stepName,
                                      const QStringList& substeps,
                                      const QVector<int>& substepStates,
                                      const QVariantMap& params,
                                      const QVariantMap& realtimeData)
{
    m_currentStepIndex = stepIndex;

    // Update title
    QString stepChar = QString("①②③④⑤⑥⑦⑧⑨").mid(stepIndex, 1);
    m_titleLabel->setText(
        QString("步骤详情 — %1 %2").arg(stepChar, stepName));

    // Clear old content
    clearContent();

    // State marks and colors
    static const char* stateMarks[] = {"○", "◉", "✓", "✕"};
    static const char* stateColors[] = {"#666", "#ffd740", "#81c784", "#ef9a9a"};

    // --- Populate substeps ---
    for (int i = 0; i < substeps.size(); ++i) {
        int s = (i < substepStates.size()) ? substepStates[i] : 0;
        QLabel* label = new QLabel(
            QString("%1  %2").arg(stateMarks[s], substeps[i]), this);
        label->setStyleSheet(
            QString("color:%1; font-size:9px; border:none;").arg(stateColors[s]));
        m_substepLayout->addWidget(label);
        m_substepLabels.append(label);
    }

    // --- Populate params (editable) ---
    QStringList paramKeys = params.keys();
    for (int i = 0; i < paramKeys.size(); ++i) {
        QLabel* nameLabel = new QLabel(paramKeys[i], this);
        nameLabel->setStyleSheet("color:#78909c; font-size:9px; border:none;");

        QLineEdit* valueEdit = new QLineEdit(this);
        valueEdit->setText(params[paramKeys[i]].toString());
        valueEdit->setStyleSheet(
            "QLineEdit { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:2px 4px; font-size:9px; }"
            "QLineEdit:focus { border-color:#64b5f6; }");
        valueEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueEdit->setFixedWidth(80);

        // Emit signal when user edits a param
        QString pName = paramKeys[i];
        int sIdx = stepIndex;
        connect(valueEdit, &QLineEdit::editingFinished, this, [this, sIdx, pName, valueEdit]() {
            bool ok;
            double val = valueEdit->text().toDouble(&ok);
            if (ok) {
                emit paramEdited(sIdx, pName, val);
            }
        });

        m_paramLayout->addWidget(nameLabel, i, 0);
        m_paramLayout->addWidget(valueEdit, i, 1);
        m_paramValues.append(QPair<QLabel*, QLineEdit*>(nameLabel, valueEdit));
    }

    // --- Populate realtime data ---
    QStringList rtKeys = realtimeData.keys();
    for (int i = 0; i < rtKeys.size(); ++i) {
        QLabel* nameLabel = new QLabel(rtKeys[i], this);
        nameLabel->setStyleSheet("color:#78909c; font-size:9px; border:none;");

        QLabel* valueLabel = new QLabel(
            realtimeData[rtKeys[i]].toString(), this);
        valueLabel->setStyleSheet("color:#4caf50; font-size:9px; border:none;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_realtimeLayout->addWidget(nameLabel, i, 0);
        m_realtimeLayout->addWidget(valueLabel, i, 1);
        m_realtimeLabels.append(QPair<QLabel*, QLabel*>(nameLabel, valueLabel));
    }
}

// ============================================================
// Incremental updates
// ============================================================

void StepDetailPanel::updateSubstepState(int stepIndex, int substepIndex, int state)
{
    if (stepIndex != m_currentStepIndex) return;
    if (substepIndex < 0 || substepIndex >= m_substepLabels.size()) return;

    static const char* stateMarks[] = {"○", "◉", "✓", "✕"};
    static const char* stateColors[] = {"#666", "#ffd740", "#81c784", "#ef9a9a"};

    QString text = m_substepLabels[substepIndex]->text();
    int spacePos = text.indexOf(' ');
    QString rest = (spacePos >= 0) ? text.mid(spacePos + 2) : text;

    m_substepLabels[substepIndex]->setText(
        QString("%1  %2").arg(stateMarks[state], rest));
    m_substepLabels[substepIndex]->setStyleSheet(
        QString("color:%1; font-size:9px; border:none;").arg(stateColors[state]));
}

void StepDetailPanel::updateRealtimeData(int stepIndex, const QVariantMap& data)
{
    if (stepIndex != m_currentStepIndex) return;  // 只更新当前显示的步骤

    QStringList keys = data.keys();
    for (int i = 0; i < keys.size() && i < m_realtimeLabels.size(); ++i) {
        if (m_realtimeLabels[i].first->text() == keys[i]) {
            m_realtimeLabels[i].second->setText(data[keys[i]].toString());
        }
    }
}

// ============================================================
// Cleanup
// ============================================================

void StepDetailPanel::clearContent()
{
    for (auto* label : m_substepLabels) {
        m_substepLayout->removeWidget(label);
        delete label;
    }
    m_substepLabels.clear();

    for (auto& pair : m_paramValues) {
        m_paramLayout->removeWidget(pair.first);
        m_paramLayout->removeWidget(pair.second);
        delete pair.first;
        delete pair.second;
    }
    m_paramValues.clear();

    for (auto& pair : m_realtimeLabels) {
        m_realtimeLayout->removeWidget(pair.first);
        m_realtimeLayout->removeWidget(pair.second);
        delete pair.first;
        delete pair.second;
    }
    m_realtimeLabels.clear();
}
