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
#include <QCoreApplication>
#include <QDir>

MotorManager::MotorManager(GncController* controller, QObject *parent)
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
        // 预留轴4/5/6不存在物理电机
        if (i == 3 || i == 4 || i == 5) ax.isActive = false;
        m_axes[i] = ax;
    }
    memset(m_homingActive, 0, sizeof(m_homingActive));

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

void MotorManager::loadLimitsFromController()
{
    for (int i = 1; i <= AXIS_COUNT; ++i) {
        if (!m_axes[i - 1].isActive) continue;
        long posPulse = 0, negPulse = 0;
        if (m_controller->getSoftLimit(GNC_CORE_NUM, static_cast<short>(i), posPulse, negPulse)) {
            // cfg未设限位时返回0, 保留默认宽限位 (不在cfg设限位的轴不拦)
            if (posPulse != 0 || negPulse != 0) {
                m_axes[i - 1].softLimitPositive = pulseToMm(i, posPulse);
                m_axes[i - 1].softLimitNegative = pulseToMm(i, negPulse);
            }
        }
    }
    qDebug() << "MotorManager: 从控制器读取软限位完成 (0=未设则保留默认)";
}

bool MotorManager::saveConfigToController(const QString& cfgFile)
{
    if (!m_controller || !m_controller->isConnected()) return false;
    bool ok = m_controller->saveConfig(GNC_CORE_NUM, cfgFile);
    if (ok) {
        qDebug() << "MotorManager: 参数已保存到控制器cfg →" << cfgFile;
    }
    return ok;
}

bool MotorManager::enableAxis(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return false;
    short axis = static_cast<short>(axisId);

    // 先清报警再使能 (cfg加载后轴可能处于报警状态)
    m_controller->clearStatus(GNC_CORE_NUM, axis, 1);

    bool ok = m_controller->axisOn(GNC_CORE_NUM, axis);
    if (!ok) {
        // 使能失败: 尝试断电→清报警→再使能
        m_controller->axisOff(GNC_CORE_NUM, axis);
        m_controller->clearStatus(GNC_CORE_NUM, axis, 1);
        ok = m_controller->axisOn(GNC_CORE_NUM, axis);
    }
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
    prm.pos = mmToPulse(axisId, targetPos);
    // 有导程: mm/s→pulse/ms; 无导程: 输入即pulse/ms
    if (ax.hasLeadScrew) {
        prm.vel = mmToPulse(axisId, vel) / 1000.0;
        prm.acc = mmToPulse(axisId, acc) / 1000000.0;
        prm.dec = mmToPulse(axisId, dec) / 1000000.0;
    } else {
        prm.vel = vel;      // 旋转轴: 输入即pulse/ms
        prm.acc = acc;
        prm.dec = dec;
    }
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
    // GT_Stop(平滑停止): 带减速曲线, 不下伺服, 不丢失位置
    m_controller->stopMove(GNC_CORE_NUM, static_cast<short>(axisId));
    m_axes[axisId - 1].isMoving = false;
    emit moveFinished(axisId, false);
}

