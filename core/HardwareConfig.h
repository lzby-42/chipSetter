#ifndef HARDWARECONFIG_H
#define HARDWARECONFIG_H

#define GNC_CORE_NUM        1
#define GNC_AXIS_START      1

#define AXIS_COUNT          13

#define AXIS_WAFER_X        1
#define AXIS_WAFER_Y        2
#define AXIS_DISPENSE_X     3
#define AXIS_DISPENSE_Y     4
#define AXIS_EJECTOR_Z1     5
#define AXIS_EJECTOR_Z2     6
#define AXIS_ROTARY_WHEEL   7
#define AXIS_RESERVED_8     8
#define AXIS_RESERVED_9     9
#define AXIS_RESERVED_10    10
#define AXIS_RESERVED_11    11
#define AXIS_RESERVED_12    12
#define AXIS_RESERVED_13    13

#define AXIS_NAMES { \
    "晶盘X", "晶盘Y", "点胶X", "点胶Y", \
    "Z顶针1", "Z顶针2", "转轮", \
    "轴8", "轴9", "轴10", "轴11", "轴12", "轴13" \
}

#define DI_COUNT            16
#define DO_COUNT            4

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

#define DO_RED_LIGHT        1
#define DO_YELLOW_LIGHT     2
#define DO_GREEN_LIGHT      3
#define DO_BUZZER           4

#define DO_NAMES { "红灯", "黄灯", "绿灯", "蜂鸣器" }

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

#endif // HARDWARECONFIG_H
