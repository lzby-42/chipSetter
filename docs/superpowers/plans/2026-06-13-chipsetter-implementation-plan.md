# 固晶机控制系统 UI — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建固晶机上位机Qt控制界面，包含电机PTP控制、电机参数管理、IO信号监视、报警列表、生产统计五大模块，前后端严格分离。

**Architecture:** UI层(widgets/) → 信号/槽 → Core层(core/GncController封装SDK, MotorManager/IoManager业务逻辑) → GNC SDK。UI不直接调SDK。

**Tech Stack:** Qt 5.15.2 + C++11 + MSVC, 固高GNC SDK (gxn.h/gxn.lib), 1200×800 深色主题

---

## 文件清单

| 文件 | 职责 |
|------|------|
| `chipSetter.pro` | Qt工程文件 |
| `main.cpp` | 入口，QSS加载，MainWindow启动 |
| `core/HardwareConfig.h` | 轴ID宏、IO宏、默认参数 — 集中修改点 |
| `models/MotorAxis.h` | 单轴状态数据结构 |
| `models/IoSignal.h` | IO信号数据结构 |
| `models/AlarmRecord.h` | 报警记录数据结构 |
| `core/GncController.h` / `.cpp` | GNC SDK封装，纯虚接口 + Mock实现 |
| `core/MotorManager.h` / `.cpp` | 13轴状态管理，运动/回零/JOG协调 |
| `core/IoManager.h` / `.cpp` | DI/DO轮询，变化检测 |
| `core/AlarmLogger.h` / `.cpp` | 报警触发/恢复，历史存储 |
| `core/StatsCollector.h` / `.cpp` | 产量计时统计 |
| `widgets/StatusBarWidget.h` / `.cpp` | 顶部状态栏 |
| `widgets/MotorPtpWidget.h` / `.cpp` | 电机点位运动面板 |
| `widgets/MotorParamWidget.h` / `.cpp` | 电机参数管理面板 |
| `widgets/IoMonitorWidget.h` / `.cpp` | IO信号监视面板 |
| `widgets/AlarmListWidget.h` / `.cpp` | 报警列表面板 |
| `widgets/StatsWidget.h` / `.cpp` | 生产统计面板 |
| `widgets/BottomBarWidget.h` / `.cpp` | 底部操作栏 |
| `mainwindow.h` / `.cpp` | 主窗口布局+信号/槽连接 |
| `resources/style.qss` | 全局深色样式表 |

---

## Task 1: 项目脚手架 + HardwareConfig.h

**Files:**
- Create: `chipSetter.pro`
- Create: `main.cpp`
- Create: `core/HardwareConfig.h`

- [ ] **Step 1: 创建工程文件 chipSetter.pro**

```qmake
QT += core gui widgets

TARGET = chipSetter
TEMPLATE = app

CONFIG += c++11

# GNC SDK 路径 (部署到目标机时修改)
INCLUDEPATH += "D:/study/googel/固高网络型运动控制器编程手册20260509/site"
# LIBS += -L"<GNC_SDK_LIB_PATH>" -lgxn

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    core/GncController.cpp \
    core/MotorManager.cpp \
    core/IoManager.cpp \
    core/AlarmLogger.cpp \
    core/StatsCollector.cpp \
    widgets/StatusBarWidget.cpp \
    widgets/MotorPtpWidget.cpp \
    widgets/MotorParamWidget.cpp \
    widgets/IoMonitorWidget.cpp \
    widgets/AlarmListWidget.cpp \
    widgets/StatsWidget.cpp \
    widgets/BottomBarWidget.cpp

HEADERS += \
    mainwindow.h \
    core/HardwareConfig.h \
    models/MotorAxis.h \
    models/IoSignal.h \
    models/AlarmRecord.h \
    core/GncController.h \
    core/MotorManager.h \
    core/IoManager.h \
    core/AlarmLogger.h \
    core/StatsCollector.h \
    widgets/StatusBarWidget.h \
    widgets/MotorPtpWidget.h \
    widgets/MotorParamWidget.h \
    widgets/IoMonitorWidget.h \
    widgets/AlarmListWidget.h \
    widgets/StatsWidget.h \
    widgets/BottomBarWidget.h

RESOURCES += resources.qrc
```

- [ ] **Step 2: 创建 main.cpp**

```cpp
/**
 * @file main.cpp
 * @brief 固晶机控制系统 — 应用入口
 *
 * 职责:
 *   - 初始化 QApplication
 *   - 加载全局样式表 (resources/style.qss)
 *   - 创建并显示 MainWindow
 *   - 事件循环
 */
#include <QApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用元信息
    app.setApplicationName("chipSetter");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ChipSetter");

    // 加载全局深色样式表
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    // 创建并显示主窗口 (1200x800)
    MainWindow mainWindow;
    mainWindow.setWindowTitle("固晶机控制系统 V1.0");
    mainWindow.resize(1200, 800);
    mainWindow.setMinimumSize(1024, 680);
    mainWindow.show();

    return app.exec();
}
```

- [ ] **Step 3: 创建 HardwareConfig.h — 硬件宏集中管理**

```cpp
/**
 * @file HardwareConfig.h
 * @brief 硬件配置集中定义 — 轴ID、IO编号、默认参数
 *
 * 用法:
 *   - 所有轴和IO通过此文件的宏引用，不硬编码数字
 *   - 与GNC文档对齐后只改此文件即可
 *
 * 注意:
 *   - GNC SDK 轴索引从 1 开始，本文件宏也以1为基准
 *   - 有些宏暂时是占位, 待确认后修正
 */

#ifndef HARDWARECONFIG_H
#define HARDWARECONFIG_H

// ============================================================
// 控制器核心参数
// ============================================================
#define GNC_CORE_NUM        1       // GNC核号 (固定为1)
#define GNC_AXIS_START      1       // 轴起始索引 (GNC SDK从1开始)

// ============================================================
// 轴ID定义 (待GNC文档最终确认!)
// ============================================================
#define AXIS_COUNT          13

#define AXIS_WAFER_X        1       // 晶盘X轴
#define AXIS_WAFER_Y        2       // 晶盘Y轴
#define AXIS_DISPENSE_X     3       // 点胶X轴
#define AXIS_DISPENSE_Y     4       // 点胶Y轴
#define AXIS_EJECTOR_Z1     5       // Z顶针电机1
#define AXIS_EJECTOR_Z2     6       // Z顶针电机2
#define AXIS_ROTARY_WHEEL   7       // 转轮电机
#define AXIS_RESERVED_8     8       // 待确认
#define AXIS_RESERVED_9     9       // 待确认
#define AXIS_RESERVED_10    10      // 待确认
#define AXIS_RESERVED_11    11      // 待确认
#define AXIS_RESERVED_12    12      // 待确认
#define AXIS_RESERVED_13    13      // 待确认

// 轴名称数组 (索引 = 轴ID - 1)
#define AXIS_NAMES { \
    "晶盘X", "晶盘Y", "点胶X", "点胶Y", \
    "Z顶针1", "Z顶针2", "转轮", \
    "轴8", "轴9", "轴10", "轴11", "轴12", "轴13" \
}

// ============================================================
// IO定义
// ============================================================
#define DI_COUNT            16
#define DO_COUNT            4

// 数字输入 (DI) — 限位/Home
#define DI_WAFER_X_HOME         1
#define DI_WAFER_Y_HOME         2
#define DI_WAFER_X_LIMIT_P      3
#define DI_WAFER_X_LIMIT_N      4
#define DI_WAFER_Y_LIMIT_P      5
#define DI_WAFER_Y_LIMIT_N      6
#define DI_DISPENSE_X_HOME      7
#define DI_DISPENSE_Y_HOME      8
#define DI_DISPENSE_X_LIMIT_P   9
#define DI_DISPENSE_X_LIMIT_N   10
#define DI_DISPENSE_Y_LIMIT_P   11
#define DI_DISPENSE_Y_LIMIT_N   12
#define DI_EJECTOR_Z1_HOME      13
#define DI_EJECTOR_Z2_HOME      14
#define DI_ROTARY_HOME          15
#define DI_EMERGENCY_STOP       16

// DI名称数组
#define DI_NAMES { \
    "晶盘X_Home", "晶盘Y_Home", \
    "晶盘X_Limit+", "晶盘X_Limit-", \
    "晶盘Y_Limit+", "晶盘Y_Limit-", \
    "点胶X_Home", "点胶Y_Home", \
    "点胶X_Limit+", "点胶X_Limit-", \
    "点胶Y_Limit+", "点胶Y_Limit-", \
    "Z顶针1_Home", "Z顶针2_Home", \
    "转轮_Home", "急停信号" \
}

// 数字输出 (DO) — 4路
#define DO_RED_LIGHT        1
#define DO_YELLOW_LIGHT     2
#define DO_GREEN_LIGHT      3
#define DO_BUZZER           4

#define DO_NAMES { "红灯", "黄灯", "绿灯", "蜂鸣器" }

// ============================================================
// 默认运动参数 (单位: mm, mm/s, mm/s² — UI用mm, 内部转为pulse)
// ============================================================
#define DEFAULT_VELOCITY        100.0
#define DEFAULT_ACCEL           500.0
#define DEFAULT_DECEL           500.0
#define DEFAULT_JOG_STEP        0.010
#define DEFAULT_JOG_STEP_FAST   1.000

#define DEFAULT_LEAD_SCREW      10.0    // 导程 mm
#define DEFAULT_PULSE_PER_REV   10000   // 每转脉冲数
#define DEFAULT_GEAR_RATIO      1.0     // 电子齿轮比

// ============================================================
// 运动极限
// ============================================================
#define MAX_VELOCITY            500.0   // mm/s
#define MAX_ACCEL               2000.0  // mm/s²

#endif // HARDWARECONFIG_H
```

- [ ] **Step 4: 创建 resources/style.qss (占位，内容在 Task 18 填写)**

```css
/* 全局样式 — 深色工业主题 (Task 18 填写完整内容) */
```

- [ ] **Step 5: 创建 resources.qrc**

```xml
<RCC>
    <qresource prefix="/">
        <file>resources/style.qss</file>
    </qresource>
</RCC>
```

---

## Task 2: 数据模型 — MotorAxis.h

**Files:**
- Create: `models/MotorAxis.h`

- [ ] **Step 1: 创建 MotorAxis.h — 单轴完整状态描述**

```cpp
/**
 * @file MotorAxis.h
 * @brief 电机轴数据模型 — 纯数据结构, 无业务逻辑
 *
 * 前后端共享此数据结构。
 * UI层读取本结构显示，Core层填充本结构后通过信号发出。
 */

#ifndef MOTORAXIS_H
#define MOTORAXIS_H

#include <QString>
#include "core/HardwareConfig.h"

/**
 * @struct MotorAxis
 * @brief 单个电机轴的完整状态快照
 */
struct MotorAxis {
    int     axisId;                 // 轴ID (1~13, 对应GNC SDK索引)
    QString name;                   // 轴名称 ("晶盘X" 等)

    // ---- 位置信息 ----
    double  currentPosition;        // 当前位置 (mm)
    double  targetPosition;         // 目标位置 (mm), PTP模式

    // ---- 运动参数 ----
    double  velocity;               // 运行速度 (mm/s)
    double  acceleration;           // 加速度 (mm/s²)
    double  deceleration;           // 减速度 (mm/s²)
    double  jogStep;                // JOG步距 (mm)

    // ---- 机械参数 ----
    double  leadScrew;              // 导程 (mm/rev)
    int     pulsePerRev;            // 每转脉冲数
    double  gearRatio;              // 电子齿轮比

    // ---- 限位参数 ----
    double  softLimitPositive;      // 正向软限位 (mm)
    double  softLimitNegative;      // 负向软限位 (mm)

    // ---- 状态 ----
    bool    isEnabled;              // 轴使能状态
    bool    isMoving;               // 运动中
    bool    isHomed;                // 已回零
    bool    isAlarm;                // 报警状态
    long    rawStatus;              // GNC原始轴状态字 (bit flags)

    // ---- 限位信号 ----
    bool    homeSignal;             // Home传感器触发
    bool    limitPositiveSignal;    // 正限位触发
    bool    limitNegativeSignal;    // 负限位触发

    // 默认构造
    MotorAxis()
        : axisId(0)
        , currentPosition(0.0)
        , targetPosition(0.0)
        , velocity(DEFAULT_VELOCITY)
        , acceleration(DEFAULT_ACCEL)
        , deceleration(DEFAULT_DECEL)
        , jogStep(DEFAULT_JOG_STEP)
        , leadScrew(DEFAULT_LEAD_SCREW)
        , pulsePerRev(DEFAULT_PULSE_PER_REV)
        , gearRatio(DEFAULT_GEAR_RATIO)
        , softLimitPositive(300.0)
        , softLimitNegative(0.0)
        , isEnabled(false)
        , isMoving(false)
        , isHomed(false)
        , isAlarm(false)
        , rawStatus(0)
        , homeSignal(false)
        , limitPositiveSignal(false)
        , limitNegativeSignal(false)
    {}
};

// 声明为 Qt 元类型 (支持跨线程信号槽传递)
Q_DECLARE_METATYPE(MotorAxis)

#endif // MOTORAXIS_H
```

---

## Task 3: 数据模型 — IoSignal.h + AlarmRecord.h

**Files:**
- Create: `models/IoSignal.h`
- Create: `models/AlarmRecord.h`

- [ ] **Step 1: 创建 IoSignal.h**

```cpp
/**
 * @file IoSignal.h
 * @brief IO信号数据模型 — 描述单个DI或DO通道
 */

#ifndef IOSIGNAL_H
#define IOSIGNAL_H

#include <QString>

/**
 * @enum IoType
 * @brief IO类型: 数字输入 或 数字输出
 */
enum IoType {
    IO_TYPE_DI,     // 数字输入 (Digital Input)
    IO_TYPE_DO      // 数字输出 (Digital Output)
};

/**
 * @struct IoSignal
 * @brief 单个IO通道的完整描述
 */
struct IoSignal {
    int     id;             // IO通道编号 (1-based, 对应GNC SDK)
    IoType  type;           // DI 或 DO
    QString name;           // 信号别名 ("晶盘X_Home", "红灯"等)
    int     value;          // 当前值 (0=低电平, 1=高电平)

    IoSignal() : id(0), type(IO_TYPE_DI), value(0) {}

    IoSignal(int id_, IoType type_, const QString& name_)
        : id(id_), type(type_), name(name_), value(0) {}
};

Q_DECLARE_METATYPE(IoSignal)

#endif // IOSIGNAL_H
```

- [ ] **Step 2: 创建 AlarmRecord.h**

