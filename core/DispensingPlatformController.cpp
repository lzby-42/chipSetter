/**
 * @file DispensingPlatformController.cpp
 * @brief 点胶平台控制器实现
 */

#include "core/DispensingPlatformController.h"
#include "core/MotorManager.h"
#include "core/AlarmLogger.h"
#include <QDebug>

DispensingPlatformController::DispensingPlatformController(
        MotorManager *motor, AlarmLogger *alarm, QObject *parent)
    : QObject(parent)
    , m_motor(motor)
    , m_alarm(alarm)
{
}

void DispensingPlatformController::initialize()
{
    // 连接 MotorManager 信号
    connect(m_motor, &MotorManager::homeFinished,
            this, &DispensingPlatformController::onMotorHomeFinished);
    connect(m_motor, &MotorManager::moveFinished,
            this, &DispensingPlatformController::onMotorMoveFinished);
    connect(m_motor, &MotorManager::positionUpdated,
            this, &DispensingPlatformController::onMotorPositionUpdated);

    // 超时定时器 (单次触发)
    m_homeTimer = new QTimer(this);
    m_homeTimer->setSingleShot(true);
    connect(m_homeTimer, &QTimer::timeout, this, &DispensingPlatformController::onHomeTimeout);

    m_moveTimer = new QTimer(this);
    m_moveTimer->setSingleShot(true);
    connect(m_moveTimer, &QTimer::timeout, this, &DispensingPlatformController::onMoveTimeout);

    qDebug() << "[DispensingPlatform] 初始化完成, AXIS_X=" << AXIS_X << "AXIS_Y=" << AXIS_Y;
}

// ============================================================
// 回零: X → Y 序列
// ============================================================

void DispensingPlatformController::home()
{
    if (m_homeState != IDLE && m_homeState != DONE) {
        qDebug() << "[DispensingPlatform] home() 忽略 — 已在回零中, state=" << m_homeState;
        return;
    }
    if (m_pendingMask != 0) {
        qDebug() << "[DispensingPlatform] home() 忽略 — 移动中";
        return;
    }

    qDebug() << "[DispensingPlatform] 开始回零序列: X→Y";
    m_homeState = HOMING_X;
    m_homeTimer->start(HOME_TIMEOUT_MS);
    m_motor->homeRequest(AXIS_X);
}

void DispensingPlatformController::onMotorHomeFinished(int axisId, bool success, int homeStage)
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
        qDebug() << "[DispensingPlatform] X轴回零完成, 开始Y轴";
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
        qDebug() << "[DispensingPlatform] 回零序列完成";
        m_homeState = DONE;
        m_homeTimer->stop();
        emit homeFinished(true);
        return;
    }
}

// ============================================================
// 双轴并发移动 (非插补)
// ============================================================

void DispensingPlatformController::moveTo(double xMm, double yMm)
{
    if (m_homeState == HOMING_X || m_homeState == HOMING_Y) {
        qDebug() << "[DispensingPlatform] moveTo() 忽略 — 回零中";
        return;
    }
    if (m_pendingMask != 0) {
        qDebug() << "[DispensingPlatform] moveTo() 忽略 — 上一次移动未完成";
        return;
    }

    qDebug() << "[DispensingPlatform] moveTo(" << xMm << "," << yMm << ")";

    const MotorAxis& axisX = m_motor->axisState(AXIS_X);
    const MotorAxis& axisY = m_motor->axisState(AXIS_Y);

    m_pendingMask = (1 << 0) | (1 << 1);  // X=bit0, Y=bit1
    m_moveFailed = false;

    m_moveTimer->start(MOVE_TIMEOUT_MS);
    m_motor->moveRequest(AXIS_X, xMm, axisX.velocity, axisX.acceleration, axisX.deceleration);
    m_motor->moveRequest(AXIS_Y, yMm, axisY.velocity, axisY.acceleration, axisY.deceleration);
}

void DispensingPlatformController::onMotorMoveFinished(int axisId, bool success)
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
        qWarning() << "[DispensingPlatform] 轴" << axisId << "移动失败";
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

void DispensingPlatformController::stop()
{
    qDebug() << "[DispensingPlatform] stop()";
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

bool DispensingPlatformController::isHomed() const
{
    return m_homeState == DONE;
}

bool DispensingPlatformController::isMoving() const
{
    return m_pendingMask != 0
        || m_homeState == HOMING_X
        || m_homeState == HOMING_Y;
}

double DispensingPlatformController::currentX() const
{
    return m_currentX;
}

double DispensingPlatformController::currentY() const
{
    return m_currentY;
}

// ============================================================
// 位置更新
// ============================================================

void DispensingPlatformController::onMotorPositionUpdated(int axisId, double position)
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

void DispensingPlatformController::reportError(const QString &action, const QString &detail)
{
    m_alarm->raiseAlarm(ALARM_LEVEL_WARNING,
                        QStringLiteral("点胶平台"),
                        QStringLiteral("%1: %2").arg(action, detail));
}

// ============================================================
// 超时处理
// ============================================================

void DispensingPlatformController::onHomeTimeout()
{
    if (m_homeState == IDLE || m_homeState == DONE)
        return;  // 已通过正常路径完成

    qWarning() << "[DispensingPlatform] 回零超时! state=" << m_homeState;
    reportError("回零超时", QStringLiteral("state=%1, 轴%2未在%3ms内完成回零")
                .arg(m_homeState)
                .arg(m_homeState == HOMING_X ? "X" : "Y")
                .arg(HOME_TIMEOUT_MS));
    stop();
    m_homeState = IDLE;
    emit homeFinished(false);
}

void DispensingPlatformController::onMoveTimeout()
{
    if (m_pendingMask == 0)
        return;  // 已通过正常路径完成

    qWarning() << "[DispensingPlatform] 移动超时! pendingMask=" << m_pendingMask;
    reportError("移动超时", QStringLiteral("X/Y轴未在%1ms内完成移动").arg(MOVE_TIMEOUT_MS));
    stop();
    emit moveFinished(false);
}
