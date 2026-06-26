/**
 * @file MotorManager.h
 * @brief 电机管理器 — 13轴状态维护、运动协调、参数管理
 *
 * 职责:
 *   - 管理13个轴的 MotorAxis 状态
 *   - 接收UI的运动请求 (moveRequest, homeRequest, jogRequest)
 *   - 通过 GncController 发送运动指令
 *   - 定时轮询位置和状态, 通过信号通知UI
 *
 * 依赖: GncController (注入), HardwareConfig.h, MotorAxis.h
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
    explicit MotorManager(GncController* controller, QObject *parent = nullptr);
    ~MotorManager();

    // ---- 初始化 ----
    bool initialize();                          // 初始化所有轴状态
    bool enableAxis(int axisId);                // 轴使能
    bool disableAxis(int axisId);               // 轴去使能

    // ---- 状态获取 ----
    const MotorAxis& axisState(int axisId) const;
    QVector<MotorAxis> allAxisStates() const;

    // ---- 诊断 ----
    QString diagnoseAxis(int axisId) const;  // 返回轴详细参数报告

    // ---- 信号更新 (来自IO) ----
    void updateAxisSignals(int axisId, bool home, bool limitPos, bool limitNeg);

    // ---- 参数管理 ----
    bool updateAxisParams(int axisId, const MotorAxis& params);  // 更新轴参数
    bool autoLoad();                           // 启动时自动加载 exe目录/motor_params.json
    bool autoSave();                           // 应用时自动保存 (原子写入)
    bool exportToFile(const QString& filePath);  // 手动导出到指定路径
    bool importFromFile(const QString& filePath);// 手动从指定路径导入

    // ---- 轮询控制 ----
    void startPolling(int intervalMs = 50);     // 开始轮询
    void stopPolling();                         // 停止轮询
    void loadLimitsFromController();            // 从GNC控制器读软限位 (cfg设定值)

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
    bool atomicWrite(const QString& filePath, const QByteArray& data);  // 原子写入
    QString configFilePath() const;                  // exe目录/motor_params.json
    QJsonObject axisToJson(const MotorAxis& ax) const;
    void jsonToAxis(const QJsonObject& obj, MotorAxis& ax);

    GncController*      m_controller;          // GNC接口 (注入, 不拥有)
    QVector<MotorAxis>   m_axes;                // 13轴状态 (0-based)
    QTimer*              m_pollTimer;           // 轮询定时器
    bool                 m_polling;
};

#endif // MOTORMANAGER_H