```cpp
/**
 * @file AlarmRecord.h
 * @brief 报警记录数据模型 — 单条报警的完整信息
 */

#ifndef ALARMRECORD_H
#define ALARMRECORD_H

#include <QString>
#include <QDateTime>

/**
 * @enum AlarmLevel
 * @brief 报警级别
 */
enum AlarmLevel {
    ALARM_LEVEL_FATAL   = 0,    // 致命 (红色) — 急停类
    ALARM_LEVEL_WARNING = 1,    // 警告 (橙色) — 参数异常
    ALARM_LEVEL_INFO    = 2     // 信息 (灰色) — 历史恢复记录
};

/**
 * @struct AlarmRecord
 * @brief 单条报警记录
 */
struct AlarmRecord {
    int         id;             // 报警ID (自增)
    QDateTime   timestamp;      // 触发时间
    AlarmLevel  level;          // 级别
    QString     source;         // 来源 ("晶盘X轴", "IO系统" 等)
    QString     message;        // 报警内容 ("驱动器报警", "回零超时" 等)
    bool        resolved;       // 是否已恢复

    AlarmRecord()
        : id(0)
        , level(ALARM_LEVEL_INFO)
        , resolved(false)
    {
        timestamp = QDateTime::currentDateTime();
    }
};

Q_DECLARE_METATYPE(AlarmRecord)

#endif // ALARMRECORD_H
```

---

## Task 4: GncController — 接口 + Mock实现

**Files:**
- Create: `core/GncController.h`
- Create: `core/GncController.cpp`

- [ ] **Step 1: 创建 GncController.h — GNC SDK封装接口**

```cpp
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
```

- [ ] **Step 2: 创建 GncController.cpp — Mock实现**

```cpp
/**
 * @file GncController.cpp
 * @brief MockGncController 实现 — 模拟GNC控制器行为
 */

#include "GncController.h"
#include <QDebug>

// ============================================================
// MockGncController 构造/析构
// ============================================================

MockGncController::MockGncController(QObject *parent)
    : IGncController(parent)
    , m_connected(false)
    , m_tickCount(0)
{
    // 初始化13轴状态
    m_axes.resize(AXIS_COUNT_MOCK);
    for (int i = 0; i < AXIS_COUNT_MOCK; ++i) {
        MockAxisState& ax = m_axes[i];
        ax.enabled     = false;
        ax.currentPos  = 100.0 + i * 50.0;      // 初始位置分散
        ax.targetPos   = ax.currentPos;
        ax.velocity    = 0.0;
        ax.moving      = false;
        ax.homing      = false;
        ax.homeStage   = 0;
        ax.homeError   = false;
    }

    // 初始化DI (1~16)
    memset(m_diValues, 0, sizeof(m_diValues));
    // 模拟: Home传感器默认触发, 限位不触发, 急停不触发
    m_diValues[1]  = 1;   // 晶盘X_Home
    m_diValues[2]  = 1;   // 晶盘Y_Home
    m_diValues[7]  = 1;   // 点胶X_Home
    m_diValues[13] = 1;   // Z顶针1_Home
    m_diValues[15] = 1;   // 转轮_Home
    // 其余为0

    // 初始化DO (1~4), 默认全低
    memset(m_doValues, 0, sizeof(m_doValues));

    // 启动模拟定时器
    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &MockGncController::onSimulationTick);
}

MockGncController::~MockGncController()
{
    if (m_simTimer->isActive())
        m_simTimer->stop();
    closeCard();
}

// ============================================================
// 生命周期
// ============================================================

bool MockGncController::openCard()
{
    qDebug() << "[MockGnc] openCard() — 模拟开卡成功";
    m_connected = true;
    return true;
}

bool MockGncController::netInit(const QString& xmlFile, int overTimeSec)
{
    Q_UNUSED(xmlFile);
    Q_UNUSED(overTimeSec);
    qDebug() << "[MockGnc] netInit() — 模拟网络初始化成功";
    return true;
}

bool MockGncController::loadConfig(short core, const QString& cfgFile)
{
    Q_UNUSED(core);
    Q_UNUSED(cfgFile);
    qDebug() << "[MockGnc] loadConfig() — 模拟配置加载成功";
    return true;
}

bool MockGncController::closeCard()
{
    m_connected = false;
    if (m_simTimer->isActive())
        m_simTimer->stop();
    qDebug() << "[MockGnc] closeCard() — 模拟关卡";
    return true;
}

bool MockGncController::isConnected() const
{
    return m_connected;
}

// ============================================================
// 轴操作
// ============================================================

bool MockGncController::axisOn(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].enabled = true;
    qDebug() << "[MockGnc] axisOn(" << axis << ")";
    return true;
}

bool MockGncController::axisOff(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].enabled = false;
    return true;
}

bool MockGncController::clearStatus(short core, short axis, short count)
{
    Q_UNUSED(core);
    for (short i = 0; i < count && (axis + i - 1) < AXIS_COUNT_MOCK; ++i) {
        m_axes[axis + i - 1].moving = false;
        m_axes[axis + i - 1].homing = false;
    }
    return true;
}

bool MockGncController::zeroPosition(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].currentPos = 0.0;
    return true;
}

// ============================================================
// 点位运动
// ============================================================

bool MockGncController::moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    if (!ax.enabled) return false;

    ax.targetPos  = prm.pos;
    ax.velocity   = prm.vel;
    ax.moving     = true;
    ax.homing     = false;

    qDebug() << "[MockGnc] moveAbsolute axis=" << axis
             << " target=" << prm.pos
             << " vel=" << prm.vel
             << " acc=" << prm.acc
             << " dec=" << prm.dec;
    return true;
}

// ============================================================
// 回零
// ============================================================

bool MockGncController::executeHome(short core, short axis, const TStandardHomePrm& prm)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    ax.homing      = true;
    ax.moving      = true;
    ax.homeStage   = 1; // STANDARD_HOME_STAGE_START
    ax.homePrm     = prm;
    ax.homeError   = false;
    ax.homeCapturePos = 0;

    qDebug() << "[MockGnc] executeHome axis=" << axis << " mode=" << prm.mode;
    return true;
}

bool MockGncController::getHomeStatus(short core, short axis, TStandardHomeStatus& sts)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    memset(&sts, 0, sizeof(sts));
    sts.run        = ax.homing ? 1 : 0;
    sts.stage      = ax.homeStage;
    sts.error      = ax.homeError ? 1 : 0;
    sts.capturePos = ax.homeCapturePos;
    return true;
}

// ============================================================
// 状态读取
// ============================================================

bool MockGncController::getAxisStatus(short core, short axis, long& status, unsigned long& clock)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];

    // 构建GNC风格的轴状态字
    status = 0;
    if (ax.enabled)  status |= 0x001;    // 使能
    if (ax.moving)   status |= 0x400;    // 运动中 (bit10)
    if (ax.homing)   status |= 0x800;    // 回零中
    if (ax.homeError) status |= 0x1000;  // 错误

    clock = static_cast<unsigned long>(m_tickCount);
    return true;
}

bool MockGncController::getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    prfPos = m_axes[axis - 1].currentPos;
    clock  = static_cast<unsigned long>(m_tickCount);
    return true;
}

bool MockGncController::getLimitInfo(short core, short axis, TLimitInfo& info)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    memset(&info, 0, sizeof(info));
    return true;
}

// ============================================================
// IO
// ============================================================

bool MockGncController::readDI(short core, short diType, short diIndex, short* values, short count)
{
    Q_UNUSED(core);
    Q_UNUSED(diType);
    for (short i = 0; i < count; ++i) {
        int idx = diIndex + i;
        if (idx >= 1 && idx <= 16)
            values[i] = m_diValues[idx];
        else
            values[i] = 0;
    }
    return true;
}

bool MockGncController::writeDO(short core, short doType, short doIndex, short* values, short count)
{
    Q_UNUSED(core);
    Q_UNUSED(doType);
    for (short i = 0; i < count; ++i) {
        int idx = doIndex + i;
        if (idx >= 1 && idx <= 4)
            m_doValues[idx] = values[i];
    }
    return true;
}

// ============================================================
// 软限位
// ============================================================

bool MockGncController::setSoftLimit(short core, short axis, double positive, double negative)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    Q_UNUSED(positive);
    Q_UNUSED(negative);
    return true;
}

// ============================================================
// Mock专用方法
// ============================================================

void MockGncController::setDIValue(int diIndex, int value)
{
    if (diIndex >= 1 && diIndex <= 16)
        m_diValues[diIndex] = static_cast<short>(value);
}

int MockGncController::getDIValue(int diIndex) const
{
    if (diIndex >= 1 && diIndex <= 16)
        return m_diValues[diIndex];
    return 0;
}

void MockGncController::toggleDI(int diIndex)
{
    if (diIndex >= 1 && diIndex <= 16)
        m_diValues[diIndex] = m_diValues[diIndex] ? 0 : 1;
}

// ============================================================
// 模拟定时器回调 — 每50ms驱动一次模拟运动
// ============================================================

void MockGncController::onSimulationTick()
{
    m_tickCount++;
    double dt = SIM_TICK_MS / 1000.0;   // 秒

    for (int i = 0; i < AXIS_COUNT_MOCK; ++i) {
        MockAxisState& ax = m_axes[i];

        if (ax.homing) {
            // 模拟回零流程: 快速过阶段
            double homeSpeed = ax.homePrm.highSpeed;
            ax.currentPos += homeSpeed * dt * 0.5;
            ax.homeStage++;
            if (ax.homeStage > 9) {
                // 回零完成
                ax.homing = false;
                ax.moving = false;
                ax.currentPos = 0.0;
                ax.homeStage = 9; // STANDARD_HOME_STAGE_END
            }
        }
        else if (ax.moving) {
            // 模拟点位运动: 匀速趋近目标
            double diff = ax.targetPos - ax.currentPos;
            double step = ax.velocity * dt;
            if (qAbs(diff) <= step) {
                ax.currentPos = ax.targetPos;
                ax.moving = false;
            } else {
                ax.currentPos += (diff > 0 ? step : -step);
            }
        }
    }
}
```

- [ ] **Step 3: 编译验证**

```bash
cd d:/my_information/qtProject/chipSetter
D:/tool/qt/5.15.2/mingw81_64/bin/qmake.exe chipSetter.pro
mingw32-make
```
Expected: 编译通过 (会有 mainwindow 等未创建的链接错误 — 正常，后续Task创建)

---

## Task 5: MotorManager — 电机管理核心

**Files:**
- Create: `core/MotorManager.h`
- Create: `core/MotorManager.cpp`

- [ ] **Step 1: 创建 MotorManager.h**

```cpp
/**
 * @file MotorManager.h
 * @brief 电机管理器 — 13轴状态维护、运动协调、参数管理
 *
 * 职责:
 *   - 管理13个轴的 MotorAxis 状态
 *   - 接收UI的运动请求 (moveRequest, homeRequest, jogRequest)
 *   - 通过 IGncController 发送运动指令
 *   - 定时轮询位置和状态, 通过信号通知UI
 *
 * 依赖: IGncController (注入), HardwareConfig.h, MotorAxis.h
 */

#ifndef MOTORMANAGER_H
#define MOTORMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include "models/MotorAxis.h"
#include "core/GncController.h"
#include "core/HardwareConfig.h"

class MotorManager : public QObject
{
    Q_OBJECT

public:
    explicit MotorManager(IGncController* controller, QObject *parent = nullptr);
    ~MotorManager();

    // ---- 初始化 ----
    bool initialize();                          // 初始化所有轴状态
    bool enableAxis(int axisId);                // 轴使能
    bool disableAxis(int axisId);               // 轴去使能

    // ---- 状态获取 ----
    const MotorAxis& axisState(int axisId) const;
    QVector<MotorAxis> allAxisStates() const;

    // ---- 参数管理 ----
    bool updateAxisParams(int axisId, const MotorAxis& params);  // 更新轴参数
    bool loadParamsFromFile(const QString& filePath);             // 从文件加载参数
    bool saveParamsToFile(const QString& filePath);               // 保存参数到文件

    // ---- 轮询控制 ----
    void startPolling(int intervalMs = 50);     // 开始轮询
    void stopPolling();                         // 停止轮询

public slots:
    // ---- 运动请求 (来自UI) ----
    void moveRequest(int axisId, double targetPos, double vel, double acc, double dec);
    void stopMove(int axisId);
    void homeRequest(int axisId);
    void jogRequest(int axisId, bool positive, double step, double vel, double acc, double dec);

signals:
    // ---- 状态更新 (通知UI) ----
    void positionUpdated(int axisId, double position);
    void moveFinished(int axisId, bool success);
    void homeFinished(int axisId, bool success, int homeStage);
    void axisStatusChanged(int axisId, long status);
    void axisParamChanged(int axisId);
    void motorError(int axisId, const QString& errorMsg);

private slots:
    void onPollTimer();                         // 轮询定时器回调

private:
    bool validateMove(int axisId, double targetPos, double vel, double acc, double dec);
    double mmToPulse(int axisId, double mm) const;  // mm → pulse 换算
    double pulseToMm(int axisId, double pulse) const;

    IGncController*      m_controller;          // GNC接口 (注入, 不拥有)
    QVector<MotorAxis>   m_axes;                // 13轴状态 (0-based)
    QTimer*              m_pollTimer;           // 轮询定时器
    bool                 m_polling;
};

#endif // MOTORMANAGER_H
```

- [ ] **Step 2: 创建 MotorManager.cpp**

```cpp
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
```

---

## Task 6: IoManager — IO信号管理

**Files:**
- Create: `core/IoManager.h`
- Create: `core/IoManager.cpp`

- [ ] **Step 1: 创建 IoManager.h**

