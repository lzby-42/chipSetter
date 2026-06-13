#include <QApplication>
#include <QFile>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("chipSetter");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ChipSetter");

    QFile styleFile(":/resources/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    } else {
        qWarning() << "Failed to load stylesheet: " << styleFile.errorString();
    }

    MainWindow mainWindow;
    mainWindow.setWindowTitle("固晶机控制系统 V1.0");
    mainWindow.resize(1200, 800);
    mainWindow.setMinimumSize(1024, 680);
    mainWindow.show();

    return app.exec();
}
