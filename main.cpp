#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QMutex>
#include <QTextStream>
#include "mainwindow.h"

// ---- file logger ----
static QFile g_logFile;
static QMutex g_logMutex;

static void fileLogHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    QMutexLocker lock(&g_logMutex);
    if (!g_logFile.isOpen()) return;

    QTextStream ts(&g_logFile);
    ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " ";

    switch (type) {
    case QtDebugMsg:    ts << "[D]"; break;
    case QtWarningMsg:  ts << "[W]"; break;
    case QtCriticalMsg: ts << "[C]"; break;
    case QtFatalMsg:    ts << "[!]"; break;
    default:            ts << "[?]"; break;
    }

    ts << " " << msg;

    if (ctx.function) {
        ts << "  (" << ctx.function << ":" << ctx.line << ")";
    }
    ts << "\n";
    ts.flush();
}

int main(int argc, char *argv[])
{
    // ---- file logging (must be first, before QApplication) ----
    g_logFile.setFileName("chipsetter_debug.log");
    if (g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(fileLogHandler);
        qDebug() << "=== chipSetter V1.0 started ===";
    }

    QApplication app(argc, argv);
    app.setApplicationName("chipSetter");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ChipSetter");

    // ---- load QSS ----
    QFile styleFile(":/resources/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
        qDebug() << "QSS loaded OK";
    } else {
        qWarning() << "Failed to load stylesheet:" << styleFile.errorString();
    }

    MainWindow mainWindow;
    mainWindow.setWindowTitle("固晶机控制系统 V1.0");
    mainWindow.resize(1024, 765);
    mainWindow.setMinimumSize(1024, 680);
    mainWindow.show();

    int ret = app.exec();

    qDebug() << "=== chipSetter exited ===";
    return ret;
}
