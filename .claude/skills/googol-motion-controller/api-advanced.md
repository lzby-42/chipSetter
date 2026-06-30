# Advanced Features API Reference

## Laser Control (激光功能)

**Hardware:** Output through HSO (High Speed Output) differential signals. 1 laser channel = 2 HSO channels (switch + PWM). GNM403 supports 2 lasers; GTM+laser daughter board supports up to 10 lasers total.

### Output Modes

**Mode 1: Duty Cycle** — Fixed PWM frequency, energy = duty cycle
```c
GTN_LaserPowerMode(core, laser, LASER_POWER_MODE_PWM, minPower, maxPower);
GTN_LaserOutFrq(core, laser, freqKhz);      // PWM frequency in kHz
GTN_LaserPrfCmd(core, laser, powerPercent);  // 0-maxPower sets duty cycle
GTN_LaserOn(core, laser);
```

**Mode 2: Frequency** — Fixed pulse width, energy = frequency
```c
GTN_LaserPowerMode(core, laser, LASER_POWER_MODE_FREQ, minPower, maxPower);
GTN_SetPulseWidth(core, laser, widthUs);     // Fixed pulse width in μs
GTN_LaserPrfCmd(core, laser, powerPercent);
GTN_LaserOn(core, laser);
```

**Mode 3: Analog Voltage** — Energy = analog output (pin4, not HSO)
```c
GTN_LaserPowerMode(core, laser, LASER_POWER_MODE_ANALOG, minPower, maxPower);
GTN_LaserPrfCmd(core, laser, powerPercent);
GTN_LaserOn(core, laser);
```

### Energy Follow

Laser energy tracks real-time motion velocity to avoid over-burn at corners and stops:
`output = ratio * velocity + minPower + GTN_LaserPrfCmd_value`
```c
GTN_LaserFollowMode(core, laser, followMode, ...);
GTN_LaserFollowRatio(core, laser, ratio);
```

### Laser Switch Delay
```c
GTN_SetLevelDelay(core, laser, onDelayUs, offDelayUs);  // 0-65535 μs each
```
On-delay compensates mechanical lag causing "matchstick head"; off-delay prevents incomplete endpoints.

### Buffer Commands (Interpolation Buffer)

Prefix `Buf` for synchronization with motion profiles:
```c
GTN_BufLaserOnEx(core, laser, &listInfo);
GTN_BufLaserOffEx(core, laser, &listInfo);
GTN_BufLaserPrfCmdEx(core, laser, power, &listInfo);
GTN_BufLaserFollowModeEx(...);
GTN_BufLaserFollowRatioEx(...);
```

### Waveform Control (波形)

For complex timed laser pulse patterns synchronized with motion:
```c
GTN_LoadWaveformParameter(...);        // Download waveform data
GTN_SetWaveformMode(core, channel, mode);
GTN_EnableWaveform(core, channel, 1);

// Dual-output waveform
GTN_SetWaveformPairPrm(core, channel, &params);
GTN_EnableWaveformPair(core, channel, 1);
GTN_SetWaveformPairAhead(core, channel, &aheadPrm);  // Advance mode
```

Waveform can associate with PSO output — each waveform pulse is validated against encoder position.

### Hardware Channel Binding
```c
// Must bind laser to HSO/LASER hardware before use
GTN_SetTerminalPermitEx(core, resource, ...);
GTN_GetTerminalPermitEx(core, resource, ...);
```

---

## Position Compare Output / PSO (位置比较输出)

Compare encoder position against target values to trigger digital output with sub-microsecond precision. Used for laser marking, inkjet printing, vision triggering.

### Mode Selection
```c
GTN_SetPosCompareModeEx(core, pso, &modePrm);  // Set FIFO/Linear/PSO mode
GTN_SetPosCompareReference(core, pso, refAxis); // Reference axis for comparison
```

### FIFO Mode — Non-uniform points
```c
GTN_PosCompareStart(core, pso);
// Push individual compare points
GTN_PosCompareData(core, pso, pos);                  // 1D
GTN_PosCompareData2D(core, pso, xPos, yPos);         // 2D
GTN_PosCompareData2DMass(core, pso, dataArray, n);   // Batch (mode 16)
GTN_PosCompareStop(core, pso);
```

### Linear Mode — Uniform spacing
```c
GTN_SetPosCompareLinear(core, pso, startPos, interval, count, outType);
// 2D linear
GTN_SetPosCompareLinear2D(core, pso, startX, startY, intX, intY, count, outType);
// Multi-group buffered
GTN_SetPosCompareLinearBuf(core, pso, &groupPrm, groupCount);
```

### PSO Mode (Position-Synchronized Output)

