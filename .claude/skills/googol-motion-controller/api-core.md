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

The controller has **two independent homing APIs**. The official documentation marks `GTN_ExecuteStandardHome` as **推荐指令 (recommended)** and `GTN_GoHome` as **不推荐指令 (deprecated for new projects)**.

### Standard Home (`GTN_ExecuteStandardHome`) ★ — 36 CANopen DS402 modes, officially recommended

Uses `TStandardHomePrm` struct. 36 predefined modes covering all common homing sequences. Edge polarity is controlled by the **drive's DI reverse setting** (configured in MotionStudio), not via API parameter. The "左侧边沿/右侧边沿" in mode descriptions refer to physical sensor position relative to motion direction, NOT electrical edges.

**Standard Home modes for DI-based homing:**

| Modes | Signals | Description |
|---|---|---|
| 3-6 | Home + Index | DI edge → search to Index (most precise) |
| 7-10 | Home + PosLimit + Index | With positive limit |
| 11-14 | Home + NegLimit + Index | With negative limit |
| **19-22** | **Home only** | **DI trigger or release position = zero** ★ |
| **23-26** | **Home + PosLimit** | DI edge on positive-limit side |
| **27-30** | **Home + NegLimit** | DI edge on negative-limit side |
| 33-34 | Index only | Encoder Z-signal only (no DI needed) |
| 35 | None | Current position = zero (instant, no motion) |

**Basic example (mode 19 — Home only, positive direction):**
```c
TStandardHomePrm prm = {0};
prm.mode = 19;                     // ★ Home(DI) trigger or release = zero
GTN_ExecuteStandardHome(core, axis, &prm);

long sts; unsigned long clk;
do { GTN_GetSts(core, axis, &sts, 1, &clk); }
while (sts & 0x400);

long homeSts;
GTN_GetStandardHomeStatus(core, axis, &homeSts);
```

**Edge polarity for Standard Home:**
- Configured in **MotionStudio** → drive DI reverse setting
- If Home switch is normally-open (常开): set DI reverse=1
- If Home switch is normally-closed (常闭): set DI reverse=0
- Slave DI defaults to active-low → trigger = falling edge, leaving = rising edge

### Smart Home (`GTN_GoHome`) — Legacy, not recommended for new projects

Labeled as **不推荐指令** in official docs. Kept for backward compatibility. The key advantage over Standard Home is the `edge` field for explicit software control of capture polarity.

```c
typedef struct HomePrm {
    short mode;              // Homing mode constant
    short moveDir;           // Initial search direction: ≤0=negative, >0=positive
    short indexDir;          // Index search direction: ≤0=negative, >0=positive
    short edge;              // Capture edge: 0=falling, non-0=rising
    short triggerIndex;      // -1=use axis's own trigger, [1,8]=use other axis's trigger
    short pad1[3];           // Reserved, must be 0
    double velHigh;          // Home search speed (pulse/ms)
    double velLow;           // Index search speed (pulse/ms)
    double acc;              // Acceleration (pulse/ms²)
    double dec;              // Deceleration (pulse/ms²)
    short smoothTime;        // S-curve smoothing time (ms)
    short pad2[3];           // Reserved, must be 0
    long homeOffset;          // Offset from home capture position (pulse)
    long searchHomeDistance;  // Max Home search distance, 0=unlimited
    long searchIndexDistance; // Max Index search distance, 0=unlimited
    long escapeStep;          // Escape step when starting on limit switch (pulse)
    long pad3[2];            // Reserved, must be 0
} THomePrm;
```

**Mode constants:**

| Constant | Value | Signals | Equivalent Standard Mode |
|---|---|---|---|
| `HOME_MODE_LIMIT` | 10 | Limit only | 17-18 |
| `HOME_MODE_LIMIT_HOME` | 11 | Limit + Home | 7-14 |
| `HOME_MODE_LIMIT_INDEX` | 12 | Limit + Index | 1-2 |
| `HOME_MODE_LIMIT_HOME_INDEX` | 13 | Limit + Home + Index | 7-14 |
| `HOME_MODE_HOME` | 20 | Home(DI) only | **19-22** |
| `HOME_MODE_HOME_INDEX` | 22 | Home(DI) + Index | **3-6** |
| `HOME_MODE_HOME_LEVEL` | 24 | Home level (no capture) | — |
| `HOME_MODE_INDEX` | 30 | Index only | 33-34 |

### Continuously Rotating Axes

- **Standard Home**: mode 19-22 (Home only) or 3-6 (Home+Index for Index-latched precision)
- **Smart Home (legacy)**: `HOME_MODE_HOME_INDEX` (22) with `edge` field for explicit edge control
- Set `searchHomeDistance = 0` (Standard auto-handles this for unlimited rotation)
- The Home sensor is **always wired to a dedicated DI** on the slave drive

### Using Generic DI (GPI) as Home Signal — When MotionStudio Can't Map It

If MotionStudio cannot map the desired DI channel as `MC_HOME`, use `GTN_SetTriggerPrm` to redirect the trigger source to a GPI channel via `CAPTURE_PROBE`.

**★ Recommended path: Manual capture with Standard Home mode 35 (current pos = zero):**
```c
// Step 1: configure the trigger to use a GPI channel as capture source
TTriggerPrm triggerPrm = {0};
triggerPrm.latchType  = MC_ENCODER;
triggerPrm.latchIndex = 1;
triggerPrm.probeType  = CAPTURE_PROBE;   // ★ 3 — use general-purpose DI
triggerPrm.probeIndex = 2;               // ★ GPI channel number (your DI)
triggerPrm.sense      = 0;               // ★ 0=falling, 1=rising edge
triggerPrm.loop       = 0;
GTN_SetTriggerPrm(core, axis, &triggerPrm);

// Step 2: Start Trap move to sweep for the GPI edge
GTN_PrfTrap(core, axis);
GTN_SetVel(core, axis, 5.0);
GTN_SetPos(core, axis, 99999999);
GTN_Update(core, 1 << (axis - 1));

// Step 3: Wait for hardware capture
long capSts = 0;
do { GTN_GetTriggerStatusEx(core, axis, &capSts, NULL); }
while (!(capSts & 0x1));

// Step 4: Stop and zero
GTN_Stop(core, 1 << (axis - 1), 0);
Sleep(50);
GTN_ZeroPos(core, axis, 1);
```

**Legacy path: configure Trigger, then call `GTN_GoHome` (不推荐):**
```c
// Step 1: same trigger config as above
// Step 2: THomePrm homePrm = {0};
homePrm.mode         = HOME_MODE_HOME;    // 20
homePrm.triggerIndex = axis;             // point to configured trigger
homePrm.edge         = 0;
GTN_GoHome(core, axis, &homePrm);
```

**Probe type options for `TTriggerPrm.probeType`:**

| Constant | Value | Signal source |
|---|---|---|
| `CAPTURE_HOME` | 1 | Dedicated Home DI (mapped in MotionStudio) |
| `CAPTURE_INDEX` | 2 | Encoder Z-signal |
| `CAPTURE_PROBE` | 3 | **General-purpose GPI (any DI channel)** |

Key constraint: **Trigger, capture source (GPI), and latched encoder MUST be on the same network module.**

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
