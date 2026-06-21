QT += core testlib
CONFIG += c++11 console
TARGET = test_process_manager
INCLUDEPATH += ..

HEADERS += ../core/ProcessManager.h

SOURCES += test_process_manager.cpp \
           ../core/ProcessManager.cpp