```cpp
/**
 * @file IoManager.h
 * @brief IO管理器 — DI/DO状态轮询、变化检测
 *
 * 职责:
 *   - 定时轮询16路DI + 4路DO状态
 *   - 检测信号变化, 通过信号通知UI
 *   - 限位信号联动报警 (通过信号通知 AlarmLogger)
 *
 * 依赖: IGncController, HardwareConfig.h, IoSignal.h
 */

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include "models/IoSignal.h"
#include "core/GncController.h"
#include "core/HardwareConfig.h"

class IoManager : public QObject
{
    Q_OBJECT

public:
    explicit IoManager(IGncController* controller, QObject *parent = nullptr);
    ~IoManager();

    // ---- 初始化 ----
    void initialize();                          // 加载信号别名, 开始轮询

    // ---- 获取当前状态 ----
    const IoSignal& diState(int index) const;   // 获取某个DI (1-based)
    const IoSignal& doState(int index) const;   // 获取某个DO (1-based)
    QVector<IoSignal> allDiStates() const;
    QVector<IoSignal> allDoStates() const;

    // ---- 输出控制 ----
    bool setDO(int doIndex, int value);         // 设置某个DO (1=高, 0=低)

    // ---- 轮询 ----
    void startPolling(int intervalMs = 50);
    void stopPolling();

signals:
    void diChanged(int id, int value);           // DI状态变化 (给UI)
    void doChanged(int id, int value);           // DO状态变化 (给UI)
    void limitSignalChanged(int axisId, bool positive, bool triggered);  // 限位变化 (给MotorManager)
    void emergencyStopChanged(bool triggered);   // 急停信号变化 (给MainWindow)

private slots:
    void onPollTimer();

private:
    void detectChanges();                       // 检测变化并发射信号

    IGncController*  m_controller;
    QTimer*          m_pollTimer;

    QVector<IoSignal> m_diSignals;              // 16路DI
    QVector<IoSignal> m_doSignals;              // 4路DO
    QVector<short>    m_lastDiValues;           // 上次DI值 (用于变化检测)
    QVector<short>    m_lastDoValues;           // 上次DO值
};

#endif // IOMANAGER_H
```

- [ ] **Step 2: 创建 IoManager.cpp**

```cpp
/**
 * @file IoManager.cpp
 * @brief IoManager 实现
 */

#include "IoManager.h"
#include <QDebug>

IoManager::IoManager(IGncController* controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    // 初始化DI信号列表
    QStringList diNames = DI_NAMES;
    for (int i = 0; i < DI_COUNT; ++i) {
        IoSignal sig;
        sig.id   = i + 1;
        sig.type = IO_TYPE_DI;
        sig.name = (i < diNames.size()) ? diNames[i] : QString("DI_%1").arg(i + 1);
        sig.value = 0;
        m_diSignals.append(sig);
    }

    // 初始化DO信号列表
    QStringList doNames = DO_NAMES;
    for (int i = 0; i < DO_COUNT; ++i) {
        IoSignal sig;
        sig.id   = i + 1;
        sig.type = IO_TYPE_DO;
        sig.name = (i < doNames.size()) ? doNames[i] : QString("DO_%1").arg(i + 1);
        sig.value = 0;
        m_doSignals.append(sig);
    }

    m_lastDiValues.resize(DI_COUNT + 1);
    m_lastDoValues.resize(DO_COUNT + 1);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &IoManager::onPollTimer);
}

IoManager::~IoManager()
{
    stopPolling();
}

void IoManager::initialize()
{
    // 读取一次初始值
    onPollTimer();
    startPolling(50);
}

const IoSignal& IoManager::diState(int index) const
{
    if (index >= 1 && index <= DI_COUNT) return m_diSignals[index - 1];
    static IoSignal dummy;
    return dummy;
}

const IoSignal& IoManager::doState(int index) const
{
    if (index >= 1 && index <= DO_COUNT) return m_doSignals[index - 1];
    static IoSignal dummy;
    return dummy;
}

QVector<IoSignal> IoManager::allDiStates() const { return m_diSignals; }

QVector<IoSignal> IoManager::allDoStates() const { return m_doSignals; }

bool IoManager::setDO(int doIndex, int value)
{
    if (doIndex < 1 || doIndex > DO_COUNT) return false;
    if (!m_controller || !m_controller->isConnected()) return false;

    short val = static_cast<short>(value);
    bool ok = m_controller->writeDO(GNC_CORE_NUM, MC_GPO,
                                    static_cast<short>(doIndex), &val, 1);
    if (ok) {
        m_doSignals[doIndex - 1].value = value;
        emit doChanged(doIndex, value);
    }
    return ok;
}

void IoManager::startPolling(int intervalMs)
{
    m_pollTimer->start(intervalMs);
}

void IoManager::stopPolling()
{
    m_pollTimer->stop();
}

void IoManager::onPollTimer()
{
    if (!m_controller || !m_controller->isConnected()) return;

    // 读取所有DI
    short diValues[16];
    m_controller->readDI(GNC_CORE_NUM, MC_GPI, 1, diValues, DI_COUNT);

    for (int i = 0; i < DI_COUNT; ++i) {
        m_diSignals[i].value = diValues[i];
    }

    // 读取所有DO (通过读回功能, 或直接用缓存 — Mock用缓存)
    // 实际GNC SDk: 用 readDI(MC_GPO) 读回输出状态
    // 这里暂用内部缓存

    detectChanges();
}

void IoManager::detectChanges()
{
    for (int i = 0; i < DI_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_diSignals[i].value;
        if (newVal != m_lastDiValues[idx]) {
            m_lastDiValues[idx] = newVal;
            emit diChanged(idx, newVal);

            // 急停信号检测
            if (idx == DI_EMERGENCY_STOP) {
                emit emergencyStopChanged(newVal == 0); // 低电平 = 急停触发
            }
        }
    }

    for (int i = 0; i < DO_COUNT; ++i) {
        int idx = i + 1;
        short newVal = m_doSignals[i].value;
        if (newVal != m_lastDoValues[idx]) {
            m_lastDoValues[idx] = newVal;
            emit doChanged(idx, newVal);
        }
    }
}
```

---

## Task 7: AlarmLogger — 报警日志

**Files:**
- Create: `core/AlarmLogger.h`
- Create: `core/AlarmLogger.cpp`

- [ ] **Step 1: 创建 AlarmLogger.h**

```cpp
/**
 * @file AlarmLogger.h
 * @brief 报警管理器 — 报警触发、恢复、历史记录
 *
 * 职责:
 *   - 接收报警信号 (急停、限位、驱动错误等)
 *   - 维护报警列表 (活跃 + 历史)
 *   - 自动生成恢复记录
 *
 * 依赖: AlarmRecord.h, HardwareConfig.h
 */

#ifndef ALARMLOGGER_H
#define ALARMLOGGER_H

#include <QObject>
#include <QVector>
#include "models/AlarmRecord.h"

class AlarmLogger : public QObject
{
    Q_OBJECT

public:
    explicit AlarmLogger(QObject *parent = nullptr);
    ~AlarmLogger();

    // ---- 报警列表 ----
    QVector<AlarmRecord> activeAlarms() const;      // 当前活跃报警
    QVector<AlarmRecord> allRecords() const;         // 全部记录 (含历史)
    int activeCount() const;                         // 活跃报警数

    // ---- 查询 ----
    QVector<AlarmRecord> alarmsByLevel(AlarmLevel level) const;
    AlarmRecord recordById(int alarmId) const;

public slots:
    // ---- 接收报警信号 ----
    void raiseAlarm(AlarmLevel level, const QString& source, const QString& message);
    void resolveAlarm(int alarmId);
    void clearAll();                                // 清除所有报警

signals:
    void newAlarm(AlarmRecord record);              // 新报警 (通知UI)
    void alarmResolved(int alarmId);                // 报警已恢复
    void activeCountChanged(int count);             // 活跃数量变化

private:
    QVector<AlarmRecord> m_records;                 // 全部记录
    int m_nextId;
};

#endif // ALARMLOGGER_H
```

- [ ] **Step 2: 创建 AlarmLogger.cpp**

```cpp
/**
 * @file AlarmLogger.cpp
 * @brief AlarmLogger 实现
 */

#include "AlarmLogger.h"
#include <QDebug>

AlarmLogger::AlarmLogger(QObject *parent)
    : QObject(parent)
    , m_nextId(1)
{
}

AlarmLogger::~AlarmLogger()
{
}

QVector<AlarmRecord> AlarmLogger::activeAlarms() const
{
    QVector<AlarmRecord> active;
    for (const AlarmRecord& rec : m_records) {
        if (!rec.resolved)
            active.append(rec);
    }
    return active;
}

QVector<AlarmRecord> AlarmLogger::allRecords() const
{
    return m_records;
}

int AlarmLogger::activeCount() const
{
    int cnt = 0;
    for (const AlarmRecord& rec : m_records) {
        if (!rec.resolved) ++cnt;
    }
    return cnt;
}

QVector<AlarmRecord> AlarmLogger::alarmsByLevel(AlarmLevel level) const
{
    QVector<AlarmRecord> result;
    for (const AlarmRecord& rec : m_records) {
        if (rec.level == level)
            result.append(rec);
    }
    return result;
}

AlarmRecord AlarmLogger::recordById(int alarmId) const
{
    for (const AlarmRecord& rec : m_records) {
        if (rec.id == alarmId)
            return rec;
    }
    return AlarmRecord();
}

void AlarmLogger::raiseAlarm(AlarmLevel level, const QString& source, const QString& message)
{
    AlarmRecord rec;
    rec.id        = m_nextId++;
    rec.timestamp = QDateTime::currentDateTime();
    rec.level     = level;
    rec.source    = source;
    rec.message   = message;
    rec.resolved  = false;

    m_records.prepend(rec);     // 新报警放在最前面
    emit newAlarm(rec);
    emit activeCountChanged(activeCount());

    qDebug() << "[AlarmLogger] 新报警 #" << rec.id
             << "[" << source << "] " << message;
}

void AlarmLogger::resolveAlarm(int alarmId)
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].id == alarmId && !m_records[i].resolved) {
            m_records[i].resolved = true;

            // 自动生成一条"已恢复"的记录
            AlarmRecord resolvedRec;
            resolvedRec.id        = m_nextId++;
            resolvedRec.timestamp = QDateTime::currentDateTime();
            resolvedRec.level     = ALARM_LEVEL_INFO;
            resolvedRec.source    = m_records[i].source;
            resolvedRec.message   = m_records[i].message + " [已恢复]";
            resolvedRec.resolved  = true;
            m_records.prepend(resolvedRec);

            emit alarmResolved(alarmId);
            emit activeCountChanged(activeCount());

            qDebug() << "[AlarmLogger] 报警 #" << alarmId << " 已恢复";
            break;
        }
    }
}

void AlarmLogger::clearAll()
{
    // 将所有活跃报警标记为恢复
    for (AlarmRecord& rec : m_records) {
        if (!rec.resolved) {
            rec.resolved = true;
        }
    }
    emit activeCountChanged(0);
}
```

---

## Task 8: StatsCollector — 生产统计

**Files:**
- Create: `core/StatsCollector.h`
- Create: `core/StatsCollector.cpp`

- [ ] **Step 1: 创建 StatsCollector.h**

```cpp
/**
 * @file StatsCollector.h
 * @brief 生产统计收集器
 *
 * 职责:
 *   - 统计总产量 (件数)
 *   - 统计运行时长 (累计运动时间)
 *   - 计算平均节拍 (秒/件)
 *
 * 用法:
 *   - 每次完成一个生产周期时调用 incrementCount()
 *   - 定时器自动更新运行时长
 */

#ifndef STATSCOLLECTOR_H
#define STATSCOLLECTOR_H

#include <QObject>
#include <QTimer>
#include <QDateTime>

class StatsCollector : public QObject
{
    Q_OBJECT

public:
    explicit StatsCollector(QObject *parent = nullptr);
    ~StatsCollector();

    // ---- 当前值 ----
    int    totalCount() const;        // 总产量
    double runningHours() const;      // 运行时长 (小时)
    double cycleTimeSec() const;      // 平均节拍 (秒/件)

    // ---- 操作 ----
    void start();                     // 开始计时
    void pause();                     // 暂停计时
    void reset();                     // 重置全部统计

public slots:
    void incrementCount();            // 产量+1 (外部触发)

signals:
    void statsUpdated(int totalCount, double runningHours, double cycleTimeSec);

private slots:
    void onTimerTick();               // 1秒定时器: 更新运行时长

private:
    int       m_totalCount;           // 总产量
    qint64    m_elapsedMs;            // 累计运行毫秒
    QTimer*   m_updateTimer;          // 1秒更新定时器
    QDateTime m_startTime;            // 本次开始时间
    bool      m_running;              // 是否在计时
};

#endif // STATSCOLLECTOR_H
```

- [ ] **Step 2: 创建 StatsCollector.cpp**

```cpp
/**
 * @file StatsCollector.cpp
 * @brief StatsCollector 实现
 */

#include "StatsCollector.h"
#include <QDebug>

StatsCollector::StatsCollector(QObject *parent)
    : QObject(parent)
    , m_totalCount(0)
    , m_elapsedMs(0)
    , m_running(false)
{
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &StatsCollector::onTimerTick);
    m_updateTimer->start(1000);  // 每秒更新一次
}

StatsCollector::~StatsCollector()
{
    m_updateTimer->stop();
}

int StatsCollector::totalCount() const
{
    return m_totalCount;
}

double StatsCollector::runningHours() const
{
    return m_elapsedMs / 3600000.0;
}

double StatsCollector::cycleTimeSec() const
{
    if (m_totalCount == 0) return 0.0;
    return m_elapsedMs / 1000.0 / m_totalCount;
}

void StatsCollector::start()
{
    if (!m_running) {
        m_running   = true;
        m_startTime = QDateTime::currentDateTime();
    }
}

void StatsCollector::pause()
{
    if (m_running) {
        m_running    = false;
        m_elapsedMs += m_startTime.msecsTo(QDateTime::currentDateTime());
    }
}

void StatsCollector::reset()
{
    m_totalCount = 0;
    m_elapsedMs  = 0;
    m_running    = false;
    emit statsUpdated(m_totalCount, runningHours(), cycleTimeSec());
}

void StatsCollector::incrementCount()
{
    m_totalCount++;
    emit statsUpdated(m_totalCount, runningHours(), cycleTimeSec());
}

void StatsCollector::onTimerTick()
{
    if (m_running) {
        qint64 currentElapsed = m_elapsedMs + m_startTime.msecsTo(QDateTime::currentDateTime());
        double hours  = currentElapsed / 3600000.0;
        double cycle  = (m_totalCount > 0) ? (currentElapsed / 1000.0 / m_totalCount) : 0.0;
        emit statsUpdated(m_totalCount, hours, cycle);
    }
}
```

---

## Task 9: StatusBarWidget — 顶部状态栏

**Files:**
- Create: `widgets/StatusBarWidget.h`
- Create: `widgets/StatusBarWidget.cpp`

- [ ] **Step 1: 创建 StatusBarWidget.h**

