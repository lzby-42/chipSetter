/**
 * @file MotorParamWidget.h
 * @brief 电机参数管理面板 — 参数表格、写值、保存/加载文件
 *
 * 职责:
 *   - 显示当前选中轴的完整参数表
 *   - 编辑参数值
 *   - 保存到JSON文件
 *   - 从JSON文件加载
 */

#ifndef MOTORPARAMWIDGET_H
#define MOTORPARAMWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QButtonGroup>
#include "models/MotorAxis.h"

class MotorParamWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorParamWidget(QWidget *parent = nullptr);
    ~MotorParamWidget();

public slots:
    void onAxisParamChanged(int axisId);        // 接收Core层参数更新
    void setCurrentAxisId(int axisId);          // 切换轴

signals:
    void paramsUpdateRequested(int axisId, const MotorAxis& params);
    void saveRequested(const QString& filePath);
    void loadRequested(const QString& filePath);

private slots:
    void onAxisButtonClicked(int axisId);
    void onApplyClicked();                      // 应用参数
    void onSaveClicked();                       // 保存到文件
    void onLoadClicked();                       // 从文件加载

private:
    void setupUI();
    void loadAxisDefaults(int axisId);          // 加载轴的当前参数到编辑框

    int  m_selectedAxisId;

    QButtonGroup*    m_axisBtnGroup;

    // 参数编辑控件
    QDoubleSpinBox*  m_leadScrewSpin;
    QSpinBox*        m_pulsePerRevSpin;
    QDoubleSpinBox*  m_gearRatioSpin;
    QDoubleSpinBox*  m_maxVelocitySpin;
    QDoubleSpinBox*  m_accelSpin;
    QDoubleSpinBox*  m_decelSpin;
    QDoubleSpinBox*  m_softLimitPosSpin;
    QDoubleSpinBox*  m_softLimitNegSpin;
    QDoubleSpinBox*  m_homeVelSpin;
    QDoubleSpinBox*  m_homeOffsetSpin;
};

#endif // MOTORPARAMWIDGET_H