**Immediate PSO (mode=2):** Output fires at equal cumulative distance intervals, immediate start.
**Wait-for-Arrival PSO (mode=3):** Output only when encoder position confirms arrival within error band. Critical for precision laser on/off timing.

```c
GTN_SetPosComparePsoPrm(core, pso, spacing);  // Pulse spacing in encoder counts
GTN_PosCompareStart(core, pso);

// Buffer-controlled PSO (start/stop within interpolation path)
GTN_BufPosCompareStartEx(core, pso, &listInfo);
GTN_BufPosCompareStopEx(core, pso, &listInfo);
```

Supports 1D, 2D, and 3D PSO.

### DMA for High-Speed PSO

Prevents FIFO underrun by buffering on PC side:
```c
GTN_PosCompareHsOn(core, pso, link);   // Enable DMA, link = unique channel ID
// Push data faster than controller consumption rate
GTN_PosCompareHsOff(core, pso);
```
Default PC buffer: 1000 segments. Auto-flush when ≥200 segments accumulated.

### Reference / Addition Axes

PSO can use a second addition axis for combined comparison:
```c
GTN_SetPosCompareAddition(core, pso, addAxis);  // e.g., encoder + encoder
```

---

## Event-Task Mechanism (事件任务机制)

Interrupt-driven automation: hardware conditions (Events) trigger actions (Tasks) within a single interrupt cycle.

### Events — Trigger Conditions
```c
TEvent event;
event.loop = 0;                        // 0 = infinite, N = count
event.var.type = WATCH_VAR_GPI;        // What to watch
event.var.index = 1;                   // Which instance
event.condition = WATCH_CONDITION_EQ;  // EQ, NE, GT, LT, GE, LE
event.value = 0;                       // Compare value (GPI 1 == 0 triggers)
GTN_AddEvent(core, &event, &eventId);
```

**Supported watch variable types:** `WATCH_VAR_GPI`, `WATCH_VAR_AXIS_STS`, `WATCH_VAR_PRF_POS`, `WATCH_VAR_ENC_POS`, `WATCH_VAR_MC_VAR`, etc.

**Conditions:** `WATCH_CONDITION_EQ`, `WATCH_CONDITION_NE`, `WATCH_CONDITION_GT`, `WATCH_CONDITION_LT`, `WATCH_CONDITION_GE`, `WATCH_CONDITION_LE`.

### Tasks — Actions
```c
// Set DO bit
TTaskSetDoBit task;
task.doType = MC_GPO;
task.doIndex = 1;
task.doValue = 0;
task.mode = TASK_SET_DO_BIT_MODE_NONE;
GTN_AddTaskPro(core, &task, &taskId);
```

Task types include: set DO, toggle DO, modify MC variable, start/stop motion, etc.

### Link & Activate
```c
GTN_AddEventTaskLink(core, eventId, taskId);  // Bind one Event to one Task
GTN_EventOn(core);                             // Start detection

// Poll for trigger
TEventStatus eventSts;
GTN_GetEventStatus(core, &eventSts);
if (eventSts.eventHit) { /* event fired */ }

GTN_EventOff(core);                            // Stop detection
```

### Workflow Pattern
```c
GTN_ClearEvent(core);
GTN_ClearTask(core);
GTN_ClearEventTaskLink(core);
// Add events + tasks + links
GTN_EventOn(core);
// ... wait for triggers ...
GTN_EventOff(core);
```

Key: One Event can link to multiple Tasks. One Task can be linked from multiple Events. Each link is a 1:1 binding. Always clear before re-configuring.

---

## Command List (指令流)

Sequential execution of buffered commands. Many API functions support `TListInfo *pListInfo` parameter.

### TListInfo Structure
```c
TListInfo listInfo;
listInfo.list = 1;       // >0 = buffer mode, 0 or NULL = immediate
listInfo.modal = 1;      // Modal state (carries between commands)
listInfo.segNum = 1;     // Segment number
```

### Lifecycle
```c
// Push commands into list
GTN_SetDelay(core, &delay, &listInfo);  // listInfo.list > 0
GTN_SetDoBit(core, index, bit, val, &listInfo);
// ... more buffered commands ...

GTN_CommandListDataEnd(core, listInfo.list);    // Flush buffers
GTN_StartCommandList(core, listInfo.list);       // Execute

// Check status
long space, status;
GTN_GetCommandListSpace(core, list, &space);    // Don't overflow buffer
GTN_GetCommandListStatus(core, list, &status);

// Stop / Resume
GTN_StopCommandList(core, list);
GTN_StartCommandList(core, list);               // Resume from stop
// Note: non-modal single-axis PTP motion is NOT resumable

// Multiple lists simultaneously
GTN_StartMultiCommandList(core, listArray, count);
```

