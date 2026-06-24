QT += core gui widgets
TARGET = chipSetter
TEMPLATE = app
CONFIG += c++11

# ---- GTS SDK 路径配置 ----
GNC_SDK_PATH = $$PWD/googol
INCLUDEPATH += $$GNC_SDK_PATH

# ---- 真实硬件模式 vs Mock模拟 (互斥) ----
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    core/GncController.cpp \
    core/GncControllerImpl.cpp \
    core/MotorManager.cpp \
    core/IoManager.cpp \
    core/AlarmLogger.cpp \
    core/StatsCollector.cpp \
    core/ProcessManager.cpp \
    widgets/StatusBarWidget.cpp \
    widgets/MotorPtpWidget.cpp \
    widgets/MotorParamWidget.cpp \
    widgets/IoMonitorWidget.cpp \
    widgets/AlarmListWidget.cpp \
    widgets/StatsWidget.cpp \
    widgets/BottomBarWidget.cpp \
    widgets/ProductionWidget.cpp \
    widgets/FlowStepBar.cpp \
    widgets/StepDetailPanel.cpp \
    widgets/DeviceStatusWidget.cpp

HEADERS += \
    mainwindow.h \
    core/HardwareConfig.h \
    models/MotorAxis.h \
    models/IoSignal.h \
    models/AlarmRecord.h \
    core/GncController.h \
    core/GncControllerImpl.h \
    core/MotorManager.h \
    core/IoManager.h \
    core/AlarmLogger.h \
    core/StatsCollector.h \
    core/ProcessManager.h \
    widgets/StatusBarWidget.h \
    widgets/MotorPtpWidget.h \
    widgets/MotorParamWidget.h \
    widgets/IoMonitorWidget.h \
    widgets/AlarmListWidget.h \
    widgets/StatsWidget.h \
    widgets/BottomBarWidget.h \
    widgets/ProductionWidget.h \
    widgets/FlowStepBar.h \
    widgets/StepDetailPanel.h \
    widgets/DeviceStatusWidget.h

RESOURCES += resources.qrc

# ---- 真实硬件 vs Mock 互斥 (必须在 SOURCES/HEADERS 之后) ----
real_gnc {
    DEFINES += USE_REAL_GNC
    LIBS += -L$$GNC_SDK_PATH -lgts
    message(">>> chipSetter: 编译真实硬件模式 (GTS SDK) <<<")
    SOURCES -= core/GncController.cpp
} else {
    message(">>> chipSetter: 编译Mock模拟模式 <<<")
    SOURCES -= core/GncControllerImpl.cpp
    HEADERS -= core/GncControllerImpl.h    # 防止moc生成未定义符号
}
