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

    QVector<QLabel*> m_diLabels;    // 19个DI状态标签
    QVector<QLabel*> m_doLabels;    // 4个DO状态标签

    void updateLabelStyle(QLabel* label, IoType type, int value);
};

#endif // IOMONITORWIDGET_H
