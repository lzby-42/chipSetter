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

class MotorManager;

class MotorParamWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorParamWidget(QWidget *parent = nullptr);
    ~MotorParamWidget();

    void setMotorManager(MotorManager* mgr);    // 注入, 用于读取轴参数

public slots:
    void onParamsApplied(int axisId);           // 接收MotorManager反馈, 刷新UI
    void setCurrentAxisId(int axisId);          // 切换轴

signals:
    void applyRequested(int axisId, const MotorAxis& params);  // 应用 + 自动保存
    void exportRequested(const QString& filePath);              // 手动导出
    void importRequested(const QString& filePath);              // 手动导入

private slots:
    void onAxisButtonClicked(int axisId);
    void onApplyClicked();
    void onExportClicked();
    void onImportClicked();

private:
    void setupUI();
    void loadAxisFromManager(int axisId);       // 从MotorManager读参数到编辑框

    int  m_selectedAxisId;
    MotorManager* m_motor = nullptr;

    QButtonGroup*    m_axisBtnGroup;

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
