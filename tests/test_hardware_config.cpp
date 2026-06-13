#include <QtTest>
#include "core/HardwareConfig.h"

class TestHardwareConfig : public QObject
{
    Q_OBJECT
private slots:
    void testAxisCount();
    void testDiCount();
    void testDoCount();
    void testAxisIdsDistinct();
    void testDefaultValues();
};

void TestHardwareConfig::testAxisCount() { QCOMPARE(AXIS_COUNT, 13); }
void TestHardwareConfig::testDiCount() { QCOMPARE(DI_COUNT, 16); }
void TestHardwareConfig::testDoCount() { QCOMPARE(DO_COUNT, 4); }
void TestHardwareConfig::testAxisIdsDistinct() {
    // Verify all axis IDs are distinct
    QSet<int> ids;
    ids << AXIS_WAFER_X << AXIS_WAFER_Y << AXIS_DISPENSE_X << AXIS_DISPENSE_Y
        << AXIS_EJECTOR_Z1 << AXIS_EJECTOR_Z2 << AXIS_ROTARY_WHEEL
        << AXIS_RESERVED_8 << AXIS_RESERVED_9 << AXIS_RESERVED_10
        << AXIS_RESERVED_11 << AXIS_RESERVED_12 << AXIS_RESERVED_13;
    QCOMPARE(ids.size(), 13);
}
void TestHardwareConfig::testDefaultValues() {
    QVERIFY(DEFAULT_VELOCITY > 0);
    QVERIFY(DEFAULT_ACCEL > 0);
    QVERIFY(DEFAULT_DECEL > 0);
    QVERIFY(DEFAULT_JOG_STEP > 0);
}

QTEST_MAIN(TestHardwareConfig)
#include "test_hardware_config.moc"
