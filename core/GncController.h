/**
 * @file GncController.h
 * @brief GNC运动控制器SDK封装 — 前后端隔离层
 *
 * 设计:
 *   - 纯虚接口 IGncController 定义所有硬件操作
 *   - MockGncController 提供无硬件时的模拟实现 (仅Mock模式编译)
 *   - GncControllerImpl 提供真实GTS SDK实现 (仅Real模式编译)
 *
 * 铁律: UI层绝不直接 #include "gts.h", 只通过此接口与硬件交互
 *
 * 编译模式:
 *   USE_REAL_GNC → include "googol/gts.h" (真实MC_GPI=4等)
 *   !USE_REAL_GNC → Mock结构体和常量
 */

#ifndef GNCCONTROLLER_H
#define GNCCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <cmath>
#include <cstdlib>

// ============================================================
// 真实GTS SDK 或 Mock模拟 (二选一, 由chipSetter.pro控制)
// ============================================================
// TMoveAbsolutePrmEx 两种模式都需要 (gts.h中无此结构体)
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

// TLimitInfo — gts.h中无此类型, 两种模式都需要
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

#ifdef USE_REAL_GNC
    #include "googol/gts.h"
    // gts.h 提供: MC_GPI=4, MC_GPO=12, MC_LIMIT_POSITIVE=0, ...
    // gts.h 提供: TStandardHomePrm, TStandardHomeStatus
#else
    // ---- Mock结构体与常量 (与GTS SDK无关) ----

    struct TStandardHomePrm {
        short mode;
        double highSpeed;
        double lowSpeed;
        double acc;
        double offset;
        short check;
        short autoZeroPos;
        short motorStopDelay;
    };

    struct TStandardHomeStatus {
        short run;
        short stage;
        short error;
        long  capturePos;
    };

    // Mock IO常量
    #define MC_GPI              0
    #define MC_GPO              1
    #define MC_LIMIT_POSITIVE   0
    #define MC_LIMIT_NEGATIVE   1
    #define MC_ALARM            2
    #define MC_HOME             3

#endif // USE_REAL_GNC

/**
 * @class IGncController
 * @brief GNC SDK 抽象接口
 */
class IGncController : public QObject
{
    Q_OBJECT

public:
    explicit IGncController(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IGncController() {}

    // ---- 生命周期 ----
    virtual bool openCard() = 0;
    virtual bool netInit(const QString& xmlFile, int overTimeSec = 120) = 0;
    virtual bool loadConfig(short core, const QString& cfgFile) = 0;
    virtual bool closeCard() = 0;
    virtual bool isConnected() const = 0;

    // ---- 轴操作 ----
    virtual bool axisOn(short core, short axis) = 0;
    virtual bool axisOff(short core, short axis) = 0;
    virtual bool clearStatus(short core, short axis, short count) = 0;
    virtual bool zeroPosition(short core, short axis) = 0;

    // ---- 点位运动 ----
    virtual bool moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm) = 0;

    // ---- 回零 ----
    virtual bool executeHome(short core, short axis, const TStandardHomePrm& prm) = 0;
    virtual bool getHomeStatus(short core, short axis, TStandardHomeStatus& sts) = 0;

    // ---- 状态读取 ----
    virtual bool getAxisStatus(short core, short axis, long& status, unsigned long& clock) = 0;
    virtual bool getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock) = 0;
    virtual bool getLimitInfo(short core, short axis, TLimitInfo& info) = 0;

    // ---- IO操作 ----
    virtual bool readDI(short core, short diType, short diIndex, short* values, short count) = 0;
    virtual bool writeDO(short core, short doType, short doIndex, short* values, short count) = 0;

    // ---- 软限位 ----
    virtual bool setSoftLimit(short core, short axis, double positive, double negative) = 0;

signals:
    // 硬件错误信号 (MainWindow 连接此信号到 AlarmLogger)
    void hardwareError(const QString& source, const QString& message);
};

#ifndef USE_REAL_GNC
/**
 * @class MockGncController
 * @brief GNC控制器的模拟实现 — 仅Mock模式编译
 */
class MockGncController : public IGncController
{
    Q_OBJECT

public:
    explicit MockGncController(QObject *parent = nullptr);
    ~MockGncController() override;

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

    // ---- Mock专用: 手动控制模拟状态 ----
    void setDIValue(int diIndex, int value);
    int  getDIValue(int diIndex) const;
    void toggleDI(int diIndex);

private slots:
    void onSimulationTick();

private:
    bool m_connected;

    struct MockAxisState {
        bool    enabled;
        double  currentPos;
        double  targetPos;
        double  velocity;
        bool    moving;
        bool    homing;
        short   homeStage;
        TStandardHomePrm homePrm;
        long    homeCapturePos;
        bool    homeError;
    };

    QVector<MockAxisState> m_axes;
    short m_diValues[20];                       // 19个DI (1-based, 索引0保留)
    short m_doValues[13];                       // 4个DO (索引9-12, 其余保留)

    QTimer* m_simTimer;
    int     m_tickCount;

    static const int SIM_TICK_MS = 50;
    static const int AXIS_COUNT_MOCK = 13;
};
#endif // !USE_REAL_GNC

#endif // GNCCONTROLLER_H
