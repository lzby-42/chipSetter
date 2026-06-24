/**
 * @file DispensingPlatformController.h
 * @brief 点胶平台控制器 — XY回零序列 + 双轴点位移动
 *
 * 职责:
 *   - 封装点胶平台 (轴2=X, 轴3=Y) 的自动回零 (X→Y序列)
 *   - 封装双轴并发点位移动 (非插补)
 *   - 错误上报 AlarmLogger
 *
 * 依赖: MotorManager, AlarmLogger, HardwareConfig.h
 */

#ifndef DISPENSINGPLATFORMCONTROLLER_H
#define DISPENSINGPLATFORMCONTROLLER_H

#include <QObject>
#include "core/HardwareConfig.h"

class MotorManager;
class AlarmLogger;

class DispensingPlatformController : public QObject
{
    Q_OBJECT

public:
    explicit DispensingPlatformController(MotorManager *motor,
                                          AlarmLogger *alarm,
                                          QObject *parent = nullptr);

    void initialize();

    // --- 动作 ---
    void home();                          // 自动回零: X→Y 序列
    void moveTo(double xMm, double yMm); // XY 移动到绝对坐标 (并发)
    void stop();                          // 停止当前动作

    // --- 状态查询 ---
    bool isHomed() const;
    bool isMoving() const;
    double currentX() const;
    double currentY() const;

signals:
    void homeFinished(bool success);
    void moveFinished(bool success);
    void positionUpdated(double x, double y);

private slots:
    void onMotorHomeFinished(int axisId, bool success, int homeStage);
    void onMotorMoveFinished(int axisId, bool success);
    void onMotorPositionUpdated(int axisId, double position);

private:
    void reportError(const QString &action, const QString &detail);

    // 点胶平台轴
    static constexpr int AXIS_X = AXIS_DISPENSE_X;   // 2
    static constexpr int AXIS_Y = AXIS_DISPENSE_Y;   // 3

    MotorManager* m_motor;   // 注入, 不拥有
    AlarmLogger*  m_alarm;   // 注入, 不拥有

    enum HomeState { IDLE, HOMING_X, HOMING_Y, DONE };
    HomeState m_homeState = IDLE;

    int  m_pendingMoves = 0;   // 当前 moveTo 等待完成的轴数
    bool m_moveFailed = false; // 当前 moveTo 是否有轴失败

    double m_currentX = 0.0;
    double m_currentY = 0.0;
};

#endif // DISPENSINGPLATFORMCONTROLLER_H
