/**
 * @file IoMonitorWidget.h
 * @brief IO信号监视面板 — DI/DO实时状态显示
 *
 * 职责:
 *   - 显示19路DI状态 (绿=高/红=低/灰=未知)
 *   - 显示4路DO状态 (蓝=高/棕=低)
 *   - 实时响应 Core/IoManager 的信号变化
 */

#ifndef IOMONITORWIDGET_H
#define IOMONITORWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVector>
#include <QEvent>
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

signals:
    void doToggleRequested(int doId, int value);  // DO开关请求 → IoManager::setDO

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;  // 拦截DO标签点击

private:
    void setupUI();

    QVector<QLabel*> m_diLabels;     // 19个DI状态标签
    QVector<QLabel*> m_doLabels;     // 4个DO状态标签 (可点击切换)
    short            m_doValues[5];  // DO当前值 (1-based)

    void updateLabelStyle(QLabel* label, IoType type, int value);
};

#endif // IOMONITORWIDGET_H