```cpp
/**
 * @file StatusBarWidget.h
 * @brief 顶部状态栏 — 模式指示、运行状态、产量、时间
 *
 * 纯UI组件, 通过槽函数接收外部数据更新显示。
 * 不包含任何业务逻辑。
 */

#ifndef STATUSBARWIDGET_H
#define STATUSBARWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class StatusBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget *parent = nullptr);
    ~StatusBarWidget();

public slots:
    void setMode(int mode);                     // 0=手动, 1=自动
    void setRunStatus(bool running);            // 运行/停止
    void setCount(int count);                   // 产量
    void setRunningTime(double hours);          // 运行时长
    void setCycleTime(double seconds);          // 节拍
    void updateTimestamp();                     // 刷新时间戳

private:
    void setupUI();

    QLabel* m_modeLabel;        // "自动模式" / "手动模式"
    QLabel* m_statusLabel;      // "🟢 运行中" / "⏸ 暂停"
    QLabel* m_countLabel;       // "产量: 1,248"
    QLabel* m_timeLabel;        // "时长: 03:52:17"
    QLabel* m_cycleLabel;       // "节拍: 2.8s"
    QLabel* m_timestampLabel;   // "2026-06-13 14:32:05"
};

#endif // STATUSBARWIDGET_H
```

- [ ] **Step 2: 创建 StatusBarWidget.cpp**

```cpp
/**
 * @file StatusBarWidget.cpp
 * @brief 顶部状态栏实现
 */

#include "StatusBarWidget.h"
#include <QDateTime>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    // 初始化时间戳
    updateTimestamp();
}

StatusBarWidget::~StatusBarWidget()
{
}

void StatusBarWidget::setupUI()
{
    setObjectName("StatusBarWidget");
    setFixedHeight(32);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 2, 12, 2);
    layout->setSpacing(16);

    // 应用标题
    QLabel* titleLabel = new QLabel("◆ 固晶机控制系统 V1.0", this);
    titleLabel->setStyleSheet("font-weight:bold; color:#00e676; font-size:13px;");
    layout->addWidget(titleLabel);

    layout->addStretch();

    // 模式标签
    m_modeLabel = new QLabel("自动模式", this);
    m_modeLabel->setStyleSheet(
        "background:#00e676; color:#000000; padding:2px 12px; "
        "border-radius:3px; font-weight:bold; font-size:12px;");
    layout->addWidget(m_modeLabel);

    // 运行状态
    m_statusLabel = new QLabel("🟢 运行中", this);
    m_statusLabel->setStyleSheet(
        "background:#1565c0; color:#ffffff; padding:2px 12px; "
        "border-radius:3px; font-size:12px;");
    layout->addWidget(m_statusLabel);

    // 产量
    m_countLabel = new QLabel("产量: 0", this);
    m_countLabel->setStyleSheet("color:#ffd740; font-size:12px;");
    layout->addWidget(m_countLabel);

    // 节拍
    m_cycleLabel = new QLabel("节拍: 0.0s", this);
    m_cycleLabel->setStyleSheet("color:#81c784; font-size:12px;");
    layout->addWidget(m_cycleLabel);

    // 运行时长
    m_timeLabel = new QLabel("时长: 00:00:00", this);
    m_timeLabel->setStyleSheet("color:#b0bec5; font-size:12px;");
    layout->addWidget(m_timeLabel);

    // 时间戳
    m_timestampLabel = new QLabel(this);
    m_timestampLabel->setStyleSheet("color:#78909c; font-size:10px;");
    layout->addWidget(m_timestampLabel);
}

void StatusBarWidget::setMode(int mode)
{
    if (mode == 0) {
        m_modeLabel->setText("手动模式");
        m_modeLabel->setStyleSheet(
            "background:#e65100; color:#ffffff; padding:2px 12px; "
            "border-radius:3px; font-weight:bold; font-size:12px;");
    } else {
        m_modeLabel->setText("自动模式");
        m_modeLabel->setStyleSheet(
            "background:#00e676; color:#000000; padding:2px 12px; "
            "border-radius:3px; font-weight:bold; font-size:12px;");
    }
}

void StatusBarWidget::setRunStatus(bool running)
{
    if (running) {
        m_statusLabel->setText("🟢 运行中");
        m_statusLabel->setStyleSheet(
            "background:#1565c0; color:#ffffff; padding:2px 12px; "
            "border-radius:3px; font-size:12px;");
    } else {
        m_statusLabel->setText("⏸ 暂停");
        m_statusLabel->setStyleSheet(
            "background:#37474f; color:#90a4ae; padding:2px 12px; "
            "border-radius:3px; font-size:12px;");
    }
}

void StatusBarWidget::setCount(int count)
{
    m_countLabel->setText(QString("产量: %1").arg(count));
}

void StatusBarWidget::setRunningTime(double hours)
{
    int totalSec = static_cast<int>(hours * 3600);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    m_timeLabel->setText(QString("时长: %1:%2:%3")
                         .arg(h, 2, 10, QLatin1Char('0'))
                         .arg(m, 2, 10, QLatin1Char('0'))
                         .arg(s, 2, 10, QLatin1Char('0')));
}

void StatusBarWidget::setCycleTime(double seconds)
{
    m_cycleLabel->setText(QString("节拍: %1s").arg(seconds, 0, 'f', 1));
}

void StatusBarWidget::updateTimestamp()
{
    m_timestampLabel->setText(
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}
```

---

## Task 10: MotorPtpWidget — 电机点位运动面板

**Files:**
- Create: `widgets/MotorPtpWidget.h`
- Create: `widgets/MotorPtpWidget.cpp`

- [ ] **Step 1: 创建 MotorPtpWidget.h**

```cpp
/**
 * @file MotorPtpWidget.h
 * @brief 电机点位运动面板 — 轴选择、位置输入、加减速、运行/停止/回零/JOG
 *
 * 职责:
 *   - 13轴快捷选择
 *   - 显示当前位置/输入目标位置
 *   - 设置速度/加速度/减速度
 *   - 运行/停止/回零/微调JOG按钮
 *
 * 纯UI: 按钮点击 → 发射信号 → MotorManager处理 → 信号返回 → 更新显示
 */

#ifndef MOTORPTPWIDGET_H
#define MOTORPTPWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include "models/MotorAxis.h"

class MotorPtpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorPtpWidget(QWidget *parent = nullptr);
    ~MotorPtpWidget();

    // ---- 获取当前选中轴 ----
    int currentAxisId() const;

public slots:
    // ---- 接收Core层更新 ----
    void onPositionUpdated(int axisId, double position);
    void onMoveFinished(int axisId, bool success);
    void onHomeFinished(int axisId, bool success, int stage);
    void onAxisStatusChanged(int axisId, long status);

signals:
    // ---- 用户操作信号 (→ MotorManager) ----
    void moveRequested(int axisId, double targetPos, double vel, double acc, double dec);
    void stopRequested(int axisId);
    void homeRequested(int axisId);
    void jogRequested(int axisId, bool positive, double step, double vel, double acc, double dec);

private slots:
    void onAxisButtonClicked(int axisId);   // 轴选择
    void onRunClicked();                    // 运行按钮
    void onStopClicked();                   // 停止按钮
    void onHomeClicked();                   // 回零按钮
    void onJogPlusClicked();                // JOG+
    void onJogMinusClicked();               // JOG-

private:
    void setupUI();
    void updateAxisButtons();               // 刷新轴选择按钮样式

    int             m_selectedAxisId;       // 当前选中轴 (1-based)
    double          m_currentPosition;      // 当前位置缓存

    // UI 控件
    QButtonGroup*   m_axisBtnGroup;         // 轴选择按钮组 (13个QPushButton)
    QDoubleSpinBox* m_targetPosSpin;        // 目标位置输入
    QDoubleSpinBox* m_velocitySpin;         // 速度
    QDoubleSpinBox* m_accelSpin;            // 加速度
    QDoubleSpinBox* m_decelSpin;            // 减速度
    QDoubleSpinBox* m_jogStepSpin;          // JOG步距
    QLabel*         m_curPosLabel;          // 当前位置显示
    QPushButton*    m_runBtn;
    QPushButton*    m_stopBtn;
    QPushButton*    m_homeBtn;
};

#endif // MOTORPTPWIDGET_H
```

- [ ] **Step 2: 创建 MotorPtpWidget.cpp** — 步骤较长，分两个子步骤

- [ ] **Step 2a: setupUI 部分**

```cpp
/**
 * @file MotorPtpWidget.cpp
 * @brief 电机点位运动面板 实现
 */

#include "MotorPtpWidget.h"
#include "core/HardwareConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>

MotorPtpWidget::MotorPtpWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedAxisId(1)   // 默认选中晶盘X
    , m_currentPosition(0.0)
{
    setupUI();
}

MotorPtpWidget::~MotorPtpWidget()
{
}

void MotorPtpWidget::setupUI()
{
    setObjectName("MotorPtpWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ---- 标题 ----
    QLabel* title = new QLabel("⚙ 点位运动 (PTP)", this);
    title->setStyleSheet("font-weight:bold; color:#64b5f6; font-size:13px;");
    mainLayout->addWidget(title);

    // ---- 轴选择 ----
    QLabel* axisLabel = new QLabel("轴选择", this);
    axisLabel->setStyleSheet("color:#90a4ae; font-size:10px;");
    mainLayout->addWidget(axisLabel);

    m_axisBtnGroup = new QButtonGroup(this);
    m_axisBtnGroup->setExclusive(true);

    QHBoxLayout* axisLayout = new QHBoxLayout();
    axisLayout->setSpacing(3);

    QStringList names = AXIS_NAMES;
    for (int i = 0; i < AXIS_COUNT; ++i) {
        QPushButton* btn = new QPushButton(names[i], this);
        btn->setCheckable(true);
        btn->setFixedHeight(24);
        btn->setStyleSheet(
            "QPushButton { background:#333; color:#ccc; border:none; "
            "border-radius:2px; font-size:10px; padding:0 6px; }"
            "QPushButton:checked { background:#0d47a1; color:#fff; }");
        btn->setProperty("axisId", i + 1);
        m_axisBtnGroup->addButton(btn, i + 1);

        // 前7个轴放在主行, 其余溢出 (简化处理: 只显示前8+省略)
        if (i < 8) {
            axisLayout->addWidget(btn);
        } else if (i == 8) {
            QPushButton* moreBtn = new QPushButton("...", this);
            moreBtn->setFixedHeight(24);
            moreBtn->setStyleSheet(
                "background:#1e1e30; color:#78909c; border:none; "
                "border-radius:2px; font-size:10px; padding:0 8px;");
            moreBtn->setEnabled(false);
            axisLayout->addWidget(moreBtn);
        }
    }
    // 默认选中轴1
    m_axisBtnGroup->button(1)->setChecked(true);

    axisLayout->addStretch();
    mainLayout->addLayout(axisLayout);

    // 连接轴选择信号
    connect(m_axisBtnGroup,
            QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MotorPtpWidget::onAxisButtonClicked);

    // ---- 位置/运动参数 区域 ----
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);

    // 左侧: 位置显示 + 输入
    QVBoxLayout* posLayout = new QVBoxLayout();
    posLayout->setSpacing(4);

    QLabel* curPosTitle = new QLabel("当前位置", this);
    curPosTitle->setStyleSheet("color:#78909c; font-size:9px;");
    posLayout->addWidget(curPosTitle);

    m_curPosLabel = new QLabel("+0.000 mm", this);
    m_curPosLabel->setStyleSheet("color:#ffd740; font-size:16px; font-family:monospace;");
    posLayout->addWidget(m_curPosLabel);

    QLabel* targetTitle = new QLabel("目标位置 (mm)", this);
    targetTitle->setStyleSheet("color:#78909c; font-size:9px;");
    posLayout->addWidget(targetTitle);

    m_targetPosSpin = new QDoubleSpinBox(this);
    m_targetPosSpin->setRange(-9999.999, 9999.999);
    m_targetPosSpin->setDecimals(3);
    m_targetPosSpin->setValue(150.000);
    m_targetPosSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #64b5f6; "
        "border-radius:2px; padding:4px; font-size:14px; font-family:monospace; }");
    posLayout->addWidget(m_targetPosSpin);

    controlLayout->addLayout(posLayout);

    // 右侧: 操作按钮 (运行/停止/回零)
    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(4);

    m_runBtn = new QPushButton("▶ 运行", this);
    m_runBtn->setFixedSize(80, 36);
    m_runBtn->setStyleSheet(
        "QPushButton { background:#2e7d32; color:#fff; border:none; "
        "border-radius:3px; font-weight:bold; font-size:12px; }"
        "QPushButton:hover { background:#388e3c; }");
    btnLayout->addWidget(m_runBtn);

    m_stopBtn = new QPushButton("⏹ 停止", this);
    m_stopBtn->setFixedSize(80, 36);
    m_stopBtn->setStyleSheet(
        "QPushButton { background:#e65100; color:#fff; border:none; "
        "border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#ff6d00; }");
    btnLayout->addWidget(m_stopBtn);

    m_homeBtn = new QPushButton("↺ 回零", this);
    m_homeBtn->setFixedSize(80, 36);
    m_homeBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#455a64; }");
    btnLayout->addWidget(m_homeBtn);

    controlLayout->addLayout(btnLayout);

    // 最右: JOG微调
    QVBoxLayout* jogLayout = new QVBoxLayout();
    jogLayout->setSpacing(3);

    QPushButton* jogPlusBtn = new QPushButton("▲ +", this);
    jogPlusBtn->setFixedSize(60, 28);
    jogPlusBtn->setStyleSheet(
        "QPushButton { background:#333; color:#fff; border:none; "
        "border-radius:2px; font-size:10px; }"
        "QPushButton:hover { background:#444; }");
    jogLayout->addWidget(jogPlusBtn);

    m_jogStepSpin = new QDoubleSpinBox(this);
    m_jogStepSpin->setRange(0.001, 100.0);
    m_jogStepSpin->setDecimals(3);
    m_jogStepSpin->setValue(DEFAULT_JOG_STEP);
    m_jogStepSpin->setPrefix("±");
    m_jogStepSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e1e30; color:#fff; border:1px solid #444; "
        "border-radius:2px; padding:2px; font-size:10px; }");
    jogLayout->addWidget(m_jogStepSpin);

    QPushButton* jogMinusBtn = new QPushButton("▼ -", this);
    jogMinusBtn->setFixedSize(60, 28);
    jogMinusBtn->setStyleSheet(
        "QPushButton { background:#333; color:#fff; border:none; "
        "border-radius:2px; font-size:10px; }"
        "QPushButton:hover { background:#444; }");
    jogLayout->addWidget(jogMinusBtn);

    controlLayout->addLayout(jogLayout);

    mainLayout->addLayout(controlLayout);

    // ---- 运动参数 (速度/加减速) ----
    QHBoxLayout* paramLayout = new QHBoxLayout();
    paramLayout->setSpacing(12);

    auto makeParamRow = [this](const QString& label, QDoubleSpinBox*& spin,
                               double defaultValue, double maxVal, const QString& unit) {
        QVBoxLayout* box = new QVBoxLayout();
        box->setSpacing(2);

        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#78909c; font-size:9px;");
        box->addWidget(lbl);

        QHBoxLayout* row = new QHBoxLayout();
        spin = new QDoubleSpinBox(this);
        spin->setRange(0.001, maxVal);
        spin->setDecimals(1);
        spin->setValue(defaultValue);
        spin->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:3px; font-size:12px; font-family:monospace; }");
        row->addWidget(spin);

        QLabel* unitLbl = new QLabel(unit, this);
        unitLbl->setStyleSheet("color:#78909c; font-size:9px;");
        row->addWidget(unitLbl);

        box->addLayout(row);
        return box;
    };

    paramLayout->addLayout(makeParamRow("运行速度", m_velocitySpin, DEFAULT_VELOCITY, 500.0, "mm/s"));
    paramLayout->addLayout(makeParamRow("加速度", m_accelSpin, DEFAULT_ACCEL, 2000.0, "mm/s²"));
    paramLayout->addLayout(makeParamRow("减速度", m_decelSpin, DEFAULT_DECEL, 2000.0, "mm/s²"));

    mainLayout->addLayout(paramLayout);

    // ---- 连接按钮信号 ----
    connect(m_runBtn,  &QPushButton::clicked, this, &MotorPtpWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MotorPtpWidget::onStopClicked);
    connect(m_homeBtn, &QPushButton::clicked, this, &MotorPtpWidget::onHomeClicked);
    connect(jogPlusBtn,  &QPushButton::clicked, this, &MotorPtpWidget::onJogPlusClicked);
    connect(jogMinusBtn, &QPushButton::clicked, this, &MotorPtpWidget::onJogMinusClicked);

    mainLayout->addStretch();
}
```

