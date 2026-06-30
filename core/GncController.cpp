/**
 * @file GncController.cpp
 * @brief GTS SDK 实现 — 对接固高 GNC-C610 运动控制器
 *
 * GTS SDK (全部使用网络版GTN_ API):
 *   GTN_OpenCard(CHANNEL_RINGNET)  — 打开环网控制器 (GNC-C610)
 *   GTN_LoadConfig(core, cfgFile)   — 加载MotionStudio生成的配置
 *   GTN_GetDi(core, MC_GPI, &bits)  — 读取GPI的32位掩码
 *   GTN_SetDoBit(core, MC_GPO, idx, val) — 设置单个GPO
 *   GTN_GetSts(core, axis, &sts)    — 读取轴状态
 */
#include "GncController.h"
#include "HardwareConfig.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <cstring>

// ============================================================
// 构造/析构
// ============================================================

GncController::GncController(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_errorCount(0)
{
    qDebug() << "[Gnc] 构造 — GTS SDK"
             << "MC_GPI=" << MC_GPI << "MC_GPO=" << MC_GPO;
}

GncController::~GncController()
{
    closeCard();
}

// ============================================================
// 工具函数
// ============================================================

short GncController::gtsCall(const char* fnName, short result)
{
    if (result == 0) {
        if (m_errorCount >= 5) {
            emit hardwareError("GTS", QString("SDK调用恢复正常 (之前%1次失败)").arg(m_errorCount));
        }
        m_errorCount = 0;
        return result;
    }
    m_errorCount++;
    QString msg = QString("%1 返回错误码 %2 (第%3次)")
        .arg(fnName).arg(result).arg(m_errorCount);
    qWarning() << "[Gnc]" << msg;

    if (m_errorCount == 1 || m_errorCount % 100 == 0) {
        emit hardwareError("GTS SDK", msg);
    }
    if (m_errorCount == 5) {
        emit hardwareError("GTS SDK", QString("SDK连续%1次调用失败, 请检查硬件连接!").arg(m_errorCount));
    }
    return result;
}

// ============================================================
// 生命周期
// ============================================================

bool GncController::openCard()
{
    qDebug() << "[Gnc] GT_Open(CHANNEL_RINGNET=" << CHANNEL_RINGNET << ")...";
    short rtn = GT_Open(CHANNEL_RINGNET);
    if (gtsCall("GT_Open", rtn) != 0) {
        qCritical() << "[Gnc] 开卡失败! 错误码:" << rtn;
        emit hardwareError("GTS", QString("开卡失败! GT_Open返回%1").arg(rtn));
        return false;
    }
    GT_Reset();

    m_connected = true;
    qDebug() << "[Gnc] openCard 成功!";
    return true;
}

bool GncController::netInit(const QString& xmlFile, int overTimeSec)
{
    Q_UNUSED(xmlFile);
    Q_UNUSED(overTimeSec);
    qDebug() << "[Gnc] netInit — GTS无需XML, 后续GTN_LoadConfig加载cfg";
    return true;
}

bool GncController::loadConfig(short core, const QString& cfgFile)
{
    QStringList searchPaths;
    searchPaths << cfgFile
                << "googol/" + cfgFile
                << QFileInfo(cfgFile).fileName();

    m_configPath.clear();
    for (const QString& path : searchPaths) {
        if (QFileInfo::exists(path)) {
            m_configPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }
    if (m_configPath.isEmpty()) {
        emit hardwareError("GTS", QString("配置文件未找到! 搜索: %1").arg(searchPaths.join(", ")));
        qCritical() << "[Gnc] cfg文件未找到! 搜索:" << searchPaths;
        return false;
    }

    qDebug() << "[Gnc] GTN_LoadConfig(core=" << core << "," << m_configPath << ")...";
    QByteArray pathBytes = m_configPath.toLocal8Bit();
    short rtn = GTN_LoadConfig(core, pathBytes.data());
    if (gtsCall("GTN_LoadConfig", rtn) != 0) {
        emit hardwareError("GTS", QString("加载配置失败! 文件:%1").arg(m_configPath));
        qCritical() << "[Gnc] LoadConfig失败!" << m_configPath;
        return false;
    }
    qDebug() << "[Gnc] GTN_LoadConfig 成功!";
    return true;
}

bool GncController::closeCard()
{
    if (!m_connected) return true;
    qDebug() << "[Gnc] GT_Close...";
    GT_Close();
    m_connected = false;
    return true;
}

bool GncController::isConnected() const
{
    return m_connected;
}

// ============================================================
// 轴操作
// ============================================================

bool GncController::axisOn(short core, short axis)
{
    return gtsCall("GTN_AxisOn", GTN_AxisOn(core, axis)) == 0;
}

bool GncController::axisOff(short core, short axis)
{
    return gtsCall("GTN_AxisOff", GTN_AxisOff(core, axis)) == 0;
}

bool GncController::clearStatus(short core, short axis, short count)
{
    return gtsCall("GTN_ClrSts", GTN_ClrSts(core, axis, count)) == 0;
}

bool GncController::zeroPosition(short core, short axis)
{
    return gtsCall("GTN_ZeroPos", GTN_ZeroPos(core, axis, 1)) == 0;
}

// ============================================================
// 点位运动
// ============================================================

bool GncController::moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm)
{
    // MotionStudio Trap模式: PrfTrap → SetTrapPrm → SetVel → SetPos → Update
    short profile = axis;
    long mask = 1L << (profile - 1);

    if (gtsCall("GTN_PrfTrap", GTN_PrfTrap(core, profile)) != 0) return false;

    TTrapPrm trapPrm;
    trapPrm.acc        = prm.acc;
    trapPrm.dec        = prm.dec;
    trapPrm.velStart   = prm.velStart;
    trapPrm.smoothTime = 0;
    // 部分轴不支持SetTrapPrm, 失败不阻塞 (使用控制器默认参数)
    gtsCall("GTN_SetTrapPrm", GTN_SetTrapPrm(core, profile, &trapPrm));

    if (gtsCall("GTN_SetVel", GTN_SetVel(core, profile, prm.vel)) != 0) return false;
    if (gtsCall("GTN_SetPos", GTN_SetPos(core, profile, static_cast<long>(prm.pos))) != 0) return false;
    if (gtsCall("GTN_Update", GTN_Update(core, mask)) != 0) return false;

    qDebug() << "[Gnc] moveAbsolute axis=" << axis << " pos=" << prm.pos << "pulse";
    return true;
}

bool GncController::moveRelative(short core, short axis, double deltaPulse,
                                    double vel, double acc, double dec)
{
    // 读取当前位置 → 计算目标 → Trap点位运动
    double curPos = 0;
    unsigned long clock;
    if (gtsCall("GTN_GetPrfPos", GTN_GetPrfPos(core, axis, &curPos, 1, &clock)) != 0) {
        return false;
    }

    TMoveAbsolutePrmEx prm;
    memset(&prm, 0, sizeof(prm));
    prm.pos      = curPos + deltaPulse;
    prm.vel      = vel;
    prm.acc      = acc;
    prm.dec      = dec;
    prm.percent  = 0;
    prm.velStart = 0;
    prm.velEnd   = 0;

    qDebug() << "[Gnc] moveRelative axis=" << axis << " delta=" << deltaPulse
             << " cur=" << curPos << " target=" << prm.pos;
    return moveAbsolute(core, axis, prm);
}

bool GncController::stopMove(short core, short axis)
{
    long mask = 1L << (axis - 1);
    if (gtsCall("GTN_Stop", GTN_Stop(core, mask, 1)) != 0) return false;
    qDebug() << "[Gnc] stopMove axis=" << axis;
    return true;
}

// ============================================================
// 回零
// ============================================================

bool GncController::executeHome(short core, short axis, const THomePrm& prm)
{
    THomePrm homePrm = prm;
    short rtn = GTN_GoHome(core, axis, &homePrm);
    if (gtsCall("GTN_GoHome", rtn) != 0) return false;
    qDebug() << "[Gnc] executeHome axis=" << axis << " mode=" << prm.mode << " dir=" << prm.moveDir;
    return true;
}

bool GncController::getHomeStatus(short core, short axis, THomeStatus& sts)
{
    memset(&sts, 0, sizeof(sts));
    return gtsCall("GTN_GetHomeStatus",
                   GTN_GetHomeStatus(core, axis, &sts)) == 0;
}

bool GncController::setTriggerEx(short axis, const TTriggerEx& trigger)
{
    // 手册标准流程: 先读当前配置, 修改后写回
    TTriggerEx t;
    short rtn = GTN_GetTriggerEx(GNC_CORE_NUM, axis, &t);
    if (gtsCall("GTN_GetTriggerEx", rtn) != 0) return false;

    t.latchType     = trigger.latchType;
    t.latchIndex    = trigger.latchIndex;
    t.probeType     = trigger.probeType;
    t.probeIndex    = trigger.probeIndex;
    t.sense         = trigger.sense;
    t.loop          = trigger.loop;
    t.offset        = trigger.offset;
    t.firstPosition = trigger.firstPosition;
    t.lastPosition  = trigger.lastPosition;
    t.windowOnly    = trigger.windowOnly;

    rtn = GTN_SetTriggerEx(GNC_CORE_NUM, axis, &t);
    if (gtsCall("GTN_SetTriggerEx", rtn) != 0) {
    qDebug() << "[Gnc] setTriggerEx axis=" << axis << " probeIndex=" << trigger.probeIndex
             << " sense=" << trigger.sense;
    return false;
    }
    return true;
}

bool GncController::getTriggerStatus(short axis, TTriggerStatusEx& sts)
{
    memset(&sts, 0, sizeof(sts));
    short rtn = GTN_GetTriggerStatusEx(GNC_CORE_NUM, axis, &sts, 1);
    return gtsCall("GTN_GetTriggerStatusEx", rtn) == 0;
}

bool GncController::startJog(short axis, double vel, const TJogPrm& prm)
{
    TJogPrm p = prm;
    if (gtsCall("GTN_PrfJog", GTN_PrfJog(GNC_CORE_NUM, axis)) != 0) return false;
    if (gtsCall("GTN_SetJogPrm", GTN_SetJogPrm(GNC_CORE_NUM, axis, &p)) != 0) return false;
    if (gtsCall("GTN_SetVel", GTN_SetVel(GNC_CORE_NUM, axis, vel)) != 0) return false;
    long mask = 1L << (axis - 1);
    if (gtsCall("GTN_Update", GTN_Update(GNC_CORE_NUM, mask)) != 0) return false;
    qDebug() << "[Gnc] startJog axis=" << axis << " vel=" << vel;
    return true;
}

// ============================================================
// Event-Task — IO回零 (GPIO边沿 → 硬件急停)
// ============================================================

bool GncController::clearEventTask(short core)
{
    GTN_ClearEvent(core);
    GTN_ClearTask(core);
    GTN_ClearEventTaskLink(core);
    return true;
}

bool GncController::setupIoHomeCapture(short core, short axis, short gpiIndex,
                                        short homeEdge, short& outEventId, short& outTaskId)
{
    // 清除旧事件/任务/链接
    GTN_ClearEvent(core);
    GTN_ClearTask(core);
    GTN_ClearEventTaskLink(core);

    // 事件: GPI边沿触发 (UP=上升沿, DOWN=下降沿, 避免电平误触发)
    TEvent event;
    memset(&event, 0, sizeof(event));
    event.loop          = 1;    // 单次触发
    event.var.type      = WATCH_VAR_GPI;
    event.var.index     = gpiIndex;
    event.condition     = homeEdge ? WATCH_CONDITION_UP : WATCH_CONDITION_DOWN;
    event.value         = 0;

    short rtn = GTN_AddEvent(core, &event, &outEventId);
    if (gtsCall("GTN_AddEvent", rtn) != 0) {
        qDebug() << "[Gnc] setupIoHome: AddEvent failed, GPI=" << gpiIndex
                 << " edge=" << homeEdge;
        return false;
    }

    // 任务: 急停指定轴 (mask方式, option=0=急停)
    TTaskStop task;
    memset(&task, 0, sizeof(task));
    task.mask   = 1L << (axis - 1);
    task.option = 0;    // 0=急停(最小过冲), 1=平滑停止

    rtn = GTN_AddTask(core, TASK_STOP, &task, &outTaskId);
    if (gtsCall("GTN_AddTask", rtn) != 0) {
        qDebug() << "[Gnc] setupIoHome: AddTask TASK_STOP failed, axis=" << axis;
        return false;
    }

    // 链接
    short linkId;
    rtn = GTN_AddEventTaskLink(core, outEventId, outTaskId, &linkId);
    if (gtsCall("GTN_AddEventTaskLink", rtn) != 0) {
        qDebug() << "[Gnc] setupIoHome: AddEventTaskLink failed";
        return false;
    }

    // 使能事件检测
    rtn = GTN_EventOn(core, outEventId, 1);
    if (gtsCall("GTN_EventOn", rtn) != 0) {
        qDebug() << "[Gnc] setupIoHome: EventOn failed";
        return false;
    }

    qDebug() << "[Gnc] setupIoHome: axis=" << axis << " GPI=" << gpiIndex
             << " edge=" << homeEdge << " eventId=" << outEventId << " taskId=" << outTaskId;
    return true;
}

bool GncController::enableEvent(short core, short eventId, short count)
{
    return gtsCall("GTN_EventOn", GTN_EventOn(core, eventId, count)) == 0;
}

bool GncController::disableEvent(short core, short eventId, short count)
{
    return gtsCall("GTN_EventOff", GTN_EventOff(core, eventId, count)) == 0;
}

bool GncController::getEventStatus(short core, short eventId, TEventStatus& sts)
{
    memset(&sts, 0, sizeof(sts));
    return gtsCall("GTN_GetEventStatus",
                   GTN_GetEventStatus(core, eventId, &sts)) == 0;
}

// ============================================================
// 状态读取
// ============================================================

bool GncController::getAxisStatus(short core, short axis, long& status, unsigned long& clock)
{
    return gtsCall("GTN_GetSts", GTN_GetSts(core, axis, &status, 1, &clock)) == 0;
}

bool GncController::getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock)
{
    // 返回原始pulse, pulseToMm负责换算 (有导程→mm, 无导程→pulse直出)
    return gtsCall("GTN_GetPrfPos", GTN_GetPrfPos(core, axis, &prfPos, 1, &clock)) == 0;
}