void MotorManager::homeRequest(int axisId)
{
    if (axisId < 1 || axisId > AXIS_COUNT) return;

    MotorAxis& ax = m_axes[axisId - 1];
    if (!ax.isActive) return;          // 预留轴跳过

    // 模式35: 当前位置清零 (不移动)
    if (ax.homeMode == 35) {
        ax.isHomed = true;
        ax.currentPosition = 0.0;
        m_controller->zeroPosition(GNC_CORE_NUM, static_cast<short>(axisId));
        emit homeFinished(axisId, true, 0);
        emit positionUpdated(axisId, 0.0);
        qDebug() << "MotorManager: 轴" << axisId << "mode=35, 直接清零";
        return;
    }

    // 尝试使能
    if (!ax.isEnabled) {
        if (enableAxis(axisId)) {
            qDebug() << "MotorManager: 轴" << axisId << "自动使能成功";
        } else {
            qDebug() << "MotorManager: 轴" << axisId << "使能失败(可能无需使能), 继续回零";
        }
    }

    short axis = static_cast<short>(axisId);
    m_controller->clearStatus(GNC_CORE_NUM, axis, 1);

    // mode=20(HOME): 硬件捕获模式 — GTN_SetTriggerPrm + 手动Trap + GetTriggerStatusEx
    if (ax.homeMode == 20 && ax.triggerIndex > 0) {
        // Step 1: 配置Trigger — 把GPI映射为硬件捕获源 (微秒级锁存)
        TTriggerPrm triggerPrm;
        memset(&triggerPrm, 0, sizeof(triggerPrm));
        triggerPrm.latchType  = 1;    // MC_ENCODER
        triggerPrm.latchIndex = 1;
        triggerPrm.probeType  = 3;    // CAPTURE_PROBE
        triggerPrm.probeIndex = static_cast<short>(ax.triggerIndex);
        triggerPrm.sense      = static_cast<short>(ax.homeEdge);
        triggerPrm.loop       = 0;
        if (!m_controller->setTriggerPrm(axis, triggerPrm)) {
            qWarning() << "MotorManager: 轴" << axisId << "Trigger配置失败";
            emit homeFinished(axisId, false, -1);
            return;
        }

        // Step 2: Jog模式 — 持续匀速往搜索方向运动 (不设目标位置)
        double hv;
        if (ax.hasLeadScrew) {
            hv = ax.homeVelocity > 0 ? mmToPulse(axisId, ax.homeVelocity) / 1000.0 : 10.0;
        } else {
            hv = ax.homeVelocity > 0 ? ax.homeVelocity : 10.0;
        }

        TJogPrm jogPrm;
        memset(&jogPrm, 0, sizeof(jogPrm));
        jogPrm.acc = 0.1;
        jogPrm.dec = 0.1;
        if (!m_controller->startJog(axis, hv, jogPrm)) {
            qWarning() << "MotorManager: 轴" << axisId << "Jog启动失败";
            emit homeFinished(axisId, false, -1);
            return;
        }

        ax.isHomed = false;
        ax.isMoving = true;
        m_homingActive[axisId - 1] = true;
        qDebug() << "MotorManager: 轴" << axisId << "IO捕获回零 GPI=" << ax.triggerIndex
                 << " dir=" << ax.homeDir << " edge=" << ax.homeEdge;
        return;
    }

    // mode=10(LIMIT): GTN_GoHome — 使用cfg配置的限位开关
    THomePrm prm;
    memset(&prm, 0, sizeof(prm));
    prm.mode          = 10;   // HOME_MODE_LIMIT
    prm.moveDir       = static_cast<short>(ax.homeDir);
    prm.edge          = static_cast<short>(ax.homeEdge);
    prm.triggerIndex  = -1;   // 使用本轴cfg限位
    double hv;
    if (ax.hasLeadScrew) {
        hv = ax.homeVelocity > 0 ? mmToPulse(axisId, ax.homeVelocity) / 1000.0 : 10.0;
    } else {
        hv = ax.homeVelocity > 0 ? ax.homeVelocity : 10.0;
    }
    prm.velHigh       = hv;
    prm.velLow        = hv * 0.5;
    prm.acc           = 0.1;
    prm.dec           = 0.1;
    prm.homeOffset    = static_cast<long>(mmToPulse(axisId, ax.homeOffset));
    prm.searchHomeDistance    = 0;
    prm.searchIndexDistance   = 0;
    prm.escapeStep    = 1;

    bool ok = m_controller->executeHome(GNC_CORE_NUM, axis, prm);
    if (ok) {
        ax.isHomed = false;
        m_homingActive[axisId - 1] = true;
        qDebug() << "MotorManager: 轴" << axisId << "开始回零 mode=10 dir=" << prm.moveDir;
    } else {
        qWarning() << "MotorManager: 轴" << axisId << "回零命令发送失败";
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
    if (!ax.isActive) return false;   // 预留轴跳过

    ax.velocity           = params.velocity;
    ax.acceleration       = params.acceleration;
    ax.deceleration       = params.deceleration;
    ax.leadScrew          = params.leadScrew;
    ax.pulsePerRev        = params.pulsePerRev;
    ax.gearRatio          = params.gearRatio;
    ax.softLimitPositive  = params.softLimitPositive;
    ax.softLimitNegative  = params.softLimitNegative;
    ax.jogStep            = params.jogStep;
    ax.homeVelocity       = params.homeVelocity;
    ax.homeOffset         = params.homeOffset;
    ax.homeDir            = params.homeDir;
    ax.homeEdge           = params.homeEdge;
    ax.homeMode           = params.homeMode;
    ax.triggerIndex       = params.triggerIndex;
    ax.hasLeadScrew       = params.hasLeadScrew;
    ax.hasSoftLimitPositive = params.hasSoftLimitPositive;
    ax.hasSoftLimitNegative = params.hasSoftLimitNegative;

    // 同步到GNC软限位 (非活跃轴或有标志位才写)
    if (ax.hasSoftLimitPositive || ax.hasSoftLimitNegative) {
        long posPulse = ax.hasSoftLimitPositive
            ? mmToPulse(axisId, ax.softLimitPositive)
            :  2147483647L;
        long negPulse = ax.hasSoftLimitNegative
            ? mmToPulse(axisId, ax.softLimitNegative)
            : -2147483647L;
        m_controller->setSoftLimit(GNC_CORE_NUM, static_cast<short>(axisId), posPulse, negPulse);
    }

    emit axisParamChanged(axisId);
    return true;
}

// ---- JSON 序列化/反序列化 ----

QJsonObject MotorManager::axisToJson(const MotorAxis& ax) const
{
    QJsonObject obj;
    obj["axisId"]           = ax.axisId;
    obj["velocity"]         = ax.velocity;
    obj["acceleration"]     = ax.acceleration;
    obj["deceleration"]     = ax.deceleration;
    obj["jogStep"]          = ax.jogStep;
    obj["leadScrew"]        = ax.leadScrew;
    obj["pulsePerRev"]      = ax.pulsePerRev;
    obj["gearRatio"]        = ax.gearRatio;
    obj["softLimitPositive"] = ax.softLimitPositive;
    obj["softLimitNegative"] = ax.softLimitNegative;
    obj["homeVelocity"]     = ax.homeVelocity;
    obj["homeOffset"]       = ax.homeOffset;
    obj["hasLeadScrew"]           = ax.hasLeadScrew;
    obj["hasSoftLimitPositive"]   = ax.hasSoftLimitPositive;
    obj["hasSoftLimitNegative"]   = ax.hasSoftLimitNegative;
    obj["homeDir"]        = ax.homeDir;
    obj["homeEdge"]       = ax.homeEdge;
    obj["homeMode"]       = ax.homeMode;
    obj["triggerIndex"]   = ax.triggerIndex;
    obj["isActive"]       = ax.isActive;
    return obj;
}

void MotorManager::jsonToAxis(const QJsonObject& obj, MotorAxis& ax)
{
    ax.axisId            = obj["axisId"].toInt(ax.axisId);
    ax.velocity          = obj["velocity"].toDouble(DEFAULT_VELOCITY);
    ax.acceleration      = obj["acceleration"].toDouble(DEFAULT_ACCEL);
    ax.deceleration      = obj["deceleration"].toDouble(DEFAULT_DECEL);
    ax.jogStep           = obj["jogStep"].toDouble(DEFAULT_JOG_STEP);
    ax.leadScrew         = obj["leadScrew"].toDouble(DEFAULT_LEAD_SCREW);
    ax.pulsePerRev       = obj["pulsePerRev"].toInt(DEFAULT_PULSE_PER_REV);
    ax.gearRatio         = obj["gearRatio"].toDouble(DEFAULT_GEAR_RATIO);
    ax.softLimitPositive = obj["softLimitPositive"].toDouble(9999.0);
    ax.softLimitNegative = obj["softLimitNegative"].toDouble(-9999.0);
    ax.homeVelocity      = obj["homeVelocity"].toDouble(10.0);
    ax.homeOffset        = obj["homeOffset"].toDouble(0.0);
    ax.hasLeadScrew            = obj["hasLeadScrew"].toBool(true);
    ax.hasSoftLimitPositive    = obj["hasSoftLimitPositive"].toBool(true);
    ax.hasSoftLimitNegative    = obj["hasSoftLimitNegative"].toBool(true);
    ax.homeDir         = obj["homeDir"].toInt(1);
    ax.homeEdge        = obj["homeEdge"].toInt(0);
    ax.homeMode        = obj["homeMode"].toInt(10);
    ax.triggerIndex    = obj["triggerIndex"].toInt(-1);
    ax.isActive        = obj["isActive"].toBool(ax.axisId != 4 && ax.axisId != 5 && ax.axisId != 6);
}

// ---- 自动加载/保存 ----

QString MotorManager::configFilePath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(MOTOR_PARAMS_FILE);
}

