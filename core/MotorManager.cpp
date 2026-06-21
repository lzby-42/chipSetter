/**
 * @file MotorManager.cpp
 * @brief MotorManager 实现
 */

#include "MotorManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

MotorManager::MotorManager(IGncController* controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_polling(false)
{
    // 初始化13轴
    m_axes.resize(AXIS_COUNT);
    QStringList axisNames = AXIS_NAMES;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        MotorAxis ax;
        ax.axisId = i + 1;  // 1-based
        ax.name   = (i < axisNames.size()) ? axisNames[i] : QString("轴%1").arg(i + 1);
        m_axes[i] = ax;
    }

    // 创建轮询定时器
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MotorManager::onPollTimer);
}

MotorManager::~MotorManager()
{
    stopPolling();
}

bool MotorManager::initialize()
{
    if (!m_controller || !m_controller->isConnected()) {
        qWarning() << "MotorManager: 控制器未连接, 无法初始化";
        return false;
    }
    startPolling(50);
    qDebug() << "MotorManager: 初始化完成, 轮询已启动";
    return true;
}

bool MotorManager::enableAxis(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return false;
    bool ok = m_controller->axisOn(GNC_CORE_NUM, static_cast<short>(axisId));
    if (ok) m_axes[axisId - 1].isEnabled = true;
    return ok;
}

bool MotorManager::disableAxis(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return false;
    bool ok = m_controller->axisOff(GNC_CORE_NUM, static_cast<short>(axisId));
    if (ok) m_axes[axisId - 1].isEnabled = false;
    return ok;
}

const MotorAxis& MotorManager::axisState(int axisId) const
{
    if (axisId < 1 || axisId > AXIS_COUNT) {
        static MotorAxis dummy;
        return dummy;
    }
    return m_axes[axisId - 1];
}

QVector<MotorAxis> MotorManager::allAxisStates() const
{
    return m_axes;
}

// ---- 运动请求 ----

void MotorManager::moveRequest(int axisId, double targetPos, double vel, double acc, double dec)
{
    if (!validateMove(axisId, targetPos, vel, acc, dec)) {
        emit motorError(axisId, QString("运动参数校验失败: 轴%1").arg(axisId));
        return;
    }

    MotorAxis& ax = m_axes[axisId - 1];
    ax.targetPosition = targetPos;
    ax.velocity       = vel;
    ax.acceleration   = acc;
    ax.deceleration   = dec;

    // 组装GNC运动参数
    TMoveAbsolutePrmEx prm;
    memset(&prm, 0, sizeof(prm));
    prm.pos     = mmToPulse(axisId, targetPos);
    prm.vel     = mmToPulse(axisId, vel) / 1000.0;    // mm/s → pulse/ms
    prm.acc     = mmToPulse(axisId, acc) / 1000000.0; // mm/s² → pulse/ms²
    prm.dec     = mmToPulse(axisId, dec) / 1000000.0;
    prm.percent = 0;
    prm.velStart = 0;
    prm.velEnd   = 0;

    bool ok = m_controller->moveAbsolute(GNC_CORE_NUM, static_cast<short>(axisId), prm);
    if (ok) {
        ax.isMoving = true;
        qDebug() << "MotorManager: 轴" << axisId << "开始运动 →" << targetPos << "mm";
    } else {
        emit motorError(axisId, QString("运动指令发送失败: 轴%1").arg(axisId));
    }
}

void MotorManager::stopMove(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return;
    m_controller->axisOff(GNC_CORE_NUM, static_cast<short>(axisId));
    m_axes[axisId - 1].isMoving = false;
    emit moveFinished(axisId, false);  // 中断
}

void MotorManager::homeRequest(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return;

    TStandardHomePrm prm;
    memset(&prm, 0, sizeof(prm));
    prm.mode         = 3;       // 正向搜索Home, Index边沿捕获
    prm.highSpeed    = 10.0;    // pulse/ms
    prm.lowSpeed     = 5.0;
    prm.acc          = 1.0;
    prm.offset       = 0;
    prm.check        = 0;
    prm.autoZeroPos  = 1;       // 回零后自动清零

    bool ok = m_controller->executeHome(GNC_CORE_NUM, static_cast<short>(axisId), prm);
    if (ok) {
        m_axes[axisId - 1].isHomed = false;
        qDebug() << "MotorManager: 轴" << axisId << "开始回零";
    }
}