### Error Codes (11000 range)
- 11001: Data after `CommandListDataEnd` already called
- 11002: Buffer full
- 11007: List empty/not empty mismatch
- 11008: List already executing
- 11014: Static mode, no new commands during execution

If command list underruns (data not pushed fast enough), it auto-stops. Check underrun flag via `GTN_GetCommandListStatus`.

---

## Compensation Functions (补偿功能)

All compensations are added directly to controller output, NOT to profile position. Use `GTN_GetCompensate` to read all current values.

### Backlash Compensation
```c
GTN_SetBacklash(core, axis, compValue, changeValue, direction);
// compValue: pulse amount
// changeValue: ramp speed (0 = instant in one cycle)
// direction: 0 = positive→negative reversal, 1 = negative→positive reversal
```

### Pitch Error (Lead Screw) Compensation
```c
GTN_SetLeadScrewComp(core, axis, startPos, totalLen, nodeCount,
                     fwdCompValues, revCompValues);
GTN_EnableLeadScrewComp(core, axis, 1);
```
Table-based: measurement data → forward/reverse compensation arrays per position node.

### 2D/3D Position Compensation
```c
GTN_SetPosOffset2D(core, axis, &params);
GTN_SetPosOffset2DTable(core, axis, tableData, nodeCount);
GTN_SetPosOffset3D(core, axis, &params);
GTN_SetPosOffset3DTable(core, axis, tableData, nodeCount);
```

### Cross / Non-Orthogonal / Path / Dynamic Compensation
```c
GTN_SetCrossComp(core, srcAxis, dstAxis, ratio);
GTN_SetNonOrthogonal(core, axis, angle);
GTN_SetDynamicCompensation(core, axis, &params);
GTN_SetDynamicCompensationTable(core, axis, tableData, count);
GTN_SetPathCompensation(core, axis, &params);
GTN_SetPathCompensationTable(core, axis, tableData, count);
GTN_SetSyncCoord(core, axis, &params);
```

### Reading Compensation Values
```c
double backlash, pitchError, crossError, encPos, prfPos;
GTN_GetCompensate(core, axis, &pitchError, &crossError,
                  &backlashError, &encPos, &prfPos);
```

---

## Galvo Scanner (振镜功能)

```c
GTN_ScanSuperpose(core, scan, &prm);    // Flying superposition mode
GTN_ScanMotionPro(core, scan, &prm);    // Scan motion (Pro variant)
// DMA for scan data
GTN_ScanHsOn(core, scan, link);
GTN_ScanHsOff(core, scan);
```

Scan + DMA + Laser = integrated marking system. Laser outputs synchronize with scan positions.

---

## Safety Mechanisms (安全机制)

### Soft Limits
```c
GTN_SetSoftLimitEx(core, axis, posLimit, negLimit);
GTN_LmtsOnEx(core, axis);        // Sequential axes from axis
GTN_LmtsOffEx(core, axis);
GTN_GetLimitInfo(core, axis, &info);
```
When triggered: emergency stop deceleration (default 1000 pulse/ms², configurable via `GTN_SetStopDec`). Limit flags are modal — must `GTN_ClrSts` to clear. **Recommended to set after homing.**

### Following Error
```c
GTN_SetPosErr(core, axis, limit);          // Closed-loop
GTN_SetAxisPosErrLimit(core, axis, limit); // Open-loop
GTN_GetAxisError(core, axis, &error);      // profile - encoder difference
GTN_SetMcMode(core, axis, mode);           // Follow error source config
```
Checked every control cycle. Dedicated gLink-II drives can report drive-computed following error for lower latency.

### Circular Safety Zone
```c
GTN_SetAxisCircularSafetyZone(core, axis1, axis2, centerX, centerY, radius);
```
Axes restricted within circle. Overstepping → emergency stop (not precision boundary stop).

### Network Disconnection Safe Mode
```c
GTN_RN_SetStationSafeModeOut(core, station, &outputStates);   // Safe state outputs
GTN_RN_SetStationSafeModeControl(core, station, mode);        // Auto/manual mode
GTN_RN_ClearStationSafeModeStatus(core, station);             // Clear on reconnect
```

---

## EtherCAT Support (GSN/GSNE Series)

### Open EtherCAT Controller
```c
// For GSNE series, use GTN_Open instead of GTN_OpenCard
short cardInfo;  // Structure depends on connection type
GTN_Open(core, &cardInfo);
```

### Network Init with EtherCAT
```c
GTN_NetInit(NET_INIT_MODE_XML_STRICT, "network.xml", 120, &status);
```
The XML is generated by MotionStudio. All slave devices (drives, IO modules) are auto-discovered and mapped.

### EtherCAT-Specific Error Codes
- 10: PDO channel error
- 11901-11923: EtherCAT init failures (library load, PCI lock, timeout, slave mismatch, cycle mismatch)
- 20001-20019: XML config mismatch (controller type, slave type, physical ID)

