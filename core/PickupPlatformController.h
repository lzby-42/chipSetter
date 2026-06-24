/**
 * @file PickupPlatformController.h
 * @brief 取晶平台控制器 — XY回零序列 + 双轴点位移动
 *
 * 职责:
 *   - 封装取晶平台 (轴10=X, 轴11=Y) 的自动回零 (X→Y序列)
 *   - 封装双轴并发点位移动 (非插补)
 *   - 错误上报 AlarmLogger
 *
 * 注: W轴(轴12)暂不纳入控制范围
 *
 * 依赖: MotorManager, AlarmLogger, HardwareConfig.h
 */

#ifndef PICKUPPLATFORMCONTROLLER_H
#define PICKUPPLATFORMCONTROLLER_H

#include <QObject>
#include "core/HardwareConfig.h"

class MotorManager;
class AlarmLogger;

class PickupPlatformController : public QObject
{
    Q_OBJECT

public:
    explicit PickupPlatformController(MotorManager *motor,
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

    // 取晶平台轴
    static constexpr int AXIS_X = AXIS_PICKUP_X;   // 10
    static constexpr int AXIS_Y = AXIS_PICKUP_Y;   // 11

    MotorManager* m_motor;   // 注入, 不拥有
    AlarmLogger*  m_alarm;   // 注入, 不拥有

    enum HomeState { IDLE, HOMING_X, HOMING_Y, DONE };
    HomeState m_homeState = IDLE;

    int  m_pendingMoves = 0;   // 当前 moveTo 等待完成的轴数
    bool m_moveFailed = false; // 当前 moveTo 是否有轴失败

    double m_currentX = 0.0;
    double m_currentY = 0.0;
};

#endif // PICKUPPLATFORMCONTROLLER_H