bool MotorManager::atomicWrite(const QString& filePath, const QByteArray& data)
{
    QString tmpPath = filePath + ".tmp";
    // 1. 写入临时文件
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MotorManager: 无法写入临时文件" << tmpPath;
        return false;
    }
    qint64 written = tmpFile.write(data);
    tmpFile.close();
    if (written != data.size()) {
        qWarning() << "MotorManager: 临时文件写入不完整" << written << "/" << data.size();
        QFile::remove(tmpPath);
        return false;
    }
    // 2. 原子替换 (NTFS上rename是原子的)
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    if (!QFile::rename(tmpPath, filePath)) {
        qWarning() << "MotorManager: rename失败" << tmpPath << "→" << filePath;
        return false;
    }
    qDebug() << "MotorManager: 参数已保存 →" << filePath;
    return true;
}

bool MotorManager::autoLoad()
{
    QString path = configFilePath();
    if (!QFile::exists(path)) {
        qDebug() << "MotorManager: 配置文件不存在, 生成默认参数 →" << path;
        autoSave();   // 用当前内存参数 (含cfg读取的软限位) 生成JSON
        return true;
    }
    return importFromFile(path);
}

bool MotorManager::autoSave()
{
    QJsonObject root;
    QJsonArray axesArr;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        axesArr.append(axisToJson(m_axes[i]));
    }
    root["axes"] = axesArr;
    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return atomicWrite(configFilePath(), data);
}

