#include <QtTest>
#include "core/HardwareConfig.h"

class TestHardwareConfig : public QObject
{
    Q_OBJECT
private slots:
    void testAxisCount();
    void testDiCount();
    void testDoCount();
    void testAxisNamesCount();
    void testDiNamesCount();
    void testDoNamesCount();
    void testAxisIdsDistinct();
    void testAxisIdsInRange();
    void testDefaultValues();
};

void TestHardwareConfig::testAxisCount() { QCOMPARE(AXIS_COUNT, 13); }
void TestHardwareConfig::testDiCount() { QCOMPARE(DI_COUNT, 19); }
void TestHardwareConfig::testDoCount() { QCOMPARE(DO_COUNT, 4); }

void TestHardwareConfig::testAxisNamesCount() {
    QStringList names = QStringList AXIS_NAMES;
    QCOMPARE(names.size(), AXIS_COUNT);
}

void TestHardwareConfig::testDiNamesCount() {
    QStringList names = QStringList DI_NAMES;
    QCOMPARE(names.size(), DI_COUNT);
}

void TestHardwareConfig::testDoNamesCount() {
    QStringList names = QStringList DO_NAMES;
    QCOMPARE(names.size(), DO_COUNT);
}

void TestHardwareConfig::testAxisIdsDistinct() {
    // Verify all axis IDs are distinct
    QSet<int> ids;
    ids << AXIS_JIAO_PAN << AXIS_DISPENSE_X << AXIS_DISPENSE_Y
        << AXIS_PICKUP_ARM_ROT << AXIS_DISPENSE_ARM_ROT << AXIS_DISPENSE_ARM_UD
        << AXIS_PICKUP_ARM_UD << AXIS_EJECTOR_UP << AXIS_EJECTOR_DOWN
        << AXIS_PICKUP_X << AXIS_PICKUP_Y << AXIS_PICKUP_W << AXIS_GRIPPER;
    QCOMPARE(ids.size(), 13);
}

void TestHardwareConfig::testAxisIdsInRange() {
    int ids[] = {
        AXIS_JIAO_PAN, AXIS_DISPENSE_X, AXIS_DISPENSE_Y,
        AXIS_PICKUP_ARM_ROT, AXIS_DISPENSE_ARM_ROT, AXIS_DISPENSE_ARM_UD,
        AXIS_PICKUP_ARM_UD, AXIS_EJECTOR_UP, AXIS_EJECTOR_DOWN,
        AXIS_PICKUP_X, AXIS_PICKUP_Y, AXIS_PICKUP_W, AXIS_GRIPPER
    };
    for (int id : ids) {
        QVERIFY(id >= 1 && id <= AXIS_COUNT);
    }
}

void TestHardwareConfig::testDefaultValues() {
    QVERIFY(DEFAULT_VELOCITY > 0);
    QVERIFY(DEFAULT_ACCEL > 0);
    QVERIFY(DEFAULT_DECEL > 0);
    QVERIFY(DEFAULT_JOG_STEP > 0);
}

QTEST_MAIN(TestHardwareConfig)
#include "test_hardware_config.moc"