bool GncController::getEncoderPosition(short core, short axis, double& encPos, unsigned long& clock)
{
    return gtsCall("GTN_GetAxisEncPos",
                   GTN_GetAxisEncPos(core, axis, &encPos, 1, &clock)) == 0;
}

bool GncController::getAxisError(short core, short axis, double& error, unsigned long& clock)
{
    return gtsCall("GTN_GetAxisError",
                   GTN_GetAxisError(core, axis, &error, 1, &clock)) == 0;
}

bool GncController::getLimitInfo(short core, short axis, TLimitInfo& info)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    memset(&info, 0, sizeof(info));
    return true;
}

// ============================================================
// IO操作
// ============================================================

bool GncController::readDI(short core, short diType, short diIndex, short* values, short count)
{
    long bitmask = 0;
    short rtn = GTN_GetDi(core, diType, &bitmask);
    if (gtsCall("GTN_GetDi", rtn) != 0) {
        memset(values, 0, count * sizeof(short));
        return false;
    }

    for (short i = 0; i < count; ++i) {
        int bitIndex = diIndex - 1 + i;
        values[i] = (bitmask >> bitIndex) & 1;
    }

    static int diCallCount = 0;
    diCallCount++;
    if (diCallCount <= 3) {
        QString bits;
        for (short i = 0; i < count; ++i) bits += QString::number(values[i]);
        qDebug() << "[Gnc] GTN_GetDi #" << diCallCount
                 << "diType=" << diType << "bitmask=0x"
                 << QString::number(bitmask, 16) << "values=[" << bits << "]";
    }
    return true;
}

