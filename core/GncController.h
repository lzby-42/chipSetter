/**
 * @file GncController.h
 * @brief GNC运动控制器SDK封装 — 前后端隔离层
 *
 * 设计:
 *   - 纯虚接口 IGncController 定义所有硬件操作
 *   - MockGncController 提供无硬件时的模拟实现
 *   - 后续集成真实SDK时, 创建 GncControllerImpl : IGncController
 *
 * 铁律: UI层绝不直接 #include "gxn.h", 只通过此接口与硬件交互
 */

#ifndef GNCCONTROLLER_H
#define GNCCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <cmath>
#include <cstdlib>   // rand()

// GNC SDK 结构体模拟定义 (真实SDK集成时替换为 #include "gxn.h")
// 这些结构体与 gxn.h 中的定义保持一致
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

// GNC SDK 常量 — IO类型定义
#define MC_GPI  0   // 通用数字输入
#define MC_GPO  1   // 通用数字输出

/**
 * @class IGncController
 * @brief GNC SDK 抽象接口
 *
 * 所有Core层通过此接口访问硬件, 不直接调GNC SDK。
 * 实现了:
 *   - 硬件替换: 改一处换全部
 *   - 可测试: 测试时注入 MockGncController
 *   - 可模拟: 无硬件时也能运行和调试UI
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
};

/**
 * @class MockGncController
 * @brief GNC控制器的模拟实现 — 无硬件时使用
 *
 * 所有运动用定时器模拟:
 *   - 点位运动: 匀速趋近目标位置
 *   - 回零: 模拟Home搜索流程
 *   - IO: 随机初始值, 可手动翻转
 *   - 用 QTimer 替代真实硬件轮询
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
    void setDIValue(int diIndex, int value);       // 手动翻转DI (模拟传感器触发)
    int  getDIValue(int diIndex) const;
    void toggleDI(int diIndex);                    // 翻转某个DI

private slots:
    void onSimulationTick();                       // 定时器回调: 模拟运动+位置更新

private:
    bool m_connected;

    // 每个轴的状态
    struct MockAxisState {
        bool    enabled;
        double  currentPos;         // 当前位置 (mm, 模拟)
        double  targetPos;          // 目标位置 (mm)
        double  velocity;           // 当前速度 (mm/s)
        bool    moving;
        bool    homing;
        short   homeStage;
        TStandardHomePrm homePrm;
        long    homeCapturePos;
        bool    homeError;
    };

    // 13轴状态数组 (索引 0-based)
    QVector<MockAxisState> m_axes;              // 13轴

    // DI/DO 值数组
    short m_diValues[17];                       // 16个DI (1-based)
    short m_doValues[5];                        // 4个DO (1-based)

    // 模拟定时器 (每50ms tick一次)
    QTimer* m_simTimer;
    int     m_tickCount;

    static const int SIM_TICK_MS = 50;          // 模拟周期 50ms = 20Hz
    static const int AXIS_COUNT_MOCK = 13;
};

#endif // GNCCONTROLLER_H