// ---- 导入/导出 (手动) ----

bool MotorManager::importFromFile(const QString& filePath)
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
        MotorAxis ax = m_axes[i];  // 从当前值开始 (保留未序列化的字段)
        jsonToAxis(axesArr[i].toObject(), ax);
        updateAxisParams(i + 1, ax);
    }
    qDebug() << "MotorManager: 参数导入完成, 共" << axesArr.size() << "个轴 →" << filePath;
    return true;
}

bool MotorManager::exportToFile(const QString& filePath)
{
    QJsonObject root;
    QJsonArray axesArr;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        axesArr.append(axisToJson(m_axes[i]));
    }
    root["axes"] = axesArr;
    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return atomicWrite(filePath, data);
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
        if (!ax.isActive) continue;           // 跳过预留轴
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

        // 回零状态 — 只在主动触发回零期间检查
        if (ax.isHomed == false && m_homingActive[axisId - 1]) {
            if (ax.homeMode == 20) {
                // IO捕获模式: 等待硬件锁存 → Stop → ZeroPos
                TTriggerStatusEx trigSts;
                if (m_controller->getTriggerStatus(axisId, trigSts) && (trigSts.execute || trigSts.done)) {
                    m_controller->stopMove(GNC_CORE_NUM, axisId);
                    ax.isHomed = true;
                    ax.isMoving = false;
                    m_homingActive[axisId - 1] = false;
                    ax.currentPosition = 0.0;
                    m_controller->zeroPosition(GNC_CORE_NUM, axisId);
                    emit homeFinished(i + 1, true, 0);
                    emit positionUpdated(i + 1, 0.0);
                    qDebug() << "MotorManager: 轴" << axisId << "IO捕获成功! latchPos=" << trigSts.position;
                } else if (!(status & 0x400) && ax.isMoving) {
                    ax.isMoving = false;
                    m_homingActive[axisId - 1] = false;
                    qWarning() << "MotorManager: 轴" << axisId << "IO回零失败: 运动停止但未捕获";
                    emit homeFinished(i + 1, false, -1);
                } else {
                    // 运动中, 每500ms报告一次等待状态
                    static int tickCount = 0;
                    if (++tickCount % 10 == 0) {  // 10 ticks = 500ms
                        qDebug() << "MotorManager: 轴" << axisId << "IO回零搜索中... execute="
                                 << trigSts.execute << " done=" << trigSts.done;
                    }
                }
            } else {
                // mode=10(LIMIT): GTN_GoHome状态检测
                THomeStatus homeSts;
                m_controller->getHomeStatus(GNC_CORE_NUM, axisId, homeSts);
                if (homeSts.run == 0 && homeSts.error == 0) {
                    ax.isHomed = true;
                    ax.isMoving = false;
                    m_homingActive[axisId - 1] = false;
                    ax.currentPosition = 0.0;
                    emit homeFinished(i + 1, true, homeSts.stage);
                    emit positionUpdated(i + 1, 0.0);
                    qDebug() << "MotorManager: 轴" << axisId << "回零完成";
                } else if (homeSts.error != 0 && homeSts.run == 0) {
                    ax.isMoving = false;
                    m_homingActive[axisId - 1] = false;
                    qWarning() << "MotorManager: 轴" << axisId << "回零失败 error=" << homeSts.error << " stage=" << homeSts.stage;
                    emit homeFinished(i + 1, false, homeSts.stage);
                }
            }
        }

        // 检测点位运动完成 (只在非回零期间)
        if (!m_homingActive[axisId - 1]) {
            bool moving = (status & 0x400) != 0;
            if (ax.isMoving && !moving) {
                ax.isMoving = false;
                ax.currentPosition = ax.targetPosition;
                emit moveFinished(i + 1, true);
                emit positionUpdated(i + 1, ax.currentPosition);
            }
            ax.isMoving = moving;
        }

        ax.rawStatus = status;
        emit axisStatusChanged(i + 1, status);
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
    // 软限位检查 (分别检查正负向软限位)
    if (ax.hasSoftLimitPositive && targetPos > ax.softLimitPositive) {
        qWarning() << "MotorManager: 轴" << axisId << "目标位置" << targetPos
                   << "超出正向软限位" << ax.softLimitPositive;
        return false;
    }
    if (ax.hasSoftLimitNegative && targetPos < ax.softLimitNegative) {
        qWarning() << "MotorManager: 轴" << axisId << "目标位置" << targetPos
                   << "超出负向软限位" << ax.softLimitNegative;
        return false;
    }
    return true;
}