---

## High-Speed Capture (高速硬件捕获)

Hardware-level position latch on external signal edge (Home/Index/Probe). The latched position is accurate to within one encoder count — far better than software polling.

### Trigger Configuration (`GTN_SetTriggerPrm` ★)

```c
typedef struct TriggerPrm {
    short latchType;       // MC_ENCODER(23) or MC_AU_ENCODER(26)
    short latchIndex;      // encoder index
    short probeType;       // CAPTURE_HOME(1), CAPTURE_INDEX(2), CAPTURE_PROBE(3)
    short probeIndex;      // DI channel for the capture signal
    short sense;           // ★ 0=falling edge, 1=rising edge
    short loopType;        // 1=DSP循环捕获 (only mode supported)
    long  offset;          // capture position offset (0 for non-GTM modules)
    unsigned long loop;    // 0=infinite loop, N=loop count
    short windowOnly;      // 0=no window, 1=enable capture window
    short pad1;
    long  firstPosition;   // window start (windowOnly=1)
    long  lastPosition;    // window end (windowOnly=1)
    short fifoMode;        // 0=static (stop when full), 1=dynamic (overwrite old)
    short pad2[3];
    double pad3;
} TTriggerPrm;
```

### Capture Modes

| probeType | Value | Signal source | Needs MotionStudio? |
|---|---|---|---|
| `CAPTURE_HOME` | 1 | Dedicated Home DI | Yes — mapped in MotionStudio |
| `CAPTURE_INDEX` | 2 | Encoder Z-signal | No |
| **`CAPTURE_PROBE`** | **3** | **Any GPI channel** ★ | **No — `probeIndex` directly specifies DI** |

### Basic Single Capture

```c
TTriggerPrm prm = {0};
prm.latchType  = MC_ENCODER;
prm.latchIndex = 1;
prm.probeType  = CAPTURE_HOME;    // or CAPTURE_PROBE for GPI
prm.probeIndex = 1;               // DI channel (for PROBE mode)
prm.sense      = 0;               // 0=falling, 1=rising edge
prm.loop       = 1;               // capture once
GTN_SetTriggerPrm(core, axis, &prm);

// Poll for capture
long capSts, latchPos;
do {
    GTN_GetTriggerStatusEx(core, axis, &capSts, NULL);
} while (!(capSts & 0x1));         // bit0 = captured

GTN_GetTriggerStatusEx(core, axis, &capSts, &latchPos);
printf("Captured at encoder position: %ld\n", latchPos);
```

### Repetitive (Loop) Capture

Set `loop = 0` for infinite, or N for N captures. Use `GTN_GetTriggerLatchValue` to batch-read:

```c
prm.loop      = 0;    // infinite
prm.fifoMode  = 1;    // dynamic FIFO (overwrites when full)
GTN_SetTriggerPrm(core, axis, &prm);

// ... let captures accumulate ...

long values[2048];
short count = GTN_GetTriggerLatchValue(core, axis, values, 2048);
printf("Read %d latched positions\n", count);
```

### Critical Constraints

- **Trigger, capture source (DI/Home/Index), and latched encoder MUST be on the same network module**
- Each module has limited Trigger resources (GNM4xx=4, GNM6xx=6, servo drive=1, GTM=1 per axis)
- Max 2048 latched values per trigger in FIFO mode

### TouchProbe (EtherCAT Drives Only)

EtherCAT drives have a built-in probe function accessible via `GTN_SetTouchProbeFunctionEx`:

```c
GTN_SetTouchProbeFunctionEx(core, axis,
    1,                                       // Probe1Enable
    ECAT_PROBE_TRIGGER_TYPE_FIRST_EVENT,     // single trigger
    ECAT_PROBE_TRIGGER_LEVEL_POS,            // ★ 1=rising, -1=falling, 0=both
    0, 0, 0);                                // Probe2 unused

// Check status
short probeSts;
GTN_GetTouchProbeStatus(core, axis, &probeSts);
// bit0=Probe1 triggered, bit1=Probe1 rising edge
```

Requires EtherCAT CoE object 60B8h in PDO mapping. If `GTN_SetTouchProbeFunction` returns 1, check PDO config.

---

## High-Speed Read (高速读功能)

Bulk register reading at high speed:
```c
GTN_HsReadConfig(...);
GTN_HsReadStart(...);
GTN_HsReadStop(...);
```

---

## Lua Scripting (Lua脚本)

The controller supports embedded Lua scripts for custom logic:
```c
// See 编程手册/lua脚本/ for Lua API reference
```
Lua can call motion APIs, read IO, and implement custom sequences running on-controller.
