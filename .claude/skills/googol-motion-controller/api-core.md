# Core Motion Control API Reference

## Motion Modes

### Trap Mode (梯形/S-curve) — Default Point-to-Point

Most common PT mode. Set parameters, set target position/velocity, then start all axes with a single `Update`.

```c
// Mode selection
GTN_PrfTrap(core, axis);

// Parameters
TTrapPrm prm = {acc, dec, velStart, smoothTime};
GTN_SetTrapPrm(core, axis, &prm);

// Target
GTN_SetVel(core, axis, maxVel);    // pulse/ms
GTN_SetPos(core, axis, targetPos); // pulse

// Start all axes in mask
GTN_Update(core, mask);  // mask = bitmask, e.g. (1<<0 | 1<<1) for axes 1,2
```

**Struct `TTrapPrm`:**
```c
typedef struct {
    double acc;          // Acceleration (pulse/ms²)
    double dec;          // Deceleration (pulse/ms²)
    double velStart;     // Start velocity (pulse/ms)
    double smoothTime;   // S-curve smoothing time (ms), 0 = pure trapezoidal
} TTrapPrm;
```

### MoveAbsolute — One-Call Absolute Move

No `Prf*` / `Update` needed. The function handles everything internally.

```c
TMoveAbsolutePrmEx prm = {0};
prm.pos = targetPosition;      // pulse
prm.vel = maxVelocity;         // pulse/ms
prm.acc = acceleration;        // pulse/ms²
prm.dec = deceleration;        // pulse/ms²
prm.percent = 0;               // S-curve % [0-100], asymmetric curve when both ends set
prm.velStart = 0;
prm.velEnd = 0;
prm.accStartPercent = 0;       // [0-100] asymmetric acc
prm.decEndPercent = 0;         // [0-100] asymmetric dec

GTN_MoveAbsoluteEx(core, axis, &prm);
// Motion starts immediately — poll GTN_GetSts for completion
```

**Struct `TMoveAbsolutePrmEx`:**
```c
typedef struct {
    double pos;              // Target position (pulse)
    double vel;              // Max velocity (pulse/ms)
    double acc;              // Acceleration (pulse/ms²)
    double dec;              // Deceleration (pulse/ms²)
    double percent;          // S-curve % [0-100]
    double velStart;         // Start velocity
    double velEnd;           // End velocity
    double accStartPercent;  // Asymmetric acc start % [0-100]
    double decEndPercent;    // Asymmetric dec end % [0-100]
} TMoveAbsolutePrmEx;
```

### Jog Mode — Continuous Velocity

```c
GTN_PrfJog(core, axis);

TJogPrm jogPrm = {acc, dec};
GTN_SetJogPrm(core, axis, &jogPrm);

GTN_SetVel(core, axis, targetVel);  // May be >maxVel (capped)
GTN_UpdatePro(core, &axis, 1);      // Start continuous motion

// Change speed on-the-fly:
GTN_SetVel(core, axis, newVel);

// Smooth stop:
GTN_Stop(core, 1 << (axis-1), 1);   // smooth=1: decel to stop
// Or: GTN_StopPro(core, &axis, 1, 1);
```

### GEAR — Electronic Gearing

Slave axis tracks master axis encoder at a configurable ratio.

```c
GTN_PrfGear(core, slaveAxis);

TGearPrm gearPrm;
gearPrm.masterAxis = masterAxis;     // Which axis to follow
gearPrm.ratio = 2.0;                 // slave = master * ratio
gearPrm.masterSource = 0;            // 0=encoder, 1=profile position
GTN_SetGearPrm(core, slaveAxis, &gearPrm);

GTN_Update(core, 1 << (slaveAxis-1));
```

**Clutch engagement** (smooth gear engagement at specific master positions, can set engagement distance window):
Use `GTN_PrfGearEx` + `GTN_SetGearPrmEx` for advanced clutch control.

### FOLLOW — Electronic Cam

Slave follows master via a pre-defined position table.

```c
GTN_PrfFollowEx(core, slaveAxis);

// Option 1: Single FIFO
GTN_FollowData(core, slaveAxis, masterInterval, slavePosArray, count);
GTN_UpdatePro(core, &slaveAxis, 1);

// Option 2: Double FIFO (ping-pong)
GTN_FollowData(core, slaveAxis, &data1, count1);  // Load buffer 1
GTN_FollowData(core, slaveAxis, &data2, count2);  // Load buffer 2
GTN_UpdatePro(core, &slaveAxis, 1);
```

### PT (Position-Time) Mode

User-defined position-vs-time points. The controller interpolates between them.

```c
GTN_PrfPt(core, axis);

// Push data points
TPtData ptData;
ptData.time = 100;       // ms from segment start
ptData.pos = 5000;       // pulse
ptData.percent = 0;      // smoothing
GTN_PtData(core, axis, &ptData);
// ... push more points ...

GTN_PtStartPro(core, &axis, 1);  // Start
```

**Sub-types** (select via `GTN_SetPtMode`):
- **Complete mode**: Position reached at exact time or waits
- **Continuous mode**: Uses actual position at time step (no waiting)
- **Percent mode**: % distance between positions for smooth transitions

### PVT (Position-Velocity-Time) Mode

Similar to PT but each point includes velocity.

```c
GTN_PrfPvt(core, axis);
TPvtData pvtData;
pvtData.time = 100;
pvtData.pos = 5000;
pvtData.vel = 50;        // Velocity at this point
GTN_PvtData(core, axis, &pvtData);
GTN_PvtStartPro(core, &axis, 1);
```

### Handwheel (MPG) Mode

Manual pulse generator mode for manual axis control.

```c
GTN_PrfHandwheel(core, axis);
// Configure ratios, enable rounding, etc.
GTN_StartHandwheel(...);
```

