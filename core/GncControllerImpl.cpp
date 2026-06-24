/**
 * @file GncControllerImpl.cpp
 * @brief GTS SDK 真实硬件实现 (仅 USE_REAL_GNC 编译)
 *
 * GTS SDK 关键API:
 *   GT_Open(CHANNEL_RINGNET)  — 打开环网控制器 (GNC-C610)
 *   GT_LoadConfig(cfgFile)    — 加载MotionStudio生成的配置
 *   GT_GetDi(MC_GPI, &bits)   — 读取GPI的32位掩码 (MC_GPI=4)
 *   GT_SetDoBit(MC_GPO, idx, val) — 设置单个GPO (MC_GPO=12)
 *   GT_GetSts(axis, &sts, 1, &clock) — 读取轴状态
 */
#include "GncControllerImpl.h"
#include "HardwareConfig.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <cstring>

// ============================================================
// 构造/析构
// ============================================================

GncControllerImpl::GncControllerImpl(QObject *parent)
    : IGncController(parent)
    , m_connected(false)
    , m_errorCount(0)
{
    qDebug() << "[GncImpl] 构造 — 使用真实GTS SDK"
             << "MC_GPI=" << MC_GPI << "MC_GPO=" << MC_GPO;
}

GncControllerImpl::~GncControllerImpl()
{
    closeCard();
}

// ============================================================
// 工具函数
// ============================================================

short GncControllerImpl::gtsCall(const char* fnName, short result)
{
    if (result == 0) {
        if (m_errorCount >= 5) {
            // 之前有连续错误，现在恢复了
            emit hardwareError("GTS", QString("SDK调用恢复正常 (之前%1次失败)").arg(m_errorCount));
        }
        m_errorCount = 0;
        return result;
    }
    m_errorCount++;
    QString msg = QString("%1 返回错误码 %2 (第%3次)")
        .arg(fnName).arg(result).arg(m_errorCount);
    qWarning() << "[GncImpl]" << msg;

    // 首次错误 + 每100次连续错误 → 报警
    if (m_errorCount == 1 || m_errorCount % 100 == 0) {
        emit hardwareError("GTS SDK", msg);
    }
    // 连续5次错误 → 严重警告
    if (m_errorCount == 5) {
        emit hardwareError("GTS SDK", QString("SDK连续%1次调用失败, 请检查硬件连接!").arg(m_errorCount));
    }
    return result;
}

// ============================================================
// 生命周期
// ============================================================

bool GncControllerImpl::openCard()
{
    qDebug() << "[GncImpl] GT_Open(CHANNEL_RINGNET=" << CHANNEL_RINGNET << ")...";
    short rtn = GT_Open(CHANNEL_RINGNET);
    if (gtsCall("GT_Open", rtn) != 0) {
        qCritical() << "[GncImpl] 开卡失败! 错误码:" << rtn;
        emit hardwareError("GTS", QString("开卡失败! GT_Open返回%1, 请检查板卡/驱动/gts.dll").arg(rtn));
        return false;
    }
    qDebug() << "[GncImpl] GT_Reset...";
    GT_Reset();

    m_connected = true;
    qDebug() << "[GncImpl] openCard 成功!";
    return true;
}

bool GncControllerImpl::netInit(const QString& xmlFile, int overTimeSec)
{
    Q_UNUSED(xmlFile);
    Q_UNUSED(overTimeSec);
    qDebug() << "[GncImpl] netInit — GTS无需XML, 后续GT_LoadConfig加载cfg";
    return true;
}

bool GncControllerImpl::loadConfig(short core, const QString& cfgFile)
{
    Q_UNUSED(core);

    // 搜索cfg文件
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
        qCritical() << "[GncImpl] cfg文件未找到! 搜索:" << searchPaths;
        return false;
    }

    qDebug() << "[GncImpl] GT_LoadConfig(" << m_configPath << ")...";
    QByteArray pathBytes = m_configPath.toLocal8Bit();
    short rtn = GT_LoadConfig(pathBytes.data());
    if (gtsCall("GT_LoadConfig", rtn) != 0) {
        emit hardwareError("GTS", QString("加载配置失败! GT_LoadConfig返回%1, 文件:%2").arg(rtn).arg(m_configPath));
        qCritical() << "[GncImpl] LoadConfig失败!" << m_configPath;
        return false;
    }
    qDebug() << "[GncImpl] GT_LoadConfig 成功!";
    return true;
}

bool GncControllerImpl::closeCard()
{
    if (!m_connected) return true;
    qDebug() << "[GncImpl] GT_Close...";
    GT_Close();
    m_connected = false;
    return true;
}

bool GncControllerImpl::isConnected() const
{
    return m_connected;
}

// ============================================================
// 轴操作
// ============================================================

