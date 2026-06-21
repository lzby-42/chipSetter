/**
 * @file BottomBarWidget.h
 * @brief 底部操作栏 — 启动/暂停/急停/收尾/立即收尾/复位、模式切换、通讯状态
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
    void setConnectionStatus(bool connected);
    void setMode(int mode);

signals:
    void startClicked();
    void pauseClicked();
    void emergencyStopClicked();
    void softFinishClicked();          // 收尾: 循环跑完后进收尾
    void immediateFinishClicked();     // 立即收尾: 立刻跳收尾
    void resetClicked();              // 复位: 急停+清除报警+重置统计
    void modeSwitchClicked();

private:
    void setupUI();

    QPushButton* m_startBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_emergencyBtn;
    QPushButton* m_softFinishBtn;
    QPushButton* m_immediateFinishBtn;
    QPushButton* m_resetBtn;
    QPushButton* m_modeBtn;
    QLabel*      m_connLabel;
};

#endif // BOTTOMBARWIDGET_H
