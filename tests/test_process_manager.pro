QT += core testlib
CONFIG += c++11 console
TARGET = test_process_manager
INCLUDEPATH += ..

SOURCES += test_process_manager.cpp \
           ../core/ProcessManager.cpp
