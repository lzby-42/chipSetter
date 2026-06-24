/**
 * @file test_io_console.cpp
 * @brief 控制台验证 — 测试IoManager+MockGncController的IO信号流
 *
 * 无需GUI, 直接输出到stdout
 * 编译: g++ -std=c++11 -I.. -I../../tool/qt/5.15.2/mingw81_32/include
 *        test_io_console.cpp ../core/GncController.cpp ../core/IoManager.cpp
 *        -L../../tool/qt/5.15.2/mingw81_32/lib -lQt5Core -o test_io_console
 */
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include "../core/GncController.h"
#include "../core/IoManager.h"
#include "../core/HardwareConfig.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== IO System Verification Test ===" << std::endl;
    std::cout << "DI_COUNT = " << DI_COUNT << std::endl;
    std::cout << "DO_COUNT = " << DO_COUNT << std::endl;
    std::cout << "DO_INDEX_BASE = " << DO_INDEX_BASE << std::endl;

    // 1. Create Mock Controller
    MockGncController controller;
    std::cout << "\n[1] Creating MockGncController..." << std::endl;
    std::cout << "    isConnected() = " << controller.isConnected() << " (expect 0)" << std::endl;

    // 2. Open Card
    controller.openCard();
    std::cout << "[2] openCard() called" << std::endl;
    std::cout << "    isConnected() = " << controller.isConnected() << " (expect 1)" << std::endl;

    // 3. Net Init
    controller.netInit("config.xml");
    controller.loadConfig(GNC_CORE_NUM, "config.cfg");
    std::cout << "[3] netInit + loadConfig done" << std::endl;

    // 4. Test readDI
    std::cout << "\n[4] Testing readDI..." << std::endl;
    short diValues[DI_COUNT];
    bool ok = controller.readDI(GNC_CORE_NUM, MC_GPI, 1, diValues, DI_COUNT);
    std::cout << "    readDI ok = " << (ok ? "true" : "false") << std::endl;

    int activeCount = 0;
    QStringList diNames = DI_NAMES;
    for (int i = 0; i < DI_COUNT; ++i) {
        QString name = (i < diNames.size()) ? diNames[i] : "?";
        std::cout << "    DI[" << (i+1) << "] " << name.toStdString()
                  << " = " << diValues[i] << (diValues[i] ? " *** ACTIVE ***" : "") << std::endl;
        if (diValues[i]) activeCount++;
    }
    std::cout << "    Active DI count: " << activeCount << " (expect 4: Home sensors)" << std::endl;

    // 5. Create IoManager
    std::cout << "\n[5] Creating IoManager..." << std::endl;
    IoManager ioMgr(&controller);
    ioMgr.initialize();
    std::cout << "    IoManager initialized, polling started" << std::endl;

    // 6. Test DO set/get
    std::cout << "\n[6] Testing DO..." << std::endl;
    for (int doIdx = 1; doIdx <= DO_COUNT; ++doIdx) {
        short physIdx = doIdx + DO_INDEX_BASE - 1;
        std::cout << "    Setting DO[" << doIdx << "] (phys GPO" << physIdx << ") = 1" << std::endl;
        bool ok = ioMgr.setDO(doIdx, 1);
        std::cout << "    setDO(" << doIdx << ", 1) = " << (ok ? "OK" : "FAIL") << std::endl;
    }

    // 7. Test writeDO verification
    std::cout << "\n[7] Verifying DO values in controller..." << std::endl;
    for (int doIdx = 1; doIdx <= DO_COUNT; ++doIdx) {
        const IoSignal& sig = ioMgr.doState(doIdx);
        std::cout << "    DO[" << doIdx << "] " << sig.name.toStdString()
                  << " = " << sig.value << std::endl;
    }

    // 8. Test DI-to-Axis mapping
    std::cout << "\n[8] Verifying DI_AXIS_MAP..." << std::endl;
    const DiToAxisMapping map[] = DI_AXIS_MAP;
    const int mapSize = sizeof(map) / sizeof(map[0]);
    std::cout << "    Map entries: " << mapSize << " (expect 19)" << std::endl;
    for (int i = 0; i < mapSize; ++i) {
        const char* typeNames[] = {"Home", "Limit+", "Limit-", "?"};
        int typeIdx = (map[i].signalType >= 0 && map[i].signalType <= 2) ? map[i].signalType : 3;
        std::cout << "    DI[" << map[i].diId << "] -> Axis[" << map[i].axisId
                  << "] type=" << typeNames[typeIdx] << std::endl;
    }

    // 9. Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "  VERIFICATION COMPLETE" << std::endl;
    std::cout << "  DI_COUNT: " << DI_COUNT << std::endl;
    std::cout << "  DO_COUNT: " << DO_COUNT << std::endl;
    std::cout << "  Active Home sensors: " << activeCount << std::endl;
    std::cout << "  DI_AXIS_MAP size: " << mapSize << std::endl;
    std::cout << "========================================" << std::endl;

    // Exit after 1 second
    QTimer::singleShot(1000, &app, &QCoreApplication::quit);
    return app.exec();
}
