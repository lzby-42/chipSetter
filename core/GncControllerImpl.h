/**
 * @file GncControllerImpl.h
 * @brief GTS SDK 真实硬件实现 — 对接固高 GNC-C610 运动控制器
 *
 * 仅 USE_REAL_GNC 定义时编译, 直接使用 googol/gts.h 中的类型和常量
 */

#ifndef GNCCONTROLLERIMPL_H
#define GNCCONTROLLERIMPL_H

#include "GncController.h"
#include <QTimer>

/**
 * @class GncControllerImpl
 * @brief GTS SDK 真实硬件实现
 */
class GncControllerImpl : public IGncController
{
    Q_OBJECT

public:
    explicit GncControllerImpl(QObject *parent = nullptr);
    ~GncControllerImpl() override;

    // ---- 生命周期 ----
    bool openCard() override;
    bool netInit(const QString& xmlFile, int overTimeSec = 120) override;
    bool loadConfig(short core, const QString& cfgFile) override;
    bool closeCard() override;
    bool isConnected() const override;

    // ---- 轴操作 ----
    bool axisOn(short core, short axis) override;
    bool axisOff(short core, short axis) override;
    bool clearStatus(short core, short axis, short count) override;
    bool zeroPosition(short core, short axis) override;

    // ---- 点位运动 ----
    bool moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm) override;

    // ---- 回零 ----
    bool executeHome(short core, short axis, const TStandardHomePrm& prm) override;
    bool getHomeStatus(short core, short axis, TStandardHomeStatus& sts) override;

    // ---- 状态读取 ----
    bool getAxisStatus(short core, short axis, long& status, unsigned long& clock) override;
    bool getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock) override;
    bool getLimitInfo(short core, short axis, TLimitInfo& info) override;

    // ---- IO操作 ----
    bool readDI(short core, short diType, short diIndex, short* values, short count) override;
    bool writeDO(short core, short doType, short doIndex, short* values, short count) override;

    // ---- 软限位 ----
    bool setSoftLimit(short core, short axis, double positive, double negative) override;

private:
    short gtsCall(const char* fnName, short result);

    bool    m_connected;
    QString m_configPath;
    int     m_errorCount;
};

#endif // GNCCONTROLLERIMPL_H
