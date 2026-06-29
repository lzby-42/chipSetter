/**
 * @file GncController.cpp
 * @brief GTS SDK 实现 — 对接固高 GNC-C610 运动控制器
 *
 * GTS SDK 关键API:
 *   GT_Open(CHANNEL_RINGNET)  — 打开环网控制器 (GNC-C610)
 *   GT_LoadConfig(cfgFile)    — 加载MotionStudio生成的配置
 *   GT_GetDi(MC_GPI, &bits)   — 读取GPI的32位掩码 (MC_GPI=4)
 *   GT_SetDoBit(MC_GPO, idx, val) — 设置单个GPO (MC_GPO=12)
 *   GT_GetSts(axis, &sts, 1, &clock) — 读取轴状态
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
        emit hardwareError("GTS", QString("开卡失败! GT_Open返回%1, 请检查板卡/驱动/gts.dll").arg(rtn));
        return false;
    }
    qDebug() << "[Gnc] GT_Reset...";
    GT_Reset();

    m_connected = true;
    qDebug() << "[Gnc] openCard 成功!";
    return true;
}

bool GncController::netInit(const QString& xmlFile, int overTimeSec)
{
    Q_UNUSED(xmlFile);
    Q_UNUSED(overTimeSec);
    qDebug() << "[Gnc] netInit — GTS无需XML, 后续GT_LoadConfig加载cfg";
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
    if (gtsCall("GT_LoadConfig", rtn) != 0) {
        emit hardwareError("GTS", QString("加载配置失败! GT_LoadConfig返回%1, 文件:%2").arg(rtn).arg(m_configPath));
        qCritical() << "[Gnc] LoadConfig失败!" << m_configPath;
        return false;
    }
    qDebug() << "[Gnc] GT_LoadConfig 成功!";
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
    Q_UNUSED(core);
    return gtsCall("GT_AxisOn", GT_AxisOn(axis)) == 0;
}

bool GncController::axisOff(short core, short axis)
{
    Q_UNUSED(core);
    return gtsCall("GT_AxisOff", GT_AxisOff(axis)) == 0;
}

bool GncController::clearStatus(short core, short axis, short count)
{
    Q_UNUSED(core);
    return gtsCall("GT_ClrSts", GT_ClrSts(axis, count)) == 0;
}

bool GncController::zeroPosition(short core, short axis)
{
    Q_UNUSED(core);
    return gtsCall("GT_ZeroPos", GT_ZeroPos(axis, 1)) == 0;
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
    if (gtsCall("GTN_SetTrapPrm", GTN_SetTrapPrm(core, profile, &trapPrm)) != 0) return false;

    if (gtsCall("GTN_SetVel", GTN_SetVel(core, profile, prm.vel)) != 0) return false;
    if (gtsCall("GTN_SetPos", GTN_SetPos(core, profile, static_cast<long>(prm.pos))) != 0) return false;
    if (gtsCall("GTN_Update", GTN_Update(core, mask)) != 0) return false;

    qDebug() << "[Gnc] moveAbsolute axis=" << axis << " pos=" << prm.pos << "pulse";
    return true;
}

bool GncController::stopMove(short core, short axis)
{
    Q_UNUSED(core);
    long mask = 1L << (axis - 1);
    // option=0: 平滑停止 (使用 GT_SetStopDec 设定的减速度), option=1: 急停
    if (gtsCall("GT_Stop", GT_Stop(mask, 0)) != 0) return false;
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

// ============================================================
// 状态读取
// ============================================================

bool GncController::getAxisStatus(short core, short axis, long& status, unsigned long& clock)
{
    Q_UNUSED(core);
    return gtsCall("GT_GetSts", GT_GetSts(axis, &status, 1, &clock)) == 0;
}

bool GncController::getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock)
{
    Q_UNUSED(core);
    short rtn = GT_GetPrfPos(axis, &prfPos, 1, &clock);
    prfPos = prfPos / 1000.0;
    return gtsCall("GT_GetPrfPos", rtn) == 0;
}

bool GncController::getEncoderPosition(short core, short axis, double& encPos, unsigned long& clock)
{
    Q_UNUSED(core);
    short rtn = GT_GetAxisEncPos(axis, &encPos, 1, &clock);
    return gtsCall("GT_GetAxisEncPos", rtn) == 0;
}

bool GncController::getAxisError(short core, short axis, double& error, unsigned long& clock)
{
    Q_UNUSED(core);
    short rtn = GT_GetAxisError(axis, &error, 1, &clock);
    return gtsCall("GT_GetAxisError", rtn) == 0;
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
    Q_UNUSED(core);
    long bitmask = 0;
    short rtn = GT_GetDi(diType, &bitmask);
    if (gtsCall("GT_GetDi", rtn) != 0) {
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
        qDebug() << "[Gnc] GT_GetDi #" << diCallCount
                 << "diType=" << diType << "bitmask=0x"
                 << QString::number(bitmask, 16) << "values=[" << bits << "]";
    }
    return true;
}

bool GncController::writeDO(short core, short doType, short doIndex, short* values, short count)
{
    Q_UNUSED(core);
    bool allOk = true;
    for (short i = 0; i < count; ++i) {
        short rtn = GT_SetDoBit(doType, doIndex + i, values[i]);
        if (gtsCall("GT_SetDoBit", rtn) != 0) allOk = false;
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
