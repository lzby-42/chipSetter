#ifndef MOTORAXIS_H
#define MOTORAXIS_H

#include <QString>
#include "core/HardwareConfig.h"

struct MotorAxis {
    int     axisId;                 // 轴ID (1~13, 对应GNC SDK索引)
    QString name;                   // 轴名称

    double  currentPosition;        // 当前位置 (mm)
    double  targetPosition;         // 目标位置 (mm)
    double  velocity;               // 运行速度 (mm/s)
    double  acceleration;           // 加速度 (mm/s²)
    double  deceleration;           // 减速度 (mm/s²)
    double  jogStep;                // JOG步距 (mm)

    double  leadScrew;              // 导程 (mm/rev)
    int     pulsePerRev;            // 每转脉冲数
    double  gearRatio;              // 电子齿轮比

    double  softLimitPositive;      // 正向软限位 (mm)
    double  softLimitNegative;      // 负向软限位 (mm)

    bool    isEnabled;              // 轴使能
    bool    isMoving;               // 运动中
    bool    isHomed;                // 已回零
    bool    isAlarm;                // 报警
    long    rawStatus;              // GNC原始轴状态字

    bool    homeSignal;             // Home传感器
    bool    limitPositiveSignal;    // 正限位
    bool    limitNegativeSignal;    // 负限位

    // Default constructor using HardwareConfig defaults
    MotorAxis()
        : axisId(0), currentPosition(0.0), targetPosition(0.0)
        , velocity(DEFAULT_VELOCITY), acceleration(DEFAULT_ACCEL)
        , deceleration(DEFAULT_DECEL), jogStep(DEFAULT_JOG_STEP)
        , leadScrew(DEFAULT_LEAD_SCREW), pulsePerRev(DEFAULT_PULSE_PER_REV)
        , gearRatio(DEFAULT_GEAR_RATIO)
        , softLimitPositive(300.0), softLimitNegative(0.0)
        , isEnabled(false), isMoving(false), isHomed(false), isAlarm(false)
        , rawStatus(0)
        , homeSignal(false), limitPositiveSignal(false), limitNegativeSignal(false)
    {}
};

Q_DECLARE_METATYPE(MotorAxis)

#endif // MOTORAXIS_H