bool GncController::writeDO(short core, short doType, short doIndex, short* values, short count)
{
    bool allOk = true;
    for (short i = 0; i < count; ++i) {
        short rtn = GTN_SetDoBit(core, doType, doIndex + i, values[i]);
        if (gtsCall("GTN_SetDoBit", rtn) != 0) allOk = false;
    }
    return allOk;
}

// ============================================================
// 软限位
// ============================================================

bool GncController::setSoftLimit(short core, short axis, long posPulse, long negPulse)
{
    // 使用GTN_版本 (网络控制器需要core参数)
    if (gtsCall("GTN_SetSoftLimit", GTN_SetSoftLimit(core, axis, posPulse, negPulse)) != 0)
        return false;
    qDebug() << "[Gnc] setSoftLimit axis=" << axis << " pos=" << posPulse << " neg=" << negPulse << "pulse";
    return true;
}

bool GncController::getSoftLimit(short core, short axis, long& posPulse, long& negPulse)
{
    if (gtsCall("GTN_GetSoftLimit", GTN_GetSoftLimit(core, axis, &posPulse, &negPulse)) != 0)
        return false;
    return true;
}

bool GncController::saveConfig(short core, const QString& cfgFile)
{
    QByteArray path = cfgFile.toLocal8Bit();
    short rtn = GTN_SaveConfig(core, path.data());
    if (gtsCall("GTN_SaveConfig", rtn) != 0) return false;
    qDebug() << "[Gnc] saveConfig →" << cfgFile;
    return true;
}