// ---- 单位换算 ----

double MotorManager::mmToPulse(int axisId, double mm) const
{
    const MotorAxis& ax = m_axes[axisId - 1];
    if (!ax.hasLeadScrew || ax.leadScrew == 0 || ax.pulsePerRev == 0) return mm;
    return mm / ax.leadScrew * ax.pulsePerRev * ax.gearRatio;
}

double MotorManager::pulseToMm(int axisId, double pulse) const
{
    const MotorAxis& ax = m_axes[axisId - 1];
    if (!ax.hasLeadScrew || ax.pulsePerRev == 0 || ax.gearRatio == 0) return pulse;
    return pulse * ax.leadScrew / (ax.pulsePerRev * ax.gearRatio);
}

// ---- 诊断 ----
QString MotorManager::diagnoseAxis(int axisId) const
{
    if (axisId < 1 || axisId > AXIS_COUNT) return QString("Invalid axis %1").arg(axisId);

    const MotorAxis& ax = m_axes[axisId - 1];
    QString r;

    // 1. 轴基础参数
    r += QString("=== Axis %1 (%2) ===\n").arg(axisId).arg(ax.name);
    r += QString("Enabled: %1 | Moving: %2 | Homed: %3 | Alarm: %4\n")
         .arg(ax.isEnabled).arg(ax.isMoving).arg(ax.isHomed).arg(ax.isAlarm);
    r += QString("SoftLimit: [%1, %2] mm\n")
         .arg(ax.softLimitNegative).arg(ax.softLimitPositive);
    r += QString("LeadScrew: %1 mm/rev | PulsePerRev: %2 | GearRatio: %3\n")
         .arg(ax.leadScrew).arg(ax.pulsePerRev).arg(ax.gearRatio);
    r += QString("Velocity: %1 mm/s | Accel: %2 | Decel: %3\n")
         .arg(ax.velocity).arg(ax.acceleration).arg(ax.deceleration);

    // 2. GNC 原始状态
    short s = static_cast<short>(axisId);
    long status = 0;
    unsigned long clock = 0;
    if (m_controller->getAxisStatus(GNC_CORE_NUM, s, status, clock)) {
        r += QString("GNC Status: 0x%1 (clock=%2)\n").arg(status, 0, 16).arg(clock);
        r += QString("  bit0(Alarm): %1  bit1(PosLimit): %2  bit2(NegLimit): %3\n")
             .arg((status>>0)&1).arg((status>>1)&1).arg((status>>2)&1);
        r += QString("  bit3(Home): %1  bit4(ServoOn): %2  bit10(Motion): %3\n")
             .arg((status>>3)&1).arg((status>>4)&1).arg((status>>10)&1);
    } else {
        r += "GNC Status: READ FAILED\n";
    }

    // 3. 规划位置 (脉冲指令位置)
    double prfPos = 0;
    clock = 0;
    if (m_controller->getProfilePosition(GNC_CORE_NUM, s, prfPos, clock)) {
        double prfMm = pulseToMm(axisId, prfPos);
        r += QString("Profile Pos: %1 pulse (%2 mm)\n").arg(prfPos, 0, 'f', 1).arg(prfMm, 0, 'f', 3);
    } else {
        r += "Profile Pos: READ FAILED\n";
    }

    // 4. 编码器位置 (判断是否有编码器)
    double encPos = -9999;
    clock = 0;
    bool hasEncoder = m_controller->getEncoderPosition(GNC_CORE_NUM, s, encPos, clock);
    if (hasEncoder) {
        r += QString("Encoder Pos: %1 pulse (clock=%2)\n").arg(encPos, 0, 'f', 1).arg(clock);
        r += "  >>> AXIS HAS ENCODER FEEDBACK <<<\n";
    } else {
        r += "Encoder Pos: NOT AVAILABLE (no encoder or read error)\n";
        r += "  >>> AXIS IS PULSE-ONLY (STEPPER, NO ENCODER) <<<\n";
    }

    // 5. 跟随误差
    double axisErr = 0;
    clock = 0;
    if (m_controller->getAxisError(GNC_CORE_NUM, s, axisErr, clock)) {
        r += QString("Follow Error: %1 pulse\n").arg(axisErr, 0, 'f', 1);
    }

    return r;
}
