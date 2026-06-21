/**
 * @file FlowStepBar.h
 * @brief 工艺流程步骤条 — 9步骤块 + 循环框 + 回路箭头
 *
 * 职责:
 *   - 显示9个步骤按钮 (①~⑨)
 *   - 用绿色QFrame包裹循环步骤(②~⑧, index 1~7)
 *   - 在循环框内绘制回路箭头
 *   - 步骤按钮颜色反映状态 (PENDING/RUNNING/COMPLETED/FAILED)
 *   - 发出 stepClicked 信号给 StepDetailPanel
 *
 * 纯UI: 不包含业务逻辑
 */

#ifndef FLOWSTEPBAR_H
#define FLOWSTEPBAR_H

#include <QWidget>
#include <QPushButton>
#include <QFrame>
#include <QLabel>
#include <QVector>
#include <QStringList>

class FlowStepBar : public QWidget
{
    Q_OBJECT

public:
    explicit FlowStepBar(QWidget *parent = nullptr);
    ~FlowStepBar();

    void setSteps(const QStringList& names);  // 设置步骤名称列表 (9项)
    int  currentStep() const;                 // 当前选中步骤

public slots:
    void setStepState(int stepIndex, int state);   // 0=PENDING,1=RUNNING,2=COMPLETED,3=FAILED
    void setCurrentStep(int stepIndex);            // 设置当前活跃步骤 (-1=无)

signals:
    void stepClicked(int stepIndex);

private:
    void setupUI();
    void updateButtonStyle(int stepIndex, int state);

    QVector<QPushButton*> m_stepButtons;   // 9个按钮
    QFrame*               m_loopFrame;     // 循环框
    int                   m_currentStep;   // 当前活跃步骤 (-1=无)

    static const int STEP_COUNT = 9;
    static const int LOOP_START = 1;       // 循环起点 index
    static const int LOOP_END   = 7;       // 循环终点 index
};

#endif // FLOWSTEPBAR_H