void MotorManager::jogRequest(int axisId, bool positive, double step, double vel, double acc, double dec)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return;

    double currentPos = m_axes[axisId - 1].currentPosition;
    double targetPos  = currentPos + (positive ? step : -step);

    // JOG复用moveRequest
    moveRequest(axisId, targetPos, vel, acc, dec);
}

// ---- 信号更新 (来自IO) ----

void MotorManager::updateAxisSignals(int axisId, bool home, bool limitPos, bool limitNeg)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return;
    MotorAxis& ax = m_axes[axisId - 1];
    ax.homeSignal          = home;
    ax.limitPositiveSignal = limitPos;
    ax.limitNegativeSignal = limitNeg;
}

// ---- 参数管理 ----

bool MotorManager::updateAxisParams(int axisId, const MotorAxis& params)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return false;
    MotorAxis& ax = m_axes[axisId - 1];

    ax.velocity           = params.velocity;
    ax.acceleration       = params.acceleration;
    ax.deceleration       = params.deceleration;
    ax.leadScrew          = params.leadScrew;
    ax.pulsePerRev        = params.pulsePerRev;
    ax.gearRatio          = params.gearRatio;
    ax.softLimitPositive  = params.softLimitPositive;
    ax.softLimitNegative  = params.softLimitNegative;
    ax.jogStep            = params.jogStep;

    // 同步到GNC软限位
    m_controller->setSoftLimit(GNC_CORE_NUM, static_cast<short>(axisId),
                               mmToPulse(axisId, ax.softLimitPositive),
                               mmToPulse(axisId, ax.softLimitNegative));

    emit axisParamChanged(axisId);
    return true;
}

bool MotorManager::loadParamsFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "MotorManager: 无法打开参数文件" << filePath;
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    QJsonArray axesArr = root["axes"].toArray();

    for (int i = 0; i < axesArr.size() && i < AXIS_COUNT; ++i) {
        QJsonObject axObj = axesArr[i].toObject();
        MotorAxis ax;
        ax.axisId           = axObj["axisId"].toInt(i + 1);
        ax.velocity         = axObj["velocity"].toDouble(DEFAULT_VELOCITY);
        ax.acceleration     = axObj["acceleration"].toDouble(DEFAULT_ACCEL);
        ax.deceleration     = axObj["deceleration"].toDouble(DEFAULT_DECEL);
        ax.leadScrew        = axObj["leadScrew"].toDouble(DEFAULT_LEAD_SCREW);
        ax.pulsePerRev      = axObj["pulsePerRev"].toInt(DEFAULT_PULSE_PER_REV);
        ax.gearRatio        = axObj["gearRatio"].toDouble(DEFAULT_GEAR_RATIO);
        ax.softLimitPositive = axObj["softLimitPositive"].toDouble(300.0);
        ax.softLimitNegative = axObj["softLimitNegative"].toDouble(0.0);
        updateAxisParams(i + 1, ax);
    }
    qDebug() << "MotorManager: 参数加载完成, 共" << axesArr.size() << "个轴";
    return true;
}

bool MotorManager::saveParamsToFile(const QString& filePath)
{
    QJsonObject root;
    QJsonArray axesArr;

    for (int i = 0; i < AXIS_COUNT; ++i) {
        const MotorAxis& ax = m_axes[i];
        QJsonObject axObj;
        axObj["axisId"]           = ax.axisId;
        axObj["velocity"]         = ax.velocity;
        axObj["acceleration"]     = ax.acceleration;
        axObj["deceleration"]     = ax.deceleration;
        axObj["leadScrew"]        = ax.leadScrew;
        axObj["pulsePerRev"]      = ax.pulsePerRev;
        axObj["gearRatio"]        = ax.gearRatio;
        axObj["softLimitPositive"] = ax.softLimitPositive;
        axObj["softLimitNegative"] = ax.softLimitNegative;
        axesArr.append(axObj);
    }
    root["axes"] = axesArr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "MotorManager: 无法写入参数文件" << filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    qDebug() << "MotorManager: 参数保存完成 →" << filePath;
    return true;
}