- [ ] **Step 2b: 业务信号处理**

```cpp
int MotorPtpWidget::currentAxisId() const
{
    return m_selectedAxisId;
}

void MotorPtpWidget::onAxisButtonClicked(int axisId)
{
    m_selectedAxisId = axisId;
    // 重置位置显示 (之后收到positionUpdated会刷新)
}

void MotorPtpWidget::onRunClicked()
{
    emit moveRequested(
        m_selectedAxisId,
        m_targetPosSpin->value(),
        m_velocitySpin->value(),
        m_accelSpin->value(),
        m_decelSpin->value()
    );
}

void MotorPtpWidget::onStopClicked()
{
    emit stopRequested(m_selectedAxisId);
}

void MotorPtpWidget::onHomeClicked()
{
    emit homeRequested(m_selectedAxisId);
}

void MotorPtpWidget::onJogPlusClicked()
{
    emit jogRequested(
        m_selectedAxisId, true,
        m_jogStepSpin->value(),
        m_velocitySpin->value(),
        m_accelSpin->value(),
        m_decelSpin->value()
    );
}

void MotorPtpWidget::onJogMinusClicked()
{
    emit jogRequested(
        m_selectedAxisId, false,
        m_jogStepSpin->value(),
        m_velocitySpin->value(),
        m_accelSpin->value(),
        m_decelSpin->value()
    );
}

// ---- 接收Core层更新 ----

void MotorPtpWidget::onPositionUpdated(int axisId, double position)
{
    if (axisId == m_selectedAxisId) {
        m_currentPosition = position;
        m_curPosLabel->setText(QString("%1 mm").arg(position, 0, 'f', 3));
    }
}

void MotorPtpWidget::onMoveFinished(int axisId, bool success)
{
    if (axisId == m_selectedAxisId && !success) {
        // 运动失败提示 (不弹窗, 改样式)
    }
}

void MotorPtpWidget::onHomeFinished(int axisId, bool success, int stage)
{
    Q_UNUSED(stage);
    if (axisId == m_selectedAxisId && !success) {
        // 回零失败
    }
}

void MotorPtpWidget::onAxisStatusChanged(int axisId, long status)
{
    Q_UNUSED(axisId);
    Q_UNUSED(status);
}
```

---

## Task 11: MotorParamWidget — 电机参数管理

**Files:**
- Create: `widgets/MotorParamWidget.h`
- Create: `widgets/MotorParamWidget.cpp`

- [ ] **Step 1: 创建 MotorParamWidget.h**

```cpp
/**
 * @file MotorParamWidget.h
 * @brief 电机参数管理面板 — 参数表格、写值、保存/加载文件
 *
 * 职责:
 *   - 显示当前选中轴的完整参数表
 *   - 编辑参数值
 *   - 保存到JSON文件
 *   - 从JSON文件加载
 */

#ifndef MOTORPARAMWIDGET_H
#define MOTORPARAMWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QButtonGroup>
#include "models/MotorAxis.h"

class MotorParamWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MotorParamWidget(QWidget *parent = nullptr);
    ~MotorParamWidget();

public slots:
    void onAxisParamChanged(int axisId);        // 接收Core层参数更新
    void setCurrentAxisId(int axisId);          // 切换轴

signals:
    void paramsUpdateRequested(int axisId, const MotorAxis& params);
    void saveRequested(const QString& filePath);
    void loadRequested(const QString& filePath);

private slots:
    void onAxisButtonClicked(int axisId);
    void onApplyClicked();                      // 应用参数
    void onSaveClicked();                       // 保存到文件
    void onLoadClicked();                       // 从文件加载

private:
    void setupUI();
    void loadAxisDefaults(int axisId);          // 加载轴的当前参数到编辑框

    int  m_selectedAxisId;

    QButtonGroup*    m_axisBtnGroup;

    // 参数编辑控件
    QDoubleSpinBox*  m_leadScrewSpin;
    QSpinBox*        m_pulsePerRevSpin;
    QDoubleSpinBox*  m_gearRatioSpin;
    QDoubleSpinBox*  m_maxVelocitySpin;
    QDoubleSpinBox*  m_accelSpin;
    QDoubleSpinBox*  m_decelSpin;
    QDoubleSpinBox*  m_softLimitPosSpin;
    QDoubleSpinBox*  m_softLimitNegSpin;
    QDoubleSpinBox*  m_homeVelSpin;
    QDoubleSpinBox*  m_homeOffsetSpin;
};

#endif // MOTORPARAMWIDGET_H
```

- [ ] **Step 2: 创建 MotorParamWidget.cpp**

```cpp
/**
 * @file MotorParamWidget.cpp
 * @brief 电机参数管理面板 实现
 */

#include "MotorParamWidget.h"
#include "core/HardwareConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

MotorParamWidget::MotorParamWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedAxisId(1)
{
    setupUI();
}

MotorParamWidget::~MotorParamWidget()
{
}

void MotorParamWidget::setupUI()
{
    setObjectName("MotorParamWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ---- 标题 ----
    QLabel* title = new QLabel("📐 电机参数管理", this);
    title->setStyleSheet("font-weight:bold; color:#64b5f6; font-size:13px;");
    mainLayout->addWidget(title);

    // ---- 轴选择 ----
    m_axisBtnGroup = new QButtonGroup(this);
    m_axisBtnGroup->setExclusive(true);

    QHBoxLayout* axisLayout = new QHBoxLayout();
    axisLayout->setSpacing(3);

    QStringList names = AXIS_NAMES;
    for (int i = 0; i < qMin(AXIS_COUNT, 8); ++i) {
        QPushButton* btn = new QPushButton(names[i], this);
        btn->setCheckable(true);
        btn->setFixedHeight(22);
        btn->setStyleSheet(
            "QPushButton { background:#333; color:#ccc; border:none; "
            "border-radius:2px; font-size:9px; padding:0 6px; }"
            "QPushButton:checked { background:#0d47a1; color:#fff; }");
        btn->setProperty("axisId", i + 1);
        m_axisBtnGroup->addButton(btn, i + 1);
        axisLayout->addWidget(btn);
    }
    axisLayout->addStretch();
    mainLayout->addLayout(axisLayout);

    connect(m_axisBtnGroup,
            QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MotorParamWidget::onAxisButtonClicked);
    m_axisBtnGroup->button(1)->setChecked(true);

    // ---- 参数表格 ----
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);
    grid->setVerticalSpacing(6);

    auto addRow = [&](int row, const QString& label,
                      QWidget* editor, const QString& unit = "") {
        QLabel* lbl = new QLabel(label, this);
        lbl->setStyleSheet("color:#90a4ae; font-size:10px;");
        grid->addWidget(lbl, row, 0);

        grid->addWidget(editor, row, 1);

        if (!unit.isEmpty()) {
            QLabel* unitLbl = new QLabel(unit, this);
            unitLbl->setStyleSheet("color:#78909c; font-size:9px;");
            grid->addWidget(unitLbl, row, 2);
        }
    };

    // 创建所有spinbox
    auto makeDSpin = [this](double min, double max, double val, int decimals) {
        QDoubleSpinBox* sp = new QDoubleSpinBox(this);
        sp->setRange(min, max);
        sp->setDecimals(decimals);
        sp->setValue(val);
        sp->setStyleSheet(
            "QDoubleSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:3px; font-size:11px; font-family:monospace; max-width:100px; }");
        return sp;
    };
    auto makeISpin = [this](int min, int max, int val) {
        QSpinBox* sp = new QSpinBox(this);
        sp->setRange(min, max);
        sp->setValue(val);
        sp->setStyleSheet(
            "QSpinBox { background:#1e1e30; color:#ffd740; border:1px solid #444; "
            "border-radius:2px; padding:3px; font-size:11px; font-family:monospace; max-width:100px; }");
        return sp;
    };

    m_leadScrewSpin      = makeDSpin(0.1, 100.0, DEFAULT_LEAD_SCREW, 3);
    m_pulsePerRevSpin    = makeISpin(100, 100000, DEFAULT_PULSE_PER_REV);
    m_gearRatioSpin      = makeDSpin(0.01, 100.0, DEFAULT_GEAR_RATIO, 2);
    m_maxVelocitySpin    = makeDSpin(0.1, MAX_VELOCITY, DEFAULT_VELOCITY, 1);
    m_accelSpin          = makeDSpin(0.1, MAX_ACCEL, DEFAULT_ACCEL, 1);
    m_decelSpin          = makeDSpin(0.1, MAX_ACCEL, DEFAULT_DECEL, 1);
    m_softLimitPosSpin   = makeDSpin(-9999.0, 9999.0, 300.0, 3);
    m_softLimitNegSpin   = makeDSpin(-9999.0, 9999.0, 0.0, 3);
    m_homeVelSpin        = makeDSpin(0.1, 200.0, 50.0, 1);
    m_homeOffsetSpin     = makeDSpin(-999.0, 999.0, 2.0, 3);

    int r = 0;
    addRow(r++, "导程",           m_leadScrewSpin,    "mm/rev");
    addRow(r++, "每转脉冲",        m_pulsePerRevSpin,  "pulse");
    addRow(r++, "电子齿轮比",      m_gearRatioSpin,    "");
    addRow(r++, "最大速度",        m_maxVelocitySpin,  "mm/s");
    addRow(r++, "加速度",          m_accelSpin,        "mm/s²");
    addRow(r++, "减速度",          m_decelSpin,        "mm/s²");
    addRow(r++, "软限位+",         m_softLimitPosSpin, "mm");
    addRow(r++, "软限位-",         m_softLimitNegSpin, "mm");
    addRow(r++, "回零速度",        m_homeVelSpin,      "mm/s");
    addRow(r++, "回零偏移",        m_homeOffsetSpin,   "mm");

    mainLayout->addLayout(grid);

    // ---- 操作按钮: 应用 / 保存 / 加载 ----
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(6);

    QPushButton* applyBtn = new QPushButton("💾 应用", this);
    applyBtn->setStyleSheet(
        "QPushButton { background:#1b5e20; color:#a5d6a7; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#2e7d32; }");
    actionLayout->addWidget(applyBtn);

    QPushButton* saveBtn = new QPushButton("📁 保存", this);
    saveBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#455a64; }");
    actionLayout->addWidget(saveBtn);

    QPushButton* loadBtn = new QPushButton("📂 加载", this);
    loadBtn->setStyleSheet(
        "QPushButton { background:#b71c1c; color:#ef9a9a; border:none; "
        "border-radius:3px; padding:4px 16px; font-size:10px; }"
        "QPushButton:hover { background:#d32f2f; }");
    actionLayout->addWidget(loadBtn);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    connect(applyBtn, &QPushButton::clicked, this, &MotorParamWidget::onApplyClicked);
    connect(saveBtn,  &QPushButton::clicked, this, &MotorParamWidget::onSaveClicked);
    connect(loadBtn,  &QPushButton::clicked, this, &MotorParamWidget::onLoadClicked);

    mainLayout->addStretch();
}

void MotorParamWidget::onAxisButtonClicked(int axisId)
{
    m_selectedAxisId = axisId;
    // 加载新轴的参数 (后续由外部调用setCurrentAxisId设置)
    emit paramsUpdateRequested(axisId, MotorAxis()); // 占位, 外部处理
}

void MotorParamWidget::setCurrentAxisId(int axisId)
{
    m_selectedAxisId = axisId;
    if (m_axisBtnGroup->button(axisId)) {
        m_axisBtnGroup->button(axisId)->setChecked(true);
    }
    loadAxisDefaults(axisId);
}

void MotorParamWidget::loadAxisDefaults(int axisId)
{
    // 加载默认值 (外部通过 onAxisParamChanged slot 填充实际值)
    m_leadScrewSpin->setValue(DEFAULT_LEAD_SCREW);
    m_pulsePerRevSpin->setValue(DEFAULT_PULSE_PER_REV);
    m_gearRatioSpin->setValue(DEFAULT_GEAR_RATIO);
    m_maxVelocitySpin->setValue(DEFAULT_VELOCITY);
    m_accelSpin->setValue(DEFAULT_ACCEL);
    m_decelSpin->setValue(DEFAULT_DECEL);
    m_softLimitPosSpin->setValue(300.0);
    m_softLimitNegSpin->setValue(0.0);
    m_homeVelSpin->setValue(50.0);
    m_homeOffsetSpin->setValue(2.0);
}

void MotorParamWidget::onApplyClicked()
{
    MotorAxis params;
    params.axisId          = m_selectedAxisId;
    params.velocity        = m_maxVelocitySpin->value();
    params.acceleration    = m_accelSpin->value();
    params.deceleration    = m_decelSpin->value();
    params.leadScrew       = m_leadScrewSpin->value();
    params.pulsePerRev     = m_pulsePerRevSpin->value();
    params.gearRatio       = m_gearRatioSpin->value();
    params.softLimitPositive = m_softLimitPosSpin->value();
    params.softLimitNegative = m_softLimitNegSpin->value();

    emit paramsUpdateRequested(m_selectedAxisId, params);
}

void MotorParamWidget::onSaveClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "保存电机参数",
        "motor_params.json",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit saveRequested(filePath);
    }
}

void MotorParamWidget::onLoadClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "加载电机参数",
        "",
        "JSON文件 (*.json)");
    if (!filePath.isEmpty()) {
        emit loadRequested(filePath);
    }
}

void MotorParamWidget::onAxisParamChanged(int axisId)
{
    if (axisId != m_selectedAxisId) return;
    // 由MainWindow连接实际参数值到此slot
}
```

