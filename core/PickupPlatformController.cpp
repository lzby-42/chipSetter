/**
 * @file PickupPlatformController.cpp
 * @brief 取晶平台控制器实现
 */

#include "core/PickupPlatformController.h"
#include "core/MotorManager.h"
#include "core/AlarmLogger.h"
#include <QDebug>

PickupPlatformController::PickupPlatformController(
        MotorManager *motor, AlarmLogger *alarm, QObject *parent)
    : QObject(parent)
    , m_motor(motor)
    , m_alarm(alarm)
{
}

void PickupPlatformController::initialize()
{
    // 连接 MotorManager 信号
    connect(m_motor, &MotorManager::homeFinished,
            this, &PickupPlatformController::onMotorHomeFinished);
    connect(m_motor, &MotorManager::moveFinished,
            this, &PickupPlatformController::onMotorMoveFinished);
    connect(m_motor, &MotorManager::positionUpdated,
            this, &PickupPlatformController::onMotorPositionUpdated);

    qDebug() << "[PickupPlatform] 初始化完成, AXIS_X=" << AXIS_X << "AXIS_Y=" << AXIS_Y;
}

// ============================================================
// 回零: X → Y 序列
// ============================================================

void PickupPlatformController::home()
{
    if (m_homeState != IDLE && m_homeState != DONE) {
        qDebug() << "[PickupPlatform] home() 忽略 — 已在回零中, state=" << m_homeState;
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[PickupPlatform] home() 忽略 — 移动中";
        return;
    }

    qDebug() << "[PickupPlatform] 开始回零序列: X→Y";
    m_homeState = HOMING_X;
    m_motor->homeRequest(AXIS_X);
}

void PickupPlatformController::onMotorHomeFinished(int axisId, bool success, int homeStage)
{
    Q_UNUSED(homeStage);

    if (m_homeState == IDLE || m_homeState == DONE)
        return;

    if (axisId == AXIS_X && m_homeState == HOMING_X) {
        if (!success) {
            reportError("X轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[PickupPlatform] X轴回零完成, 开始Y轴";
        m_homeState = HOMING_Y;
        m_motor->homeRequest(AXIS_Y);
        return;
    }

    if (axisId == AXIS_Y && m_homeState == HOMING_Y) {
        if (!success) {
            reportError("Y轴回零", QStringLiteral("home error, stage=%1").arg(homeStage));
            m_homeState = IDLE;
            emit homeFinished(false);
            return;
        }
        qDebug() << "[PickupPlatform] 回零序列完成";
        m_homeState = DONE;
        emit homeFinished(true);
        return;
    }
}

// ============================================================
// 双轴并发移动 (非插补)
// ============================================================

void PickupPlatformController::moveTo(double xMm, double yMm)
{
    if (m_homeState == HOMING_X || m_homeState == HOMING_Y) {
        qDebug() << "[PickupPlatform] moveTo() 忽略 — 回零中";
        return;
    }
    if (m_pendingMoves > 0) {
        qDebug() << "[PickupPlatform] moveTo() 忽略 — 上一次移动未完成";
        return;
    }

    qDebug() << "[PickupPlatform] moveTo(" << xMm << "," << yMm << ")";

    const MotorAxis& axisX = m_motor->axisState(AXIS_X);
    const MotorAxis& axisY = m_motor->axisState(AXIS_Y);

    m_pendingMoves = 2;
    m_moveFailed = false;

    m_motor->moveRequest(AXIS_X, xMm, axisX.velocity, axisX.acceleration, axisX.deceleration);
    m_motor->moveRequest(AXIS_Y, yMm, axisY.velocity, axisY.acceleration, axisY.deceleration);
}

void PickupPlatformController::onMotorMoveFinished(int axisId, bool success)
{
    if (m_pendingMoves == 0)
        return;

    if (axisId != AXIS_X && axisId != AXIS_Y)
        return;

    if (!success) {
        m_moveFailed = true;
        qWarning() << "[PickupPlatform] 轴" << axisId << "移动失败";
    }

    m_pendingMoves--;

    if (m_pendingMoves == 0) {
        if (m_moveFailed) {
            reportError("点位移动", "X/Y轴移动异常");
            emit moveFinished(false);
        } else {
            emit moveFinished(true);
        }
    }
}

// ============================================================
// 停止
// ============================================================

void PickupPlatformController::stop()
{
    qDebug() << "[PickupPlatform] stop()";
    m_motor->stopMove(AXIS_X);
    m_motor->stopMove(AXIS_Y);
    m_homeState = IDLE;
    m_pendingMoves = 0;
    m_moveFailed = false;
}

// ============================================================
// 状态查询
// ============================================================

bool PickupPlatformController::isHomed() const
{
    return m_homeState == DONE;
}

bool PickupPlatformController::isMoving() const
{
    return m_pendingMoves > 0
        || m_homeState == HOMING_X
        || m_homeState == HOMING_Y;
}

double PickupPlatformController::currentX() const
{
    return m_currentX;
}

double PickupPlatformController::currentY() const
{
    return m_currentY;
}

// ============================================================
// 位置更新
// ============================================================

void PickupPlatformController::onMotorPositionUpdated(int axisId, double position)
{
    if (axisId == AXIS_X) {
        m_currentX = position;
        emit positionUpdated(m_currentX, m_currentY);
    } else if (axisId == AXIS_Y) {
        m_currentY = position;
        emit positionUpdated(m_currentX, m_currentY);
    }
}

// ============================================================
// 错误上报
// ============================================================

void PickupPlatformController::reportError(const QString &action, const QString &detail)
{
    m_alarm->raiseAlarm(ALARM_LEVEL_WARNING,
                        QStringLiteral("取晶平台"),
                        QStringLiteral("%1: %2").arg(action, detail));
}
