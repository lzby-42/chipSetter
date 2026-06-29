/**
 * @file GncController.h
 * @brief GTS SDK 封装 — 对接固高 GNC-C610 运动控制器
 *
 * 职责:
 *   - 封装 GT_Open/GT_Close/GT_LoadConfig 等SDK生命周期
 *   - 封装 GT_AxisOn/GT_ClrSts/GT_ZeroPos 等轴操作
 *   - 封装 GT_GetDi/GT_SetDoBit 等IO操作
 *   - 封装 GT_GetPrfPos/GT_GetSts 等状态读取
 *   - SDK错误追踪, 通过 hardwareError 信号上报
 */

#ifndef GNCCONTROLLER_H
#define GNCCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <cmath>

#include "googol/gts.h"

// ============================================================
// 自定义结构体 (gts.h 中无对应定义)
// ============================================================

struct TMoveAbsolutePrmEx {
    double pos;
    double vel;
    double acc;
    double dec;
    double percent;
    double velStart;
    double velEnd;
    double accStartPercent;
    double decEndPercent;
};

struct TLimitInfo {
    short hwLmtPositiveEnable;
    short hwLmtNegativeEnable;
    short swLmtPositiveEnable;
    short swLmtNegativeEnable;
    short hwLmtPositiveStatus;
    short hwLmtNegativeStatus;
    short swLmtPositiveStatus;
    short swLmtNegativeStatus;
};

// ============================================================
// GncController — GTS SDK 唯一实现
// ============================================================

class GncController : public QObject
{
    Q_OBJECT

public:
    explicit GncController(QObject *parent = nullptr);
    ~GncController() override;

    // ---- 生命周期 ----
    bool openCard();
    bool netInit(const QString& xmlFile, int overTimeSec = 120);
    bool loadConfig(short core, const QString& cfgFile);
    bool closeCard();
    bool isConnected() const;

    // ---- 轴操作 ----
    bool axisOn(short core, short axis);
    bool axisOff(short core, short axis);
    bool clearStatus(short core, short axis, short count);
    bool zeroPosition(short core, short axis);

    // ---- 点位运动 ----
    bool moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm);
    bool stopMove(short core, short axis);       // 平滑停止 (GT_Stop, 带减速)

    // ---- 回零 ----
    bool executeHome(short core, short axis, const THomePrm& prm);  // GTN_GoHome
    bool getHomeStatus(short core, short axis, THomeStatus& sts);

    // ---- 状态读取 ----
    bool getAxisStatus(short core, short axis, long& status, unsigned long& clock);
    bool getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock);
    bool getEncoderPosition(short core, short axis, double& encPos, unsigned long& clock);
    bool getAxisError(short core, short axis, double& error, unsigned long& clock);
    bool getLimitInfo(short core, short axis, TLimitInfo& info);

    // ---- IO操作 ----
    bool readDI(short core, short diType, short diIndex, short* values, short count);
    bool writeDO(short core, short doType, short doIndex, short* values, short count);

    // ---- 软限位 (pulse单位, 由MotorManager做mm→pulse换算) ----
    bool setSoftLimit(short core, short axis, long posPulse, long negPulse);
    bool getSoftLimit(short core, short axis, long& posPulse, long& negPulse);
    bool saveConfig(short core, const QString& cfgFile);  // 保存当前参数到cfg

signals:
    void hardwareError(const QString& source, const QString& message);

private:
    short gtsCall(const char* fnName, short result);

    bool    m_connected;
    QString m_configPath;
    int     m_errorCount;
};

#endif // GNCCONTROLLER_H
