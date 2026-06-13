QT += core gui widgets
TARGET = chipSetter
TEMPLATE = app
CONFIG += c++11

GNC_SDK_PATH = $$(GNC_SDK)
isEmpty(GNC_SDK_PATH): GNC_SDK_PATH = $$PWD/../sdk
INCLUDEPATH += $$GNC_SDK_PATH/site

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h \
    core/HardwareConfig.h \
    models/MotorAxis.h

RESOURCES += resources.qrc