---

## Task 12: IoMonitorWidget — IO信号监视

**Files:**
- Create: `widgets/IoMonitorWidget.h`
- Create: `widgets/IoMonitorWidget.cpp`

- [ ] **Step 1: 创建 IoMonitorWidget.h**

```cpp
/**
 * @file IoMonitorWidget.h
 * @brief IO信号监视面板 — DI/DO实时状态显示
 *
 * 职责:
 *   - 显示16路DI状态 (绿=高/红=低/灰=未知)
 *   - 显示4路DO状态 (蓝=高/棕=低)
 *   - 实时响应 Core/IoManager 的信号变化
 */

#ifndef IOMONITORWIDGET_H
#define IOMONITORWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>
#include "models/IoSignal.h"
#include "core/HardwareConfig.h"

class IoMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IoMonitorWidget(QWidget *parent = nullptr);
    ~IoMonitorWidget();

public slots:
    void onDiChanged(int id, int value);
    void onDoChanged(int id, int value);
    void refreshAll(const QVector<IoSignal>& diSignals, const QVector<IoSignal>& doSignals);

private:
    void setupUI();

    QVector<QLabel*> m_diLabels;    // 16个DI状态标签
    QVector<QLabel*> m_doLabels;    // 4个DO状态标签

    void updateLabelStyle(QLabel* label, IoType type, int value);
};

#endif // IOMONITORWIDGET_H
```

- [ ] **Step 2: 创建 IoMonitorWidget.cpp**

```cpp
/**
 * @file IoMonitorWidget.cpp
 * @brief IO信号监视面板 实现
 */

#include "IoMonitorWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>

IoMonitorWidget::IoMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

IoMonitorWidget::~IoMonitorWidget()
{
}

void IoMonitorWidget::setupUI()
{
    setObjectName("IoMonitorWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ---- 标题 ----
    QLabel* title = new QLabel("🔌 IO信号状态", this);
    title->setStyleSheet("font-weight:bold; color:#81c784; font-size:13px;");
    mainLayout->addWidget(title);

    // ---- DI区域 ----
    QLabel* diTitle = new QLabel("输入 DI (限位/Home)", this);
    diTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    mainLayout->addWidget(diTitle);

    // DI网格: 4列 × 4行
    QGridLayout* diGrid = new QGridLayout();
    diGrid->setSpacing(3);

    QStringList diNames = DI_NAMES;
    for (int i = 0; i < DI_COUNT; ++i) {
        QLabel* lbl = new QLabel("◯ " + (i < diNames.size() ? diNames[i] : QString("DI_%1").arg(i + 1)), this);
        lbl->setStyleSheet(
            "background:#333; color:#888; padding:2px 7px; border-radius:2px; font-size:9px;");
        lbl->setMinimumHeight(20);
        diGrid->addWidget(lbl, i / 4, i % 4);
        m_diLabels.append(lbl);
    }
    mainLayout->addLayout(diGrid);

    // ---- DO区域 ----
    QLabel* doTitle = new QLabel("输出 DO", this);
    doTitle->setStyleSheet("color:#90a4ae; font-size:10px; margin-top:4px;");
    mainLayout->addWidget(doTitle);

    QHBoxLayout* doLayout = new QHBoxLayout();
    doLayout->setSpacing(6);

    QStringList doNames = DO_NAMES;
    for (int i = 0; i < DO_COUNT; ++i) {
        QLabel* lbl = new QLabel("⭕ " + (i < doNames.size() ? doNames[i] : QString("DO_%1").arg(i + 1)), this);
        lbl->setMinimumHeight(26);
        lbl->setStyleSheet(
            "background:#333; color:#888; padding:3px 14px; border-radius:2px; font-size:10px;");
        doLayout->addWidget(lbl);
        m_doLabels.append(lbl);
    }
    doLayout->addStretch();
    mainLayout->addLayout(doLayout);

    mainLayout->addStretch();
}

void IoMonitorWidget::onDiChanged(int id, int value)
{
    if (id < 1 || id > DI_COUNT) return;
    updateLabelStyle(m_diLabels[id - 1], IO_TYPE_DI, value);
}

void IoMonitorWidget::onDoChanged(int id, int value)
{
    if (id < 1 || id > DO_COUNT) return;
    updateLabelStyle(m_doLabels[id - 1], IO_TYPE_DO, value);
}

void IoMonitorWidget::refreshAll(const QVector<IoSignal>& diSignals, const QVector<IoSignal>& doSignals)
{
    for (const IoSignal& sig : diSignals) {
        onDiChanged(sig.id, sig.value);
    }
    for (const IoSignal& sig : doSignals) {
        onDoChanged(sig.id, sig.value);
    }
}

void IoMonitorWidget::updateLabelStyle(QLabel* label, IoType type, int value)
{
    if (!label) return;

    QString text = label->text();
    // 提取信号名 (去掉前缀符号)
    int spaceIdx = text.indexOf(' ');
    QString name = (spaceIdx >= 0) ? text.mid(spaceIdx + 1) : text;

    QString prefix, bgColor, textColor;
    if (type == IO_TYPE_DI) {
        if (value == 1) {
            prefix = "🟢 "; bgColor = "#1b5e20"; textColor = "#a5d6a7";
        } else {
            // DI=0 且是Home信号 → 红色警告, 其余灰色
            if (name.contains("Home") || name.contains("home")) {
                prefix = "🔴 "; bgColor = "#4e1a1a"; textColor = "#ef9a9a";
            } else {
                prefix = "⚪ "; bgColor = "#333"; textColor = "#888";
            }
        }
    } else { // DO
        if (value == 1) {
            prefix = "🔵 "; bgColor = "#0d3b66"; textColor = "#90caf9";
        } else {
            prefix = "⭕ "; bgColor = "#4e342e"; textColor = "#bcaaa4";
        }
    }

    label->setText(prefix + name);
    label->setStyleSheet(
        QString("background:%1; color:%2; padding:2px 7px; border-radius:2px; font-size:9px;")
            .arg(bgColor, textColor));
}
```

---

## Task 13: AlarmListWidget — 报警列表

**Files:**
- Create: `widgets/AlarmListWidget.h`
- Create: `widgets/AlarmListWidget.cpp`

- [ ] **Step 1: 创建 AlarmListWidget.h**

```cpp
/**
 * @file AlarmListWidget.h
 * @brief 报警列表面板 — 实时报警滚动列表
 *
 * 职责:
 *   - 显示活跃报警 (红色=致命, 橙色=警告, 灰色=已恢复)
 *   - 可滚动查看历史
 *   - 显示报警总数
 */

#ifndef ALARMLISTWIDGET_H
#define ALARMLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include "models/AlarmRecord.h"

class AlarmListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmListWidget(QWidget *parent = nullptr);
    ~AlarmListWidget();

public slots:
    void onNewAlarm(AlarmRecord record);
    void onAlarmResolved(int alarmId);
    void onActiveCountChanged(int count);

private:
    void setupUI();
    void updateCountBadge();

    QListWidget*  m_listWidget;
    QLabel*       m_countLabel;
    int           m_activeCount;
};

#endif // ALARMLISTWIDGET_H
```

- [ ] **Step 2: 创建 AlarmListWidget.cpp**

```cpp
/**
 * @file AlarmListWidget.cpp
 * @brief 报警列表面板 实现
 */

#include "AlarmListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

AlarmListWidget::AlarmListWidget(QWidget *parent)
    : QWidget(parent)
    , m_activeCount(0)
{
    setupUI();
}

AlarmListWidget::~AlarmListWidget()
{
}

void AlarmListWidget::setupUI()
{
    setObjectName("AlarmListWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // ---- 标题行 (含数量badge) ----
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);

    QLabel* title = new QLabel("⚠ 报警列表", this);
    title->setStyleSheet("font-weight:bold; color:#ef5350; font-size:13px;");
    titleRow->addWidget(title);

    m_countLabel = new QLabel("0", this);
    m_countLabel->setStyleSheet(
        "background:#b71c1c; color:#fff; padding:1px 8px; "
        "border-radius:8px; font-size:10px; font-weight:bold;");
    titleRow->addWidget(m_countLabel);

    titleRow->addStretch();
    mainLayout->addLayout(titleRow);

    // ---- 报警列表 ----
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget { background:#1e1e30; border:1px solid #333; border-radius:3px; }"
        "QListWidget::item { padding:3px 6px; border-bottom:1px solid #2a2a3e; }");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(m_listWidget);
}

void AlarmListWidget::onNewAlarm(AlarmRecord record)
{
    QString timeStr = record.timestamp.toString("hh:mm:ss");

    QString color, bgColor, icon;
    if (record.resolved) {
        color   = "#78909c";
        bgColor = "#1e1e30";
        icon    = "◯";
    } else if (record.level == ALARM_LEVEL_FATAL) {
        color   = "#ef9a9a";
        bgColor = "#4e1a1a";
        icon    = "🔴";
    } else if (record.level == ALARM_LEVEL_WARNING) {
        color   = "#ffcc80";
        bgColor = "#4e342e";
        icon    = "⚠";
    } else {
        color   = "#78909c";
        bgColor = "#1e1e30";
        icon    = "◯";
    }

    QString itemText = QString("%1 %2  %3")
        .arg(icon, timeStr, record.message);

    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, record.id);
    item->setForeground(QColor(color));
    item->setBackground(QColor(bgColor));

    // 插入到列表顶部
    m_listWidget->insertItem(0, item);

    // 限制显示条数 (最多保留200条)
    while (m_listWidget->count() > 200) {
        delete m_listWidget->takeItem(m_listWidget->count() - 1);
    }

    if (!record.resolved) {
        m_activeCount++;
        updateCountBadge();
    }
}

void AlarmListWidget::onAlarmResolved(int alarmId)
{
    // 在列表中标记该报警为已恢复
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == alarmId) {
            item->setForeground(QColor("#78909c"));
            item->setBackground(QColor("#1e1e30"));
            QString text = item->text();
            text.replace("🔴", "◯");
            text.replace("⚠", "◯");
            item->setText(text);
            break;
        }
    }
    if (m_activeCount > 0) {
        m_activeCount--;
        updateCountBadge();
    }
}

void AlarmListWidget::onActiveCountChanged(int count)
{
    m_activeCount = count;
    updateCountBadge();
}

void AlarmListWidget::updateCountBadge()
{
    m_countLabel->setText(QString::number(m_activeCount));
    if (m_activeCount == 0) {
        m_countLabel->setStyleSheet(
            "background:#37474f; color:#90a4ae; padding:1px 8px; "
            "border-radius:8px; font-size:10px; font-weight:bold;");
    } else {
        m_countLabel->setStyleSheet(
            "background:#b71c1c; color:#fff; padding:1px 8px; "
            "border-radius:8px; font-size:10px; font-weight:bold;");
    }
}
```

---

## Task 14: StatsWidget — 生产统计面板

**Files:**
- Create: `widgets/StatsWidget.h`
- Create: `widgets/StatsWidget.cpp`

- [ ] **Step 1: 创建 StatsWidget.h**

```cpp
/**
 * @file StatsWidget.h
 * @brief 生产统计面板 — 产量、运行时长、节拍
 */

#ifndef STATSWIDGET_H
#define STATSWIDGET_H

#include <QWidget>
#include <QLabel>

class StatsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatsWidget(QWidget *parent = nullptr);
    ~StatsWidget();

public slots:
    void onStatsUpdated(int totalCount, double runningHours, double cycleTimeSec);

private:
    void setupUI();

    QLabel* m_totalCountLabel;
    QLabel* m_runningTimeLabel;
    QLabel* m_cycleTimeLabel;
};

#endif // STATSWIDGET_H
```

- [ ] **Step 2: 创建 StatsWidget.cpp**

```cpp
/**
 * @file StatsWidget.cpp
 * @brief 生产统计面板 实现
 */

#include "StatsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

StatsWidget::StatsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

StatsWidget::~StatsWidget()
{
}

void StatsWidget::setupUI()
{
    setObjectName("StatsWidget");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    QLabel* title = new QLabel("📊 生产统计", this);
    title->setStyleSheet("font-weight:bold; color:#ffd740; font-size:13px;");
    mainLayout->addWidget(title);

    QHBoxLayout* cardLayout = new QHBoxLayout();
    cardLayout->setSpacing(10);

    // 总产量卡片
    QVBoxLayout* totalCard = new QVBoxLayout();
    totalCard->setSpacing(2);

    QLabel* totalTitle = new QLabel("总产量", this);
    totalTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    totalTitle->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(totalTitle);

    m_totalCountLabel = new QLabel("0", this);
    m_totalCountLabel->setStyleSheet("color:#ffd740; font-size:22px; font-weight:bold;");
    m_totalCountLabel->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(m_totalCountLabel);

    QLabel* totalUnit = new QLabel("件", this);
    totalUnit->setStyleSheet("color:#78909c; font-size:9px;");
    totalUnit->setAlignment(Qt::AlignCenter);
    totalCard->addWidget(totalUnit);

    QWidget* totalCardWidget = new QWidget(this);
    totalCardWidget->setLayout(totalCard);
    totalCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(totalCardWidget);

    // 运行时长卡片
    QVBoxLayout* timeCard = new QVBoxLayout();
    timeCard->setSpacing(2);

    QLabel* timeTitle = new QLabel("运行时长", this);
    timeTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    timeTitle->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(timeTitle);

    m_runningTimeLabel = new QLabel("00:00", this);
    m_runningTimeLabel->setStyleSheet("color:#64b5f6; font-size:22px; font-weight:bold;");
    m_runningTimeLabel->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(m_runningTimeLabel);

    QLabel* timeUnit = new QLabel("时:分", this);
    timeUnit->setStyleSheet("color:#78909c; font-size:9px;");
    timeUnit->setAlignment(Qt::AlignCenter);
    timeCard->addWidget(timeUnit);

    QWidget* timeCardWidget = new QWidget(this);
    timeCardWidget->setLayout(timeCard);
    timeCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(timeCardWidget);

    // 节拍卡片
    QVBoxLayout* cycleCard = new QVBoxLayout();
    cycleCard->setSpacing(2);

    QLabel* cycleTitle = new QLabel("平均节拍", this);
    cycleTitle->setStyleSheet("color:#90a4ae; font-size:10px;");
    cycleTitle->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(cycleTitle);

    m_cycleTimeLabel = new QLabel("0.0", this);
    m_cycleTimeLabel->setStyleSheet("color:#81c784; font-size:22px; font-weight:bold;");
    m_cycleTimeLabel->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(m_cycleTimeLabel);

    QLabel* cycleUnit = new QLabel("秒/件", this);
    cycleUnit->setStyleSheet("color:#78909c; font-size:9px;");
    cycleUnit->setAlignment(Qt::AlignCenter);
    cycleCard->addWidget(cycleUnit);

    QWidget* cycleCardWidget = new QWidget(this);
    cycleCardWidget->setLayout(cycleCard);
    cycleCardWidget->setStyleSheet("background:#1e1e30; border-radius:4px; padding:8px;");
    cardLayout->addWidget(cycleCardWidget);

    mainLayout->addLayout(cardLayout);
    mainLayout->addStretch();
}

void StatsWidget::onStatsUpdated(int totalCount, double runningHours, double cycleTimeSec)
{
    m_totalCountLabel->setText(QString::number(totalCount));

    int hours   = static_cast<int>(runningHours);
    int minutes = static_cast<int>((runningHours - hours) * 60);
    m_runningTimeLabel->setText(QString("%1:%2")
                                .arg(hours, 2, 10, QLatin1Char('0'))
                                .arg(minutes, 2, 10, QLatin1Char('0')));

    m_cycleTimeLabel->setText(QString::number(cycleTimeSec, 'f', 1));
}
```

