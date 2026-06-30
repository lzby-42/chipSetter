#ifndef HARDWARECONFIG_H
#define HARDWARECONFIG_H

#define GNC_CORE_NUM        1
#define GNC_AXIS_START      1

#define AXIS_COUNT          16
#define MOTOR_PARAMS_FILE   "motor_params.json"   // 电机参数配置文件 (exe同目录)

// ---- 轴ID定义 (对应 core1_20260625.cfg [axisN]) ----
#define AXIS_JIAO_PAN           1   // 胶盘
#define AXIS_DISPENSE_X         2   // 点胶平台X轴
#define AXIS_DISPENSE_Y         3   // 点胶平台Y轴
#define AXIS_RESERVED_4         4   // (cfg axis4, 无配置信号)
#define AXIS_RESERVED_5         5   // (cfg axis5, 报警轴)
#define AXIS_RESERVED_6         6   // (cfg axis6, 报警轴)
#define AXIS_PICKUP_ARM_ROT     7   // 取晶臂转动
#define AXIS_DISPENSE_ARM_ROT   8   // 点胶臂转动
#define AXIS_DISPENSE_ARM_UD    9   // 点胶臂上下
#define AXIS_PICKUP_ARM_UD     10   // 取晶臂上下
#define AXIS_EJECTOR_UP        11   // 上顶针
#define AXIS_EJECTOR_DOWN      12   // 下顶针
#define AXIS_PICKUP_X          13   // 取晶平台X轴
#define AXIS_PICKUP_Y          14   // 取晶平台Y轴
#define AXIS_PICKUP_W          15   // 取晶平台W轴
#define AXIS_GRIPPER           16   // 抓手

#define AXIS_NAMES { \
    "胶盘", "点胶平台X轴", "点胶平台Y轴", \
    "预留4", "预留5", "预留6", \
    "取晶臂转动", "点胶臂转动", "点胶臂上下", "取晶臂上下", \
    "上顶针", "下顶针", "取晶平台X轴", "取晶平台Y轴", \
    "取晶平台W轴", "抓手" \
}

// ---- IO数量 (匹配实际机器) ----
#define DI_COUNT            19   // X0-X18, 对应 GTS GPI 1-19
#define DO_COUNT             4   // Y9-Y12, 对应 GTS GPO 9-12
#define DO_INDEX_BASE        9   // DO在GTS中的起始编号 (GPO9=Y9)

// ---- DI信号ID定义 (对应 GTS GPI 1-19) ----
#define DI_DISPENSE_X_LIMIT_P    1   // GPI1  X0 点胶平台X轴正限位
#define DI_DISPENSE_X_LIMIT_N    2   // GPI2  X1 点胶平台X轴负限位
#define DI_DISPENSE_X_HOME       3   // GPI3  X2 点胶平台X轴HOME
#define DI_DISPENSE_Y_LIMIT_P    4   // GPI4  X3 点胶平台Y轴正限位
#define DI_DISPENSE_Y_LIMIT_N    5   // GPI5  X4 点胶平台Y轴负限位
#define DI_DISPENSE_Y_HOME       6   // GPI6  X5 点胶平台Y轴HOME
#define DI_PICKUP_X_LIMIT_P      7   // GPI7  X6 取晶X轴正限位
#define DI_PICKUP_X_LIMIT_N      8   // GPI8  X7 取晶X轴负限位
#define DI_PICKUP_X_HOME         9   // GPI9  X8 取晶X轴HOME
#define DI_PICKUP_Y_LIMIT_N     10   // GPI10 X9 取晶Y轴负限位
#define DI_PICKUP_Y_LIMIT_P     11   // GPI11 X10 取晶Y轴正限位
#define DI_PICKUP_ARM_ROT       12   // GPI12 X11 取晶臂旋转
#define DI_DISPENSE_ARM_ROT     13   // GPI13 X12 点胶臂旋转
#define DI_PICKUP_PLATFORM_ROT  14   // GPI14 X13 取晶平台旋转
#define DI_GRIPPER_HOME         15   // GPI15 X14 吸嘴HOME
#define DI_EJECTOR_PIN1         16   // GPI16 X15 顶针1
#define DI_EJECTOR_PIN2         17   // GPI17 X16 顶针2
#define DI_DISPENSE_CYLINDER    18   // GPI18 X17 点胶上下气缸到位
#define DI_PICKUP_ARM_UD        19   // GPI19 X18 取晶臂上下

