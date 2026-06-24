/**
 * @file MotorPtpWidget.h
 * @brief 电机点位运动面板 — 轴选择、位置输入、加减速、运行/停止/回零/JOG/清报警/使能
 *
 * 职责:
 *   - 13轴快捷选择
 *   - 显示当前位置/输入目标位置
 *   - 设置速度/加速度/减速度
 *   - 运行/停止/回零/微调JOG按钮
 *   - 清报警/使能失能按钮
 *
 * 纯UI: 按钮点击 → 发射信号 → MotorManager处理 → 信号返回 → 更新显示
 */

#ifndef MOTORPTPWIDGET_H
#define MOTORPTPWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include "models/MotorAxis.h"

class MotorPtpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorPtpWidget(QWidget *parent = nullptr);
    ~MotorPtpWidget();

    // ---- 获取当前选中轴 ----
    int currentAxisId() const;

public slots:
    // ---- 接收Core层更新 ----
    void onPositionUpdated(int axisId, double position);
    void onMoveFinished(int axisId, bool success);
    void onHomeFinished(int axisId, bool success, int stage);
    void onAxisStatusChanged(int axisId, long status);

signals:
    // ---- 用户操作信号 (→ MotorManager) ----
    void moveRequested(int axisId, double targetPos, double vel, double acc, double dec);
    void stopRequested(int axisId);
    void homeRequested(int axisId);
    void jogRequested(int axisId, bool positive, double step, double vel, double acc, double dec);
    void clearAlarmRequested(int axisId);                 // 清当前轴报警
    void enableRequested(int axisId, bool enable);        // 使能/失能

private slots:
    void onAxisButtonClicked(int axisId);   // 轴选择
    void onRunClicked();                    // 运行按钮
    void onStopClicked();                   // 停止按钮
    void onHomeClicked();                   // 回零按钮
    void onJogPlusClicked();                // JOG+
    void onJogMinusClicked();               // JOG-
    void onClearAlarmClicked();             // 清报警
    void onEnableClicked();                 // 使能/失能切换

private:
    void setupUI();
    void updateAxisButtons();               // 刷新轴选择按钮样式

    int             m_selectedAxisId;       // 当前选中轴 (1-based)
    double          m_currentPosition;      // 当前位置缓存

    // UI 控件
    QButtonGroup*   m_axisBtnGroup;         // 轴选择按钮组 (13个QPushButton)
    QDoubleSpinBox* m_targetPosSpin;        // 目标位置输入
    QDoubleSpinBox* m_velocitySpin;         // 速度
    QDoubleSpinBox* m_accelSpin;            // 加速度
    QDoubleSpinBox* m_decelSpin;            // 减速度
    QDoubleSpinBox* m_jogStepSpin;          // JOG步距
    QLabel*         m_curPosLabel;          // 当前位置显示
    QPushButton*    m_runBtn;
    QPushButton*    m_stopBtn;
    QPushButton*    m_homeBtn;
    QPushButton*    m_clearAlarmBtn;
    QPushButton*    m_enableBtn;

    bool            m_axisEnabled;          // 当前选中轴的使能状态
};

#endif // MOTORPTPWIDGET_H
