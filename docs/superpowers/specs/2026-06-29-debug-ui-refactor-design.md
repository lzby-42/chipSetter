# Debug UI Refactor — MotorControlWidget

## Context

MotorParamWidget.cpp has duplicate grid rows (lines 160-165 are copy-paste of 154-159),
and both MotorPtpWidget and MotorParamWidget each have a 13-axis button bar wasting
vertical space in the 420px left column. Refactor into a single clean widget.

## Outcome

Delete `MotorPtpWidget` and `MotorParamWidget`, replace with new `MotorControlWidget`.

## Layout (420px left column)

```
┌── 13 axis buttons (shared, 2 rows) ──────────────┐
├─────────────────────┬──────────────────────────────┤
│ PTP motion control  │ Param grid                   │
│                     │                               │
│ Position +0.000 mm  │     导程        10.000  rev  │
│ [Target___________] │ [✓] 每转脉冲    10000   pls  │
│                     │     齿轮比       1.00        │
│ Speed:    [100.0 ]  │                              │
│ Accel:    [500.0 ]  │ [✓] 软限位+     9999    mm  │
│ Decel:    [500.0 ]  │ [✓] 软限位-    -9999    mm  │
│                     │     回零速度     50.0   s    │
│ [Run] [Stop]        │     回零偏移      2.0   mm   │
│ [Home]              │                              │
│ [JOG+] [JOG-]       │ [Apply] [Export] [Import]    │
│ [ClrAlarm] [Enable] │                              │
└─────────────────────┴──────────────────────────────┘
```

- Speed/accel/decel: only in PTP area, removed from param grid. Still persisted to JSON.
- 每转脉冲/齿轮比: no checkbox (every axis has these).
- 导程: checkbox `hasLeadScrew` (uncheck for rotary/crank axes).
- 软限位+: checkbox `hasSoftLimitPositive`.
- 软限位-: checkbox `hasSoftLimitNegative`.
- On axis switch: auto-apply pending changes (preserve current behavior).

## Data model changes

Add to `MotorAxis`:
- `bool hasLeadScrew` (default true)
- `bool hasSoftLimitPositive` (default true)
- `bool hasSoftLimitNegative` (default true)

Remove old `hasSoftLimit` (replaced by the two individual flags).

## Signals (backward compatible)

From old MotorPtpWidget:
- `moveRequested(int, double, double, double, double)`
- `stopRequested(int)`
- `homeRequested(int)`
- `jogRequested(int, bool, double, double, double, double)`
- `clearAlarmRequested(int)`
- `enableRequested(int, bool)`

From old MotorParamWidget:
- `applyRequested(int, const MotorAxis&)`
- `exportRequested(const QString&)`
- `importRequested(const QString&)`

Slots:
- `onPositionUpdated(int, double)`
- `onMoveFinished(int, bool)`
- `onHomeFinished(int, bool, int)`
- `onAxisStatusChanged(int, long)`
- `onAxisEnableChanged(int, bool)`
- `onParamsApplied(int)`
- `setCurrentAxisId(int)`

## Files

| Action | File |
|--------|------|
| Create | `widgets/MotorControlWidget.h` |
| Create | `widgets/MotorControlWidget.cpp` |
| Delete | `widgets/MotorPtpWidget.h` |
| Delete | `widgets/MotorPtpWidget.cpp` |
| Delete | `widgets/MotorParamWidget.h` |
| Delete | `widgets/MotorParamWidget.cpp` |
| Modify | `mainwindow.cpp` — replace references |
| Modify | `chipSetter.pro` — replace SOURCES/HEADERS |
| Modify | `models/MotorAxis.h` — update bool flags |
| Modify | `core/MotorManager.cpp` — update serialization |
