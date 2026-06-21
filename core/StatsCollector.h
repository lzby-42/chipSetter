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
