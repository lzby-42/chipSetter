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