---

## Multi-Axis Interpolation

Interpolation uses **coordinate channels** (crd), not single axes.

```c
// 2D Linear interpolation
TCrdPrm crdPrm;
crdPrm.dimension = 2;
crdPrm.synVelMax = maxVel;
crdPrm.synAccMax = maxAcc;
// ... more fields

// Queue a segment
TCrdDataLine lineData;
lineData.x = 10000;   // pulse
lineData.y = 20000;
GTN_CrdLine(crd, &crdPrm, (1<<xAxis-1 | 1<<yAxis-1), &lineData, opt, sense);

// Queue arc
TCrdDataArc arcData;
arcData.xEnd = 20000;  arcData.yEnd = 30000;
arcData.xCenter = 5000; arcData.yCenter = 5000;
GTN_ArcXYC(crd, &crdPrm, mask, &arcData, opt, sense);

// Start all queued segments
GTN_CrdStart(crd, mask, opt);

// During motion: maintain buffer — check space, add more segments
long space;
GTN_CrdGetSpace(crd, &space);
```

**Available interpolation shapes:**
- `GTN_CrdLine` — Linear
- `GTN_ArcXYC` — Arc (center mode)
- `GTN_ArcXYR` — Arc (radius mode)
- `GTN_ArcXYZ` — 3D arc
- `GTN_Helix` — Helix
- `GTN_Ellipse` — Ellipse
- `GTN_CrdDma` — DMA streaming (high-throughput)

**Look-ahead / Path optimization:**
`GTN_SetPathOptimizePrmLa` — Configures look-ahead velocity planning for smoothing path corners.

---

## Axis Operations

### Essential
```c
GTN_AxisOn(core, axis);           // Enable motor (servo on)
GTN_AxisOff(core, axis);          // Disable motor (servo off)
GTN_ClrSts(core, axis, count);    // Clear status/alarm bits
GTN_ZeroPos(core, axis, count);   // Zero the position counter
GTN_Stop(core, mask, smooth);     // Stop motion (smooth=0: emergency)
```

### Position Reading
```c
GTN_GetPrfPos(core, axis, &pos, 1, &clock);     // Profile (commanded) position
GTN_GetEncPos(core, axis, &pos, 1, &clock);     // Encoder position (axis encoder)
GTN_GetAxisEncPos(core, axis, &pos, 1, &clock); // Axis encoder (may differ)
```

### Status
```c
long sts; unsigned long clock;
GTN_GetSts(core, axis, &sts, 1, &clock);        // Basic 32-bit status
GTN_GetStsEx(core, axis, &stsEx, 1, &clock);    // Extended status
```

### Velocity/Accel Override
```c
GTN_SetOverride(core, axis, ratio);  // Velocity override ratio (e.g. 0.5 = 50%)
```

### Multi-Axis Convenience
```c
GTN_MultiAxisOn(core, axisArray, count);
GTN_MultiAxisOff(core, axisArray, count);
GTN_UpdatePro(core, axisArray, count);  // Preferred over bitmask Update
```

---

## Homing (回零)

```c
THomePrm homePrm;
homePrm.mode = HOME_MODE_LIMIT;             // Homing mode
homePrm.moveVel = vel;
homePrm.homeVel = lowVel;
homePrm.homeAcc = acc;
homePrm.homeDir = 1;                        // Direction
homePrm.homeOffset = 0;                     // Offset from home position

GTN_GoHome(core, axis, &homePrm);
// Or two-step: GTN_Home + polling
```

Modes include limit switch homing, index (Z-signal) homing, etc. Full reference at `编程手册/回零功能/回零功能.html`.

---

## Hardware IO

### Digital IO
```c
GTN_SetDo(core, index, value);      // Set DO output
GTN_GetDo(core, index, &value);     // Read DO state
GTN_SetDoBit(core, index, bit, 1);  // Set single bit
GTN_GetDi(core, index, &value);     // Read DI input
GTN_GetDiBit(core, index, bit);     // Read single bit
```

### Analog IO
```c
GTN_SetDac(core, index, value);     // Set DAC output (V or mA)
GTN_GetAdc(core, index, &value);    // Read ADC input
```

### Encoder / Pulse / MRAM
```c
GTN_GetEncPos(core, axis, &pos, 1, &clock);      // Read encoder
GTN_SetEncoderPrm(...);                            // Configure encoder
GTN_SetPulseMode(...);                             // Configure pulse output
GTN_SetMRamData / GTN_GetMRamData;                 // Persistent storage (MRAM)
```

### Resource Query
```c
long count;
GTN_GetResCount(core, MC_AXIS, &count);    // Number of axes
GTN_GetResCount(core, MC_DAC, &count);     // Number of DACs
GTN_GetResCount(core, MC_ENCODER, &count); // Number of encoders
GTN_GetResCount(core, MC_LASER, &count);   // Number of laser resources
// ... similar for other resource types
```

---

## Control Mode (轴控制模式)

Select between open-loop (step/dir) and closed-loop (servo) control:

```c
GTN_SetControlMode(core, axis, mode);  // MC_OPEN_LOOP or MC_CLOSED_LOOP
```

For closed-loop mode, configure following error limits:
```c
GTN_SetPosErr(core, axis, errorLimit);  // Closed-loop following error alarm
```

---

## Watch Variable System

Many status reads use a unified "watch variable" system. Key watch variable types:
- `WATCH_VAR_GPI` / `WATCH_VAR_GPO` — General-purpose IO
- `WATCH_VAR_AXIS_STS` — Axis status
- `WATCH_VAR_PRF_POS` — Profile position
- `WATCH_VAR_ENC_POS` — Encoder position
- `WATCH_VAR_MC_VAR` — MC system variables

Used extensively by Event-Task mechanism (see api-advanced.md).
