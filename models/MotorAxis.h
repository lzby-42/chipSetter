#ifndef MOTORAXIS_H
#define MOTORAXIS_H

#include <QString>
#include "core/HardwareConfig.h"

struct MotorAxis {
    int     axisId;                 // 轴ID (1~16, 对应GNC SDK索引)
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
    double  homeVelocity;           // 回零速度 (mm/s), 内部mmToPulse转换
    double  homeOffset;             // 回零偏移 (mm), 内部mmToPulse转换
    int     homeDir;                // 回零搜索方向 (1=正向 -1=负向)
    int     homeEdge;               // 触发边沿 (0=下降沿 1=上升沿)
    int     homeMode;               // GTN_GoHome模式 (10=LIMIT 20=HOME 35=当前位置)
    int     triggerIndex;           // Home触发GPI编号 (mode=20时使用, -1=本轴默认)
    int     homeEscapeStep;         // 限位回零退离步数 (0=自动计算, >0=手动指定 pulse)
    bool    hasLeadScrew;           // 是否有导程 (旋转轴/曲柄=false)
    bool    hasSoftLimitPositive;   // 是否启用正向软限位
    bool    hasSoftLimitNegative;   // 是否启用负向软限位
    bool    isActive;               // 轴是否实际存在 (false=预留/不存在的轴)

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
        , softLimitPositive(9999.0), softLimitNegative(-9999.0)
        , homeVelocity(10.0), homeOffset(0.0), homeDir(1), homeEdge(0)
        , homeMode(10), triggerIndex(-1), homeEscapeStep(0)
        , hasLeadScrew(true), hasSoftLimitPositive(true), hasSoftLimitNegative(true), isActive(true)
        , isEnabled(false), isMoving(false), isHomed(false), isAlarm(false)
        , rawStatus(0)
        , homeSignal(false), limitPositiveSignal(false), limitNegativeSignal(false)
    {}
};

Q_DECLARE_METATYPE(MotorAxis)

#endif // MOTORAXIS_H
