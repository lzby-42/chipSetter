QT += core gui widgets
TARGET = chipSetter
TEMPLATE = app
CONFIG += c++11

GNC_SDK_PATH = $$(GNC_SDK)
isEmpty(GNC_SDK_PATH): GNC_SDK_PATH = $$PWD/../sdk
INCLUDEPATH += $$GNC_SDK_PATH/site

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    core/GncController.cpp \
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
