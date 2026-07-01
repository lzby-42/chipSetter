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

    // 超时定时器 (单次触发)
    m_homeTimer = new QTimer(this);
    m_homeTimer->setSingleShot(true);
    connect(m_homeTimer, &QTimer::timeout, this, &PickupPlatformController::onHomeTimeout);

    m_moveTimer = new QTimer(this);
    m_moveTimer->setSingleShot(true);
    connect(m_moveTimer, &QTimer::timeout, this, &PickupPlatformController::onMoveTimeout);

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
    if (m_pendingMask != 0) {
        qDebug() << "[PickupPlatform] home() 忽略 — 移动中";
        return;
    }

    qDebug() << "[PickupPlatform] 开始回零序列: X→Y";
    m_homeState = HOMING_X;
    m_homeTimer->start(HOME_TIMEOUT_MS);
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
            m_homeTimer->stop();
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
            m_homeTimer->stop();
            emit homeFinished(false);
            return;
        }
        qDebug() << "[PickupPlatform] 回零序列完成";
        m_homeState = DONE;
        m_homeTimer->stop();
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
    if (m_pendingMask != 0) {
        qDebug() << "[PickupPlatform] moveTo() 忽略 — 上一次移动未完成";
        return;
    }

    qDebug() << "[PickupPlatform] moveTo(" << xMm << "," << yMm << ")";

    const MotorAxis& axisX = m_motor->axisState(AXIS_X);
    const MotorAxis& axisY = m_motor->axisState(AXIS_Y);

    m_pendingMask = (1 << 0) | (1 << 1);  // X=bit0, Y=bit1
    m_moveFailed = false;

    m_moveTimer->start(MOVE_TIMEOUT_MS);
    m_motor->moveRequest(AXIS_X, xMm, axisX.velocity, axisX.acceleration, axisX.deceleration);
    m_motor->moveRequest(AXIS_Y, yMm, axisY.velocity, axisY.acceleration, axisY.deceleration);
}

void PickupPlatformController::onMotorMoveFinished(int axisId, bool success)
{
    // 位掩码: 只处理还在等待中的轴 (防重复信号)
    int bit = -1;
    if (axisId == AXIS_X) bit = 0;
    else if (axisId == AXIS_Y) bit = 1;
    else return;

    if (!(m_pendingMask & (1 << bit)))
        return;  // 该轴已处理过

    if (!success) {
        m_moveFailed = true;
        qWarning() << "[PickupPlatform] 轴" << axisId << "移动失败";
    }

    m_pendingMask &= ~(1 << bit);  // 清除该轴位

    if (m_pendingMask == 0) {
        m_moveTimer->stop();
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
    m_pendingMask = 0;
    m_moveFailed = false;
    m_homeTimer->stop();
    m_moveTimer->stop();
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
    return m_pendingMask != 0
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

// ============================================================
// 超时处理
// ============================================================

void PickupPlatformController::onHomeTimeout()
{
    if (m_homeState == IDLE || m_homeState == DONE)
        return;

    qWarning() << "[PickupPlatform] 回零超时! state=" << m_homeState;
    reportError("回零超时", QStringLiteral("state=%1, 轴%2未在%3ms内完成回零")
                .arg(m_homeState)
                .arg(m_homeState == HOMING_X ? "X" : "Y")
                .arg(HOME_TIMEOUT_MS));
    stop();
    m_homeState = IDLE;
    emit homeFinished(false);
}

void PickupPlatformController::onMoveTimeout()
{
    if (m_pendingMask == 0)
        return;

    qWarning() << "[PickupPlatform] 移动超时! pendingMask=" << m_pendingMask;
    reportError("移动超时", QStringLiteral("X/Y轴未在%1ms内完成移动").arg(MOVE_TIMEOUT_MS));
    stop();
    emit moveFinished(false);
}