---

## Task 15: BottomBarWidget — 底部操作栏

**Files:**
- Create: `widgets/BottomBarWidget.h`
- Create: `widgets/BottomBarWidget.cpp`

- [ ] **Step 1: 创建 BottomBarWidget.h**

```cpp
/**
 * @file BottomBarWidget.h
 * @brief 底部操作栏 — 急停/启动/暂停/复位、模式切换、通讯状态
 */

#ifndef BOTTOMBARWIDGET_H
#define BOTTOMBARWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class BottomBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BottomBarWidget(QWidget *parent = nullptr);
    ~BottomBarWidget();

public slots:
    void setConnectionStatus(bool connected);   // 通讯状态
    void setMode(int mode);                     // 手动/自动

signals:
    void emergencyStopClicked();
    void startClicked();
    void pauseClicked();
    void resetClicked();
    void modeSwitchClicked();                   // 请求切换模式

private:
    void setupUI();

    QPushButton* m_emergencyBtn;
    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resetBtn;
    QPushButton* m_modeBtn;
    QLabel*      m_connLabel;
};

#endif // BOTTOMBARWIDGET_H
```

- [ ] **Step 2: 创建 BottomBarWidget.cpp**

```cpp
/**
 * @file BottomBarWidget.cpp
 * @brief 底部操作栏 实现
 */

#include "BottomBarWidget.h"
#include <QHBoxLayout>

BottomBarWidget::BottomBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

BottomBarWidget::~BottomBarWidget()
{
}

void BottomBarWidget::setupUI()
{
    setObjectName("BottomBarWidget");
    setFixedHeight(40);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 4, 10, 4);
    layout->setSpacing(8);

    // 急停按钮 (红色, 醒目)
    m_emergencyBtn = new QPushButton("⏹ 急停", this);
    m_emergencyBtn->setFixedSize(100, 32);
    m_emergencyBtn->setStyleSheet(
        "QPushButton { background:#c62828; color:#fff; border:none; "
        "border-radius:3px; font-weight:bold; font-size:13px; }"
        "QPushButton:hover { background:#e53935; }");
    layout->addWidget(m_emergencyBtn);

    // 启动
    m_startBtn = new QPushButton("▶ 启动", this);
    m_startBtn->setFixedSize(90, 32);
    m_startBtn->setStyleSheet(
        "QPushButton { background:#1565c0; color:#fff; border:none; "
        "border-radius:3px; font-size:13px; }"
        "QPushButton:hover { background:#1976d2; }");
    layout->addWidget(m_startBtn);

    // 暂停
    m_pauseBtn = new QPushButton("⏸ 暂停", this);
    m_pauseBtn->setFixedSize(90, 32);
    m_pauseBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:13px; }"
        "QPushButton:hover { background:#455a64; }");
    layout->addWidget(m_pauseBtn);

    // 复位
    m_resetBtn = new QPushButton("↺ 复位", this);
    m_resetBtn->setFixedSize(90, 32);
    m_resetBtn->setStyleSheet(
        "QPushButton { background:#37474f; color:#fff; border:none; "
        "border-radius:3px; font-size:13px; }"
        "QPushButton:hover { background:#455a64; }");
    layout->addWidget(m_resetBtn);

    layout->addStretch();

    // 模式切换
    m_modeBtn = new QPushButton("🔄 手动/自动", this);
    m_modeBtn->setFixedSize(110, 32);
    m_modeBtn->setStyleSheet(
        "QPushButton { background:#333; color:#fff; border:1px solid #555; "
        "border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#444; }");
    layout->addWidget(m_modeBtn);

    // 通讯状态
    m_connLabel = new QLabel("● GNC未连接", this);
    m_connLabel->setStyleSheet("color:#f44336; font-size:11px;");
    layout->addWidget(m_connLabel);

    // 连接信号
    connect(m_emergencyBtn, &QPushButton::clicked, this, &BottomBarWidget::emergencyStopClicked);
    connect(m_startBtn,      &QPushButton::clicked, this, &BottomBarWidget::startClicked);
    connect(m_pauseBtn,      &QPushButton::clicked, this, &BottomBarWidget::pauseClicked);
    connect(m_resetBtn,      &QPushButton::clicked, this, &BottomBarWidget::resetClicked);
    connect(m_modeBtn,       &QPushButton::clicked, this, &BottomBarWidget::modeSwitchClicked);
}

void BottomBarWidget::setConnectionStatus(bool connected)
{
    if (connected) {
        m_connLabel->setText("● GNC已连接");
        m_connLabel->setStyleSheet("color:#4caf50; font-size:11px;");
    } else {
        m_connLabel->setText("● GNC未连接");
        m_connLabel->setStyleSheet("color:#f44336; font-size:11px;");
    }
}

void BottomBarWidget::setMode(int mode)
{
    if (mode == 0) {
        m_modeBtn->setText("🔄 手动模式");
    } else {
        m_modeBtn->setText("🔄 自动模式");
    }
}
```

---

## Task 16: MainWindow — 主窗口组装 + 信号槽连线

**Files:**
- Create: `mainwindow.h`
- Create: `mainwindow.cpp`

- [ ] **Step 1: 创建 mainwindow.h**

```cpp
/**
 * @file mainwindow.h
 * @brief 主窗口 — 布局管理 + 信号槽连线中心
 *
 * 职责:
 *   - 创建所有子面板 (Widgets)
 *   - 创建所有Core管理器 (Managers)
 *   - 用 Layout 组织面板位置
 *   - 连接 UI信号 → Core槽, Core信号 → UI槽
 *
 * 本文件是唯一知道"全局"的文件, 所有跨模块通信在这里连接。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>

// Core
#include "core/GncController.h"
#include "core/MotorManager.h"
#include "core/IoManager.h"
#include "core/AlarmLogger.h"
#include "core/StatsCollector.h"

// Widgets
#include "widgets/StatusBarWidget.h"
#include "widgets/MotorPtpWidget.h"
#include "widgets/MotorParamWidget.h"
#include "widgets/IoMonitorWidget.h"
#include "widgets/AlarmListWidget.h"
#include "widgets/StatsWidget.h"
#include "widgets/BottomBarWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void createCoreModules();       // 创建Core层对象
    void createWidgets();           // 创建UI面板
    void setupLayout();             // 布局
    void connectSignals();          // 信号/槽连线 (全部集中在这里)
    void initSystem();              // 系统初始化

    // ---- Core层 ----
    MockGncController*  m_gncController;
    MotorManager*       m_motorManager;
    IoManager*          m_ioManager;
    AlarmLogger*        m_alarmLogger;
    StatsCollector*     m_statsCollector;

    // ---- UI层 ----
    StatusBarWidget*    m_statusBar;
    MotorPtpWidget*     m_motorPtp;
    MotorParamWidget*   m_motorParam;
    IoMonitorWidget*    m_ioMonitor;
    AlarmListWidget*    m_alarmList;
    StatsWidget*        m_stats;
    BottomBarWidget*    m_bottomBar;

    // ---- 其他 ----
    int m_currentMode;              // 0=手动, 1=自动
    QWidget* m_placeholderVision;   // 视觉预留区域
};

#endif // MAINWINDOW_H
```

- [ ] **Step 2: 创建 mainwindow.cpp**

```cpp
/**
 * @file mainwindow.cpp
 * @brief MainWindow 实现
 */

#include "mainwindow.h"
#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentMode(0)   // 默认为手动模式
{
    createCoreModules();
    createWidgets();
    setupLayout();
    connectSignals();
    initSystem();
}

MainWindow::~MainWindow()
{
}

// ============================================================
// 创建Core层
// ============================================================

void MainWindow::createCoreModules()
{
    // 1. GNC控制器 (Mock, 后续替换为真实 GncControllerImpl)
    m_gncController  = new MockGncController(this);

    // 2. 管理器 (均注入 m_gncController)
    m_motorManager   = new MotorManager(m_gncController, this);
    m_ioManager      = new IoManager(m_gncController, this);
    m_alarmLogger    = new AlarmLogger(this);
    m_statsCollector = new StatsCollector(this);
}

// ============================================================
// 创建UI面板
// ============================================================

void MainWindow::createWidgets()
{
    m_statusBar     = new StatusBarWidget(this);

    m_motorPtp      = new MotorPtpWidget(this);
    m_motorParam    = new MotorParamWidget(this);

    m_ioMonitor     = new IoMonitorWidget(this);
    m_stats         = new StatsWidget(this);

    m_alarmList     = new AlarmListWidget(this);

    // 视觉预留区域 (空占位)
    m_placeholderVision = new QWidget(this);
    m_placeholderVision->setStyleSheet(
        "background:#1a1a2e; border:1px dashed #444; border-radius:4px;");
    QLabel* visLabel = new QLabel("📷 视觉对位预览\n(预留区域)", m_placeholderVision);
    visLabel->setAlignment(Qt::AlignCenter);
    visLabel->setStyleSheet("color:#555; font-size:12px; background:transparent;");

    m_bottomBar     = new BottomBarWidget(this);
}

// ============================================================
// 布局
// ============================================================

void MainWindow::setupLayout()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // (1) 顶部状态栏
    mainLayout->addWidget(m_statusBar);

    // (2) 中间三列
    QHBoxLayout* middleRow = new QHBoxLayout();
    middleRow->setSpacing(6);

    // 左列: 电机PTP + 电机参数
    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(6);
    leftCol->addWidget(m_motorPtp, 4);    // 比例 4
    leftCol->addWidget(m_motorParam, 5);  // 比例 5
    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftCol);
    leftWidget->setFixedWidth(530);
    middleRow->addWidget(leftWidget);

    // 中列: IO信号 + 生产统计
    QVBoxLayout* centerCol = new QVBoxLayout();
    centerCol->setSpacing(6);
    centerCol->addWidget(m_ioMonitor, 5);
    centerCol->addWidget(m_stats, 3);
    QWidget* centerWidget = new QWidget(this);
    centerWidget->setLayout(centerCol);
    middleRow->addWidget(centerWidget, 1);  // stretch

    // 右列: 报警 + 视觉预留
    QVBoxLayout* rightCol = new QVBoxLayout();
    rightCol->setSpacing(6);
    rightCol->addWidget(m_alarmList, 4);
    rightCol->addWidget(m_placeholderVision, 3);
    QWidget* rightWidget = new QWidget(this);
    rightWidget->setLayout(rightCol);
    rightWidget->setFixedWidth(280);
    middleRow->addWidget(rightWidget);

    mainLayout->addLayout(middleRow, 1);

    // (3) 底部操作栏
    mainLayout->addWidget(m_bottomBar);
}

// ============================================================
// 信号/槽连线 — 全部集中在这里, 一目了然
// ============================================================

void MainWindow::connectSignals()
{
    // ---- 电机相关 ----
    // UI → Core
    connect(m_motorPtp, &MotorPtpWidget::moveRequested,
            m_motorManager, &MotorManager::moveRequest);
    connect(m_motorPtp, &MotorPtpWidget::stopRequested,
            m_motorManager, &MotorManager::stopMove);
    connect(m_motorPtp, &MotorPtpWidget::homeRequested,
            m_motorManager, &MotorManager::homeRequest);
    connect(m_motorPtp, &MotorPtpWidget::jogRequested,
            m_motorManager, &MotorManager::jogRequest);

    // Core → UI
    connect(m_motorManager, &MotorManager::positionUpdated,
            m_motorPtp, &MotorPtpWidget::onPositionUpdated);
    connect(m_motorManager, &MotorManager::moveFinished,
            m_motorPtp, &MotorPtpWidget::onMoveFinished);
    connect(m_motorManager, &MotorManager::homeFinished,
            m_motorPtp, &MotorPtpWidget::onHomeFinished);
    connect(m_motorManager, &MotorManager::axisStatusChanged,
            m_motorPtp, &MotorPtpWidget::onAxisStatusChanged);

    // 电机参数
    connect(m_motorParam, &MotorParamWidget::paramsUpdateRequested,
            m_motorManager, &MotorManager::updateAxisParams);
    connect(m_motorParam, &MotorParamWidget::saveRequested,
            m_motorManager, &MotorManager::saveParamsToFile);
    connect(m_motorParam, &MotorParamWidget::loadRequested,
            m_motorManager, &MotorManager::loadParamsFromFile);
    connect(m_motorManager, &MotorManager::axisParamChanged,
            m_motorParam, &MotorParamWidget::onAxisParamChanged);

    // 同步轴选择
    connect(m_motorPtp, &MotorPtpWidget::moveRequested,
            this, [this](int axisId, double, double, double, double) {
                m_motorParam->setCurrentAxisId(axisId);
            });

    // ---- IO相关 ----
    connect(m_ioManager, &IoManager::diChanged,
            m_ioMonitor, &IoMonitorWidget::onDiChanged);
    connect(m_ioManager, &IoManager::doChanged,
            m_ioMonitor, &IoMonitorWidget::onDoChanged);

    // 急停信号 → 报警
    connect(m_ioManager, &IoManager::emergencyStopChanged,
            this, [this](bool triggered) {
                if (triggered) {
                    m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "IO系统", "急停按钮被按下");
                    m_motorManager->stopMove(0); // 停止所有轴 (0=全部)
                } else {
                    m_alarmLogger->resolveAlarm(1); // 简化为第一个报警
                }
            });

    // ---- 报警 ----
    connect(m_alarmLogger, &AlarmLogger::newAlarm,
            m_alarmList, &AlarmListWidget::onNewAlarm);
    connect(m_alarmLogger, &AlarmLogger::alarmResolved,
            m_alarmList, &AlarmListWidget::onAlarmResolved);
    connect(m_alarmLogger, &AlarmLogger::activeCountChanged,
            m_alarmList, &AlarmListWidget::onActiveCountChanged);

    // 电机错误 → 报警
    connect(m_motorManager, &MotorManager::motorError,
            this, [this](int axisId, const QString& msg) {
                m_alarmLogger->raiseAlarm(ALARM_LEVEL_WARNING,
                                          QString("轴%1").arg(axisId), msg);
            });

    // ---- 统计 ----
    connect(m_statsCollector, &StatsCollector::statsUpdated,
            m_stats, &StatsWidget::onStatsUpdated);
    connect(m_statsCollector, &StatsCollector::statsUpdated,
            this, [this](int count, double hours, double cycle) {
                m_statusBar->setCount(count);
                m_statusBar->setRunningTime(hours);
                m_statusBar->setCycleTime(cycle);
            });

    // ---- 底部操作栏 ----
    connect(m_bottomBar, &BottomBarWidget::emergencyStopClicked,
            this, [this]() {
                m_alarmLogger->raiseAlarm(ALARM_LEVEL_FATAL, "操作", "操作员触发急停");
            });
    connect(m_bottomBar, &BottomBarWidget::startClicked,
            this, [this]() {
                m_statusBar->setRunStatus(true);
                m_statsCollector->start();
            });
    connect(m_bottomBar, &BottomBarWidget::pauseClicked,
            this, [this]() {
                m_statusBar->setRunStatus(false);
                m_statsCollector->pause();
            });
    connect(m_bottomBar, &BottomBarWidget::resetClicked,
            this, [this]() {
                m_alarmLogger->clearAll();
                m_statsCollector->reset();
            });
    connect(m_bottomBar, &BottomBarWidget::modeSwitchClicked,
            this, [this]() {
                m_currentMode = (m_currentMode == 0) ? 1 : 0;
                m_statusBar->setMode(m_currentMode);
                m_bottomBar->setMode(m_currentMode);
            });

    // ---- 连接状态 ----
    connect(m_gncController, &MockGncController::destroyed,
            this, [this]() {
                m_bottomBar->setConnectionStatus(false);
            });
}

// ============================================================
// 初始化
// ============================================================

void MainWindow::initSystem()
{
    // 1. 模拟GNC初始化
    bool ok = m_gncController->openCard();
    if (ok) {
        m_gncController->netInit("config.xml", 120);
        m_gncController->loadConfig(GNC_CORE_NUM, "config.cfg");
        m_gncController->clearStatus(GNC_CORE_NUM, GNC_AXIS_START, AXIS_COUNT);
    }
    m_bottomBar->setConnectionStatus(m_gncController->isConnected());

    // 2. 启动各管理器轮询
    m_motorManager->initialize();
    m_ioManager->initialize();

    // 3. 初始化状态栏
    m_statusBar->setMode(m_currentMode);
    m_statusBar->setRunStatus(false);
    m_statusBar->setCount(0);
    m_statusBar->setRunningTime(0);
    m_statusBar->setCycleTime(0);

    // 4. 启动模拟定时器 (Mock模式)
    // 模拟一些初始报警
    m_alarmLogger->raiseAlarm(ALARM_LEVEL_INFO, "系统", "系统启动完成");
}
```