// ---- 轮询 ----

void MotorManager::startPolling(int intervalMs)
{
    if (m_polling) return;
    m_polling = true;
    m_pollTimer->start(intervalMs);
}

void MotorManager::stopPolling()
{
    m_polling = false;
    m_pollTimer->stop();
}

void MotorManager::onPollTimer()
{
    if (!m_controller || !m_controller->isConnected()) return;

    for (int i = 0; i < AXIS_COUNT; ++i) {
        MotorAxis& ax = m_axes[i];
        short axisId  = static_cast<short>(ax.axisId);

        long status   = 0;
        double prfPos = 0.0;
        unsigned long clock = 0;

        m_controller->getAxisStatus(GNC_CORE_NUM, axisId, status, clock);
        m_controller->getProfilePosition(GNC_CORE_NUM, axisId, prfPos, clock);

        double newPosMm = pulseToMm(i + 1, prfPos);

        // 检测位置变化
        if (qAbs(newPosMm - ax.currentPosition) > 0.0001) {
            ax.currentPosition = newPosMm;
            emit positionUpdated(i + 1, ax.currentPosition);
        }

        // 检测运动完成
        bool moving = (status & 0x400) != 0;
        if (ax.isMoving && !moving) {
            ax.isMoving = false;
            ax.currentPosition = ax.targetPosition; // 修正到目标
            emit moveFinished(i + 1, true);
            emit positionUpdated(i + 1, ax.currentPosition);
        }
        ax.isMoving = moving;

        // 回零状态
        if (ax.isHomed == false) {
            TStandardHomeStatus homeSts;
            m_controller->getHomeStatus(GNC_CORE_NUM, axisId, homeSts);
            if (homeSts.run == 0 && homeSts.error == 0 && homeSts.stage == 9) {
                ax.isHomed = true;
                ax.currentPosition = 0.0;
                emit homeFinished(i + 1, true, homeSts.stage);
                emit positionUpdated(i + 1, 0.0);
            } else if (homeSts.error != 0) {
                ax.isMoving = false;
                emit homeFinished(i + 1, false, homeSts.stage);
            }
        }

        ax.rawStatus = status;
    }
}

// ---- 校验 ----

bool MotorManager::validateMove(int axisId, double targetPos, double vel, double acc, double dec)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return false;
    if (vel <= 0 || vel > MAX_VELOCITY) return false;
    if (acc <= 0 || acc > MAX_ACCEL) return false;
    if (dec <= 0 || dec > MAX_ACCEL) return false;

    const MotorAxis& ax = m_axes[axisId - 1];
    // 软限位检查
    if (targetPos > ax.softLimitPositive || targetPos < ax.softLimitNegative) {
        qWarning() << "MotorManager: 轴" << axisId << "目标位置" << targetPos
                   << "超出软限位 [" << ax.softLimitNegative << "," << ax.softLimitPositive << "]";
        return false;
    }
    return true;
}

// ---- 单位换算 ----

double MotorManager::mmToPulse(int axisId, double mm) const
{
    const MotorAxis& ax = m_axes[axisId - 1];
    if (ax.leadScrew == 0 || ax.pulsePerRev == 0) return mm;
    // pulse = mm / 导程 * 每转脉冲 * 齿轮比
    return mm / ax.leadScrew * ax.pulsePerRev * ax.gearRatio;
}

double MotorManager::pulseToMm(int axisId, double pulse) const
{
    const MotorAxis& ax = m_axes[axisId - 1];
    if (ax.pulsePerRev == 0 || ax.gearRatio == 0) return pulse;
    // mm = pulse * 导程 / (每转脉冲 * 齿轮比)
    return pulse * ax.leadScrew / (ax.pulsePerRev * ax.gearRatio);
}
