/**
 * @file IoManager.h
 * @brief IO管理器 — DI/DO状态轮询、变化检测
 *
 * 职责:
 *   - 定时轮询19路DI + 4路DO状态
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
