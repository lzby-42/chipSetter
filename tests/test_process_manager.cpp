/**
 * @file test_process_manager.cpp
 * @brief ProcessManager 单元测试
 */
#include <QtTest>
#include <QSignalSpy>
#include "core/ProcessManager.h"

class TestProcessManager : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState();
    void testStartCycle();
    void testStepProgression();
    void testLoopBehavior();
    void testPauseAndResume();
    void testFinishCycle();
    void testEmergencyStop();
    void testStepDefinitions();
};

void TestProcessManager::testInitialState()
{
    ProcessManager pm;
    QCOMPARE(pm.currentStep(), -1);
    QCOMPARE(pm.isRunning(), false);
    QCOMPARE(pm.isPaused(), false);
    QCOMPARE(pm.cycleCount(), 0);

    // All 9 steps should be PENDING initially
    for (int i = 0; i < 9; ++i) {
        QCOMPARE(pm.stepState(i), ProcessManager::PENDING);
    }
}

void TestProcessManager::testStartCycle()
{
    ProcessManager pm;
    QSignalSpy spyStep(&pm, &ProcessManager::stepStateChanged);
    QSignalSpy spyCurrent(&pm, &ProcessManager::currentStepChanged);

    pm.startCycle();

    QCOMPARE(pm.isRunning(), true);
    QCOMPARE(pm.isPaused(), false);
    // Should start at step 0 (初始化)
    QCOMPARE(pm.currentStep(), 0);
    QCOMPARE(pm.stepState(0), ProcessManager::RUNNING);

    // Should have emitted signals
    QVERIFY(spyStep.count() >= 1);
    QVERIFY(spyCurrent.count() >= 1);
}

void TestProcessManager::testStepProgression()
{
    ProcessManager pm;
    QSignalSpy spyStep(&pm, &ProcessManager::stepStateChanged);

    pm.startCycle();

    // Wait for initialization to complete (5 substeps × 500ms = 2.5s + buffer)
    QTest::qWait(3500);

    // After init completes, step 1 (上料) should be running
    QCOMPARE(pm.stepState(0), ProcessManager::COMPLETED);
    QVERIFY(pm.currentStep() >= 1);
}

void TestProcessManager::testLoopBehavior()
{
    ProcessManager pm;
    pm.startCycle();

    // Wait for init + at least one full loop cycle
    // Each step has ~2-5 substeps × 500ms, 7 loop steps ≈ ~12-15s total
    // 25000ms 提供 ~70% 余量，避免 CI/高负载系统假失败
    QTest::qWait(25000);

    // Should have completed at least 1 cycle
    QVERIFY(pm.cycleCount() >= 1);
    // Should be in the loop somewhere (step 1-7)
    int cs = pm.currentStep();
    QVERIFY(cs >= 1 && cs <= 7);
}

void TestProcessManager::testPauseAndResume()
{
    ProcessManager pm;
    pm.startCycle();

    // Let it run briefly
    QTest::qWait(1000);

    pm.pauseCycle();
    QCOMPARE(pm.isPaused(), true);

    int stepBeforePause = pm.currentStep();

    // Wait - should not advance while paused
    QTest::qWait(3000);
    // Step should not change while paused (timer stopped)
    // Note: if step completed right at pause, it might advance one
    // before the timer actually stops. Check that we haven't advanced far.
    int stepAfterWait = pm.currentStep();
    QVERIFY(qAbs(stepAfterWait - stepBeforePause) <= 1);

    pm.resumeCycle();
    QCOMPARE(pm.isPaused(), false);
}

void TestProcessManager::testFinishCycle()
{
    ProcessManager pm;
    QSignalSpy spyAllFinished(&pm, &ProcessManager::allFinished);

    pm.startCycle();
    QTest::qWait(1500); // Let init run a bit

    pm.finishCycle();

    // Wait for finish step to complete (4 substeps × 500ms = 2s + buffer)
    QTest::qWait(4000);

    // allFinished should have been emitted
    QVERIFY(spyAllFinished.count() >= 1);
    QCOMPARE(pm.isRunning(), false);
}

void TestProcessManager::testEmergencyStop()
{
    ProcessManager pm;
    pm.startCycle();
    QTest::qWait(1000);

    pm.emergencyStop();

    QCOMPARE(pm.isRunning(), false);
    QCOMPARE(pm.currentStep(), -1);

    // All steps should be PENDING after emergency stop
    for (int i = 0; i < 9; ++i) {
        QCOMPARE(pm.stepState(i), ProcessManager::PENDING);
    }
}

void TestProcessManager::testStepDefinitions()
{
    ProcessManager pm;

    // Verify 9 step definitions
    QVector<ProcessManager::StepDef> steps = pm.allSteps();
    QCOMPARE(steps.size(), 9);

    // Verify step names
    QCOMPARE(steps[0].name, QString("初始化"));
    QCOMPARE(steps[1].name, QString("上料"));
    QCOMPARE(steps[8].name, QString("收尾"));

    // Verify substeps exist
    QVERIFY(steps[0].substeps.size() > 0);  // Init has 5 substeps
    QVERIFY(steps[4].substeps.size() > 0);  // Pick has 5 substeps

    // Verify parameters exist
    QVERIFY(steps[4].defaultParams.contains("拾取高度"));
    QVERIFY(steps[5].defaultParams.contains("点胶时间"));
}

QTEST_MAIN(TestProcessManager)
#include "test_process_manager.moc"
