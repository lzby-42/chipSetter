#include <QtTest>
#include "models/MotorAxis.h"
#include <QVector>

class TestMotorAxis : public QObject
{
    Q_OBJECT
private slots:
    void testDefaultConstruction();
    void testCustomAxisId();
    void testMetatypeRegistration();
    void testAllDefaultsAreReasonable();
};

void TestMotorAxis::testDefaultConstruction()
{
    MotorAxis ax;
    QCOMPARE(ax.axisId, 0);
    QCOMPARE(ax.currentPosition, 0.0);
    QCOMPARE(ax.velocity, DEFAULT_VELOCITY);
    QCOMPARE(ax.acceleration, DEFAULT_ACCEL);
    QCOMPARE(ax.deceleration, DEFAULT_DECEL);
    QCOMPARE(ax.jogStep, DEFAULT_JOG_STEP);
    QCOMPARE(ax.leadScrew, DEFAULT_LEAD_SCREW);
    QCOMPARE(ax.pulsePerRev, DEFAULT_PULSE_PER_REV);
    QCOMPARE(ax.gearRatio, DEFAULT_GEAR_RATIO);
    QCOMPARE(ax.softLimitPositive, 300.0);
    QCOMPARE(ax.softLimitNegative, 0.0);
    QVERIFY(!ax.isEnabled);
    QVERIFY(!ax.isMoving);
    QVERIFY(!ax.isHomed);
    QVERIFY(!ax.isAlarm);
}

void TestMotorAxis::testCustomAxisId()
{
    MotorAxis ax;
    ax.axisId = 3;
    ax.name = "点胶X";
    ax.currentPosition = 150.5;
    QCOMPARE(ax.axisId, 3);
    QCOMPARE(ax.name, QString("点胶X"));
    QCOMPARE(ax.currentPosition, 150.5);
}

void TestMotorAxis::testMetatypeRegistration()
{
    QVariant v = QVariant::fromValue(MotorAxis());
    QVERIFY(v.isValid());
    QVERIFY(v.canConvert<MotorAxis>());
}

void TestMotorAxis::testAllDefaultsAreReasonable()
{
    MotorAxis ax;
    QVERIFY(ax.velocity > 0);
    QVERIFY(ax.velocity <= MAX_VELOCITY);
    QVERIFY(ax.acceleration > 0);
    QVERIFY(ax.acceleration <= MAX_ACCEL);
    QVERIFY(ax.softLimitPositive > ax.softLimitNegative);
}

QTEST_MAIN(TestMotorAxis)
#include "test_motor_axis.moc"
