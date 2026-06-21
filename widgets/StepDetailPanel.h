/**
 * @file StepDetailPanel.h
 * @brief 步骤详情面板 — 子步骤列表 + 步骤参数 + 实时状态 三栏布局
 */

#ifndef STEPDETAILPANEL_H
#define STEPDETAILPANEL_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class StepDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StepDetailPanel(QWidget *parent = nullptr);
    ~StepDetailPanel();

public slots:
    // Full refresh: switch to a different step and show all its details
    void showStepDetail(int stepIndex,
                        const QString& stepName,
                        const QStringList& substeps,
                        const QVector<int>& substepStates,
                        const QVariantMap& params,
                        const QVariantMap& realtimeData);

    // Incremental updates
    void updateSubstepState(int stepIndex, int substepIndex, int state);
    void updateRealtimeData(const QVariantMap& data);

private:
    void setupUI();
    void clearContent();

    // Title
    QLabel* m_titleLabel;

    // Left column: substeps
    QLabel*      m_substepTitle;
    QVBoxLayout* m_substepLayout;
    QVector<QLabel*> m_substepLabels;

    // Middle column: params
    QLabel*      m_paramTitle;
    QGridLayout* m_paramLayout;
    QVector<QPair<QLabel*, QLabel*>> m_paramLabels;

    // Right column: realtime status
    QLabel*      m_realtimeTitle;
    QGridLayout* m_realtimeLayout;
    QVector<QPair<QLabel*, QLabel*>> m_realtimeLabels;

    int m_currentStepIndex;
};

#endif // STEPDETAILPANEL_H
