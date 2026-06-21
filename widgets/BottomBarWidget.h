/**
 * @file BottomBarWidget.h
 * @brief 底部操作栏 — 急停/启动/暂停/复位、模式切换、通讯状态
 */

#ifndef BOTTOMBARWIDGET_H
#define BOTTOMBARWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class BottomBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BottomBarWidget(QWidget *parent = nullptr);
    ~BottomBarWidget();

public slots:
    void setConnectionStatus(bool connected);   // 通讯状态
    void setMode(int mode);                     // 手动/自动

signals:
    void emergencyStopClicked();
    void startClicked();
    void pauseClicked();
    void resetClicked();
    void modeSwitchClicked();
    void shutdownClicked();                     // 关机

private:
    void setupUI();

    QPushButton* m_emergencyBtn;
    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_resetBtn;
    QPushButton* m_shutdownBtn;
    QPushButton* m_modeBtn;
    QLabel*      m_connLabel;
};

#endif // BOTTOMBARWIDGET_H