bool GncControllerImpl::axisOn(short core, short axis)
{
    Q_UNUSED(core);
    return gtsCall("GT_AxisOn", GT_AxisOn(axis)) == 0;
}

bool GncControllerImpl::axisOff(short core, short axis)
{
    Q_UNUSED(core);
    return gtsCall("GT_AxisOff", GT_AxisOff(axis)) == 0;
}

bool GncControllerImpl::clearStatus(short core, short axis, short count)
{
    Q_UNUSED(core);
    return gtsCall("GT_ClrSts", GT_ClrSts(axis, count)) == 0;
}

bool GncControllerImpl::zeroPosition(short core, short axis)
{
    Q_UNUSED(core);
    return gtsCall("GT_ZeroPos", GT_ZeroPos(axis, 1)) == 0;
}

// ============================================================
// 点位运动
// ============================================================

bool GncControllerImpl::moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm)
{
    Q_UNUSED(core);
    // GTS 点位运动: SetPos → SetVel → PrfTrap → Update
    short profile = axis;
    long mask = 1L << (profile - 1);

    long targetPulse = static_cast<long>(prm.pos * 1000.0);
    double velPulse = prm.vel * 1000.0;

    if (gtsCall("GT_SetPos", GT_SetPos(profile, targetPulse)) != 0) return false;
    if (gtsCall("GT_SetVel", GT_SetVel(profile, velPulse)) != 0) return false;
    if (gtsCall("GT_SetStopDec", GT_SetStopDec(profile, prm.dec * 1000.0, prm.dec * 2000.0)) != 0) return false;
    if (gtsCall("GT_PrfTrap", GT_PrfTrap(profile)) != 0) return false;
    if (gtsCall("GT_Update", GT_Update(mask)) != 0) return false;

    qDebug() << "[GncImpl] moveAbsolute axis=" << axis << " pos=" << prm.pos << "mm";
    return true;
}

// ============================================================
// 回零
// ============================================================

bool GncControllerImpl::executeHome(short core, short axis, const TStandardHomePrm& prm)
{
    Q_UNUSED(core);
    // 使用标准回零API GT_ExecuteStandardHome (mode=10=HOME_MODE_LIMIT 限位回零)
    TStandardHomePrm homePrm = prm;
    short rtn = GT_ExecuteStandardHome(axis, &homePrm);
    if (gtsCall("GT_ExecuteStandardHome", rtn) != 0) return false;
    qDebug() << "[GncImpl] executeHome axis=" << axis << " mode=" << prm.mode;
    return true;
}

bool GncControllerImpl::getHomeStatus(short core, short axis, TStandardHomeStatus& sts)
{
    Q_UNUSED(core);
    TStandardHomeStatus stdSts;
    memset(&stdSts, 0, sizeof(stdSts));

    if (gtsCall("GT_GetStandardHomeStatus", GT_GetStandardHomeStatus(axis, &stdSts)) != 0)
        return false;

    sts.run        = stdSts.run;
    sts.stage      = stdSts.stage;
    sts.error      = stdSts.error;
    sts.capturePos = stdSts.capturePos;
    return true;
}

// ============================================================
// 状态读取
// ============================================================

bool GncControllerImpl::getAxisStatus(short core, short axis, long& status, unsigned long& clock)
{
    Q_UNUSED(core);
    return gtsCall("GT_GetSts", GT_GetSts(axis, &status, 1, &clock)) == 0;
}

bool GncControllerImpl::getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock)
{
    Q_UNUSED(core);
    short rtn = GT_GetPrfPos(axis, &prfPos, 1, &clock);
    prfPos = prfPos / 1000.0;
    return gtsCall("GT_GetPrfPos", rtn) == 0;
}

bool GncControllerImpl::getLimitInfo(short core, short axis, TLimitInfo& info)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    memset(&info, 0, sizeof(info));
    return true;
}

// ============================================================
// IO操作 — 核心功能
// ============================================================

bool GncControllerImpl::readDI(short core, short diType, short diIndex, short* values, short count)
{
    Q_UNUSED(core);
    // GT_GetDi 返回32位掩码, 每一位代表一个通道
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
        qDebug() << "[GncImpl] GT_GetDi #" << diCallCount
                 << "diType=" << diType << "bitmask=0x"
                 << QString::number(bitmask, 16) << "values=[" << bits << "]";
    }
    return true;
}

bool GncControllerImpl::writeDO(short core, short doType, short doIndex, short* values, short count)
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

bool GncControllerImpl::setSoftLimit(short core, short axis, double positive, double negative)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    Q_UNUSED(positive);
    Q_UNUSED(negative);
    qDebug() << "[GncImpl] setSoftLimit — 运行时修改暂不支持, 请在cfg中配置";
    return true;
}
