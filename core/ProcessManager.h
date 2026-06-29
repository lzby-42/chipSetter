/**
 * @file ProcessManager.h
 * @brief 工艺流程管理器 — 9步状态机、自动循环、步骤切换
 *
 * 职责:
 *   - 管理9个工艺步骤（初始化→生产循环②~⑧→收尾）
 *   - 控制步骤切换和循环逻辑
 *   - 维护每个步骤的子步骤状态、参数、实时数据
 *   - 通过信号通知UI层状态变化
 *
 * 依赖: QObject, QTimer
 */

#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>

class DispensingPlatformController;
class PickupPlatformController;

class ProcessManager : public QObject
{
    Q_OBJECT

public:
    // 步骤状态枚举
    enum StepState {
        PENDING   = 0,    // 待执行 (灰色)
        RUNNING   = 1,    // 运行中 (蓝色发光)
        COMPLETED = 2,    // 已完成 (绿色)
        FAILED    = 3     // 失败 (红色)
    };
    Q_ENUM(StepState)

    // 步骤定义结构
    struct StepDef {
        int         index;            // 步骤索引 0~8
        QString     name;             // 步骤名称
        QStringList substeps;         // 子步骤名称列表
        QVariantMap defaultParams;    // 默认参数
    };

    explicit ProcessManager(QObject *parent = nullptr);
    ~ProcessManager();

    void setDispensingPlatform(DispensingPlatformController* platform);
    void setPickupPlatform(PickupPlatformController* platform);

    // ---- 状态查询 ----
    StepState stepState(int stepIndex) const;
    int currentStep() const;
    const StepDef& stepDef(int stepIndex) const;
    void setStepParam(int stepIndex, const QString& name, double value);
    QVector<StepDef> allSteps() const;
    bool isRunning() const;
    bool isPaused() const;
    int  cycleCount() const;

    // ---- 步骤详情 (供 StepDetailPanel) ----
    QVector<int>  substepStates(int stepIndex) const;
    QVariantMap   stepParams(int stepIndex) const;
    QVariantMap   realtimeData(int stepIndex) const;

public slots:
    void startCycle();                // 开始: 从初始化(0)开始
    void pauseCycle();                // 暂停: 完成当前步骤后停
    void resumeCycle();               // 继续: 继续自动循环
    void finishCycle();               // 立刻收尾: 跳至步骤8, 完成后停止
    void requestFinish();             // 出循环关机: 循环跑完后进收尾
    void emergencyStop();             // 急停: 全部重置为待执行

    bool isPendingFinish() const;     // 是否已标记出循环关机

signals:
    void stepStateChanged(int stepIndex, int state);
    void currentStepChanged(int stepIndex);
    void substepStateChanged(int stepIndex, int substepIndex, int state);
    void cycleCompleted(int totalCount);
    void allFinished();
    void realtimeDataUpdated(int stepIndex, const QVariantMap& data);

private slots:
    void onStepTimerTick();

private:
    void initStepDefs();              // 初始化9步定义
    void executeStep(int stepIndex);
    void completeCurrentStep();
    void advanceToNextStep();

    // 9步定义
    QVector<StepDef> m_steps;
    // 9步状态
    QVector<StepState> m_stepStates;
    // 9×n 子步骤状态
    QVector<QVector<StepState>> m_substepStates;

    int     m_currentStepIdx;         // -1=无, 0~8
    bool    m_running;
    bool    m_paused;
    bool    m_pendingFinish;          // 出循环关机标志
    int     m_cycleCount;
    int     m_substepProgress;        // 当前子步骤进度 (模拟用)
    QTimer* m_stepTimer;

    DispensingPlatformController* m_dispensingPlatform = nullptr;
    PickupPlatformController*     m_pickupPlatform     = nullptr;
};

#endif // PROCESSMANAGER_H
