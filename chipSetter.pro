QT += core gui widgets
TARGET = chipSetter
TEMPLATE = app
CONFIG += c++11

INCLUDEPATH += "D:/study/googel/固高网络型运动控制器编程手册20260509/site"

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h \
    core/HardwareConfig.h

RESOURCES += resources.qrc