// 急停由硬件电路直接处理, 无对应DI输入
#define DI_EMERGENCY_STOP        0

#define DI_NAMES { \
    "X0_点胶平台X+", "X1_点胶平台X-", "X2_点胶平台X_Home", \
    "X3_点胶平台Y+", "X4_点胶平台Y-", "X5_点胶平台Y_Home", \
    "X6_取晶X+",     "X7_取晶X-",     "X8_取晶X_Home", \
    "X9_取晶Y-",     "X10_取晶Y+",    "X11_取晶臂旋转", \
    "X12_点胶臂旋转", "X13_取晶平台旋转", "X14_吸嘴Home", \
    "X15_顶针1",     "X16_顶针2",     "X17_点胶上下到位", "X18_取晶臂上下" \
}

// ---- DO信号ID定义 ----
#define DO_RED_LIGHT        1   // GPO9  Y9  红灯
#define DO_YELLOW_LIGHT     2   // GPO10 Y10 黄灯
#define DO_GREEN_LIGHT      3   // GPO11 Y11 绿灯
#define DO_BUZZER           4   // GPO12 Y12 蜂鸣器

#define DO_NAMES { "红灯", "黄灯", "绿灯", "蜂鸣器" }

// ---- 默认运动参数 ----
#define DEFAULT_VELOCITY        100.0
#define DEFAULT_ACCEL           500.0
#define DEFAULT_DECEL           500.0
#define DEFAULT_JOG_STEP        0.010
#define DEFAULT_JOG_STEP_FAST   1.000
#define DEFAULT_LEAD_SCREW      10.0
#define DEFAULT_PULSE_PER_REV   10000
#define DEFAULT_GEAR_RATIO      1.0
#define MAX_VELOCITY            500.0
#define MAX_ACCEL               2000.0

// ============================================================
// DI → 轴限位映射表
// 基于 core1_20260625.cfg 中的 limitPositiveIndex/limitNegativeIndex
// signalType: 0=Home, 1=正限位, 2=负限位, -1=普通状态(不限位)
// ============================================================
struct DiToAxisMapping {
    int diId;       // DI通道编号 (1-19, 对应GPI)
    int axisId;     // 对应逻辑轴ID (1-16), 0=无映射
    int signalType; // 0=Home, 1=正限位, 2=负限位, -1=无
};


#define DI_AXIS_MAP { \
    { 1,  2, 1 },  /* GPI1  X0  点胶平台X+    → 轴2  正限位 */ \
    { 2,  2, 2 },  /* GPI2  X1  点胶平台X-    → 轴2  负限位 */ \
    { 3,  2, 0 },  /* GPI3  X2  点胶平台X_Home → 轴2  Home  */ \
    { 4,  3, 1 },  /* GPI4  X3  点胶平台Y+    → 轴3  正限位 */ \
    { 5,  3, 2 },  /* GPI5  X4  点胶平台Y-    → 轴3  负限位 */ \
    { 6,  3, 0 },  /* GPI6  X5  点胶平台Y_Home → 轴3  Home  */ \
    { 7, 13, 1 },  /* GPI7  X6  取晶X+        → 轴13 正限位 */ \
    { 8, 13, 2 },  /* GPI8  X7  取晶X-        → 轴13 负限位 */ \
    { 9, 13, 0 },  /* GPI9  X8  取晶X_Home    → 轴13 Home  */ \
    {10, 14, 2 },  /* GPI10 X9  取晶Y-        → 轴14 负限位 */ \
    {11, 14, 1 },  /* GPI11 X10 取晶Y+        → 轴14 正限位 */ \
    {12,  7, 1 },  /* GPI12 X11 取晶臂转动     → 轴7  正限位 */ \
    {13,  8, 0 },  /* GPI13 X12 点胶臂转动     → 轴8  Home  */ \
    {14, 15, 0 },  /* GPI14 X13 取晶平台W轴    → 轴15 Home  */ \
    {15, 16, 0 },  /* GPI15 X14 抓手Home       → 轴16 Home  */ \
    {16, 11, 1 },  /* GPI16 X15 上顶针         → 轴11 正限位 */ \
    {17, 12, 1 },  /* GPI17 X16 下顶针         → 轴12 正限位 */ \
    {18,  9, 1 },  /* GPI18 X17 点胶臂上下     → 轴9  正限位 */ \
    {19, 10, 1 },  /* GPI19 X18 取晶臂上下     → 轴10 正限位 */ \
}

#endif // HARDWARECONFIG_H