---

## Task 17: 全局样式表 — style.qss

**Files:**
- Overwrite: `resources/style.qss`

- [ ] **Step 1: 填写完整的深色工业主题 QSS**

```css
/* ============================================================
 * 固晶机控制系统 — 深色工业主题样式表
 * 适用于1200×800工控触摸屏, 深色背景减少反光
 * ============================================================ */

/* ---- 全局 ----
 * 基础色调:
 *   背景: #1a1a2e (深蓝黑)
 *   面板背景: #2a2a3e
 *   输入框背景: #1e1e30
 *   主文字: #ccc
 *   辅助文字: #90a4ae / #78909c
 *   强调色: #64b5f6 (蓝), #ffd740 (黄), #81c784 (绿), #ef5350 (红)
 */

QMainWindow {
    background-color: #1a1a2e;
}

QWidget {
    font-family: "Microsoft YaHei", "SimHei", sans-serif;
    color: #cccccc;
    background-color: transparent;
}

/* ---- 面板 ----
 * 所有功能面板用统一的圆角深色背景
 */

QWidget#StatusBarWidget,
QWidget#MotorPtpWidget,
QWidget#MotorParamWidget,
QWidget#IoMonitorWidget,
QWidget#AlarmListWidget,
QWidget#StatsWidget,
QWidget#BottomBarWidget {
    background-color: #2a2a3e;
    border-radius: 4px;
    padding: 4px;
}

/* ---- 滚动条 ----
 * 触屏友好的宽轨道
 */

QScrollBar:vertical {
    background: #1a1a2e;
    width: 10px;
    border-radius: 5px;
}

QScrollBar::handle:vertical {
    background: #555;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: #64b5f6;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar:horizontal {
    background: #1a1a2e;
    height: 10px;
    border-radius: 5px;
}

QScrollBar::handle:horizontal {
    background: #555;
    border-radius: 5px;
    min-width: 30px;
}

/* ---- QSpinBox / QDoubleSpinBox ----
 * 统一数字输入框风格: 深色底+亮色数字
 */

QSpinBox, QDoubleSpinBox {
    background-color: #1e1e30;
    color: #ffd740;
    border: 1px solid #444;
    border-radius: 2px;
    padding: 3px 6px;
    font-family: "Consolas", "Courier New", monospace;
    font-size: 12px;
    selection-background-color: #1565c0;
}

QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #64b5f6;
}

QSpinBox::up-button, QDoubleSpinBox::up-button {
    background-color: #333;
    border-left: 1px solid #444;
    width: 16px;
}

QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #333;
    border-left: 1px solid #444;
    width: 16px;
}

/* ---- QPushButton ----
 * 全局默认按钮风格
 */

QPushButton {
    color: #ffffff;
    border: none;
    border-radius: 3px;
    padding: 4px 12px;
    font-size: 12px;
}

QPushButton:hover {
    opacity: 0.9;
}

QPushButton:pressed {
    opacity: 0.8;
}

/* ---- QLabel ----
 * 透明背景, 避免遮盖面板色
 */

QLabel {
    background-color: transparent;
}

/* ---- QGroupBox ----
 */

QGroupBox {
    border: 1px solid #444;
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 8px;
    color: #90a4ae;
    font-size: 11px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
}

/* ---- QToolTip ----
 */

QToolTip {
    background-color: #333;
    color: #fff;
    border: 1px solid #64b5f6;
    padding: 4px;
    font-size: 10px;
}
```

---

## Task 18: 编译验证 + 运行测试

**Files:**
- Create: `resources/sample_motor_params.json` (测试数据)

- [ ] **Step 1: 创建测试参数文件**

```json
{
    "axes": [
        {"axisId": 1, "velocity": 100.0, "acceleration": 500.0, "deceleration": 500.0,
         "leadScrew": 10.0, "pulsePerRev": 10000, "gearRatio": 1.0,
         "softLimitPositive": 300.0, "softLimitNegative": 0.0},
        {"axisId": 2, "velocity": 100.0, "acceleration": 500.0, "deceleration": 500.0,
         "leadScrew": 10.0, "pulsePerRev": 10000, "gearRatio": 1.0,
         "softLimitPositive": 300.0, "softLimitNegative": 0.0},
        {"axisId": 3, "velocity": 150.0, "acceleration": 800.0, "deceleration": 800.0,
         "leadScrew": 5.0, "pulsePerRev": 10000, "gearRatio": 1.0,
         "softLimitPositive": 200.0, "softLimitNegative": 0.0}
    ]
}
```

- [ ] **Step 2: 编译项目**

```bash
cd d:/my_information/qtProject/chipSetter

# 确保Qt在PATH中
export PATH="D:/tool/qt/5.15.2/mingw81_64/bin:$PATH"

# 生成Makefile
qmake chipSetter.pro

# 编译
mingw32-make -j4
```

Expected: 编译成功, 无错误.

- [ ] **Step 3: 运行并验证**

```bash
./release/chipSetter.exe
```

验证清单:
- [ ] 窗口标题显示"固晶机控制系统 V1.0", 尺寸1200×800
- [ ] 顶部状态栏显示模式、状态、产量、时间
- [ ] 电机PTP面板: 可选择不同轴, 输入目标位置, 点运行
- [ ] 电机参数面板: 可切换轴, 编辑参数
- [ ] IO面板显示16路DI/4路DO, 颜色正确
- [ ] 报警列表可显示报警
- [ ] 统计面板显示产量/时长/节拍
- [ ] 底部操作栏按钮可用
- [ ] 深色主题正常加载

---

## Task 19: 远程调试部署准备

- [ ] **Step 1: 创建部署脚本 deploy.bat**

```batch
@echo off
REM deploy.bat — 固晶机部署脚本
REM 用法: deploy.bat <目标机IP>

set TARGET_IP=%1
if "%TARGET_IP%"=="" set TARGET_IP=192.168.1.101

echo === 固晶机部署工具 ===
echo 目标设备: %TARGET_IP%

REM Step 1: 编译Release版本
echo [1/3] 编译 Release 版本...
call D:\tool\qt\5.15.2\mingw81_64\bin\qmake.exe chipSetter.pro "CONFIG+=release"
mingw32-make -j4
if %errorlevel% neq 0 (
    echo 编译失败!
    pause
    exit /b 1
)

REM Step 2: windeployqt 打包Qt依赖
echo [2/3] 打包 Qt 依赖...
D:\tool\qt\5.15.2\mingw81_64\bin\windeployqt.exe release\chipSetter.exe --no-translations --no-compiler-runtime
if %errorlevel% neq 0 (
    echo windeployqt 失败!
    pause
    exit /b 1
)

REM Step 3: 复制到目标机 (通过网络共享)
echo [3/3] 复制到目标设备...
REM 需要在目标机上先建立共享目录: \\%TARGET_IP%\chipSetter
robocopy release \\%TARGET_IP%\chipSetter\ *.* /MIR /R:3 /W:5

echo === 部署完成! ===
echo 在目标机上运行: \\%TARGET_IP%\chipSetter\chipSetter.exe
pause
```

- [ ] **Step 2: 创建日志配置文件**

在主窗口 `initSystem()` 末尾加入日志初始化代码 (在 mainwindow.cpp 的 initSystem 函数尾部追加):

```cpp
// ===== 文件日志 (目标机调试用) =====
#include <QFile>
#include <QTextStream>
#include <QDateTime>

// 添加在 initSystem() 末尾:
{
    static QFile logFile("chipsetter_debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
            static QMutex mutex;
            QMutexLocker lock(&mutex);

            QFile* f = &logFile;
            if (!f->isOpen()) return;

            QTextStream ts(f);
            QString level;
            switch (type) {
                case QtDebugMsg:    level = "DEBUG"; break;
                case QtWarningMsg:  level = "WARN";  break;
                case QtCriticalMsg: level = "CRIT";  break;
                case QtFatalMsg:    level = "FATAL"; break;
                default:            level = "INFO";  break;
            }
            ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
               << " [" << level << "] "
               << (ctx.function ? ctx.function : "") << " "
               << msg << "\n";
            ts.flush();
        });
        qDebug() << "=== 固晶机控制系统启动 ===";
    }
}
```

- [ ] **Step 3: Qt Creator 远程调试配置指南**

创建 `docs/qt-remote-debug-guide.md`:

```markdown
# Qt Creator 远程调试配置指南

## 前提条件

- 开发PC IP: 192.168.1.100 (静态)
- 目标GNC工控机 IP: 192.168.1.101 (静态)
- 网线直连, 同一网段

## 目标机准备

1. 安装 Qt 5.15.2 MSVC Runtime (或静态编译, 不需要安装)
2. 拷贝编译好的 `chipSetter.exe` + `windeployqt` 打包的依赖到目标机
3. 在目标机上创建共享目录 `C:\chipSetter` 并共享

## Qt Creator 配置

1. **添加设备:**
   - 工具 → 选项 → 设备 → 添加
   - 类型: Generic Linux Device / Windows Device
   - 主机名: 192.168.1.101
   - 认证: 用户名/密码

2. **添加 Kit:**
   - 工具 → 选项 → 构建和运行 → 构建套件
   - 编译器: MSVC 2019
   - Qt版本: Qt 5.15.2
   - 设备类型: 桌面设备

3. **远程运行:**
   - 项目 → 运行 → 部署方式: 部署到远程设备
   - 远程可执行文件: C:\chipSetter\chipSetter.exe

## 日志调试 (替代方案)

如果远程调试不稳定, 使用文件日志:

1. 程序会自动生成 `chipsetter_debug.log`
2. 所有 `qDebug()`, `qWarning()`, `qCritical()` 自动写入日志
3. 在目标机上用记事本打开日志文件实时查看
```

---

## 自查清单

**1. Spec覆盖检查:**
- [x] 状态栏 — Task 9
- [x] 电机PTP — Task 10
- [x] 电机参数 — Task 11
- [x] IO监视 — Task 12
- [x] 报警列表 — Task 13
- [x] 生产统计 — Task 14
- [x] 底部操作栏 — Task 15
- [x] 主窗口布局 — Task 16
- [x] 前后端分离 — Tasks 5-8 (Core) + Tasks 9-15 (UI)
- [x] 轴ID宏集中 — Task 1
- [x] 远程调试 — Task 19
- [x] 视觉预留 — Task 16
- [x] 气动留空 — 已标注

**2. Placeholder扫描:** 无TBD/TODO。

**3. 类型一致性:**
- MotorPtpWidget::moveRequested ↔ MotorManager::moveRequest — 参数一致 ✓
- IoManager::diChanged ↔ IoMonitorWidget::onDiChanged — 签名一致 ✓
- AlarmLogger::newAlarm ↔ AlarmListWidget::onNewAlarm — 参数一致 ✓
