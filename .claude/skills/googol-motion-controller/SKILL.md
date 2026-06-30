---
name: googol-motion-controller
description: Use when writing C++ code for 固高 (Googol) GSN/GTN/GVN series networked motion controllers, looking up API functions (GTN_*), debugging motion control issues, configuring controller resources (axes/IO/laser/PSO), or needing to understand motion controller programming patterns and workflows.
---

# Googol Networked Motion Controller Development

## Overview

固高 network-type motion controllers (GSN EtherCAT, GTN/GVN gLink-II) use the **gxn.h / gxn.lib** C API with `GTN_` prefix. All functions return `short` (0 = `CMD_SUCCESS`). Axes and cores are **1-indexed**. The controller uses an interrupt-driven real-time architecture with ms-level cycle times.

## Documentation Source

API documentation is local HTML at this site (built with MkDocs). Key paths:
| Path | Content |
|---|---|
| `编程手册/` | Programming manual by subsystem |
| `指令说明/` | Per-function API reference (~400+ functions) |
| `例程/` | C++ examples organized by feature |
| `附录/返回值/` | Error/return code reference |

## Which API to Use — Quick Finder

**"I want to..."** → API:

| Goal | First call(s) | Then... |
|---|---|---|
| **Init controller** | `GTN_OpenCard` (PCIe) or `GTN_Open` (EtherCAT) → `GTN_NetInit` → `GTN_LoadConfig` | `GTN_ClrSts` → `GTN_AxisOn` |
| **Single-axis PT move** | `GTN_PrfTrap` + `GTN_SetTrapPrm` | `GTN_SetVel` + `GTN_SetPos` → `GTN_UpdatePro` ★ |
| **Multi-axis synced PT** | Configure each axis → `GTN_Update` (bitmask) or `GTN_UpdatePro` (axis array) ★ | `GTN_UpdatePro` preferred for non-consecutive axes |
| **One-call absolute move** | `GTN_MoveAbsoluteEx(core, axis, &prm)` ★ — simpler than Trap | Poll `GTN_GetSts` bit 10 |
| **Continuous jog** | `GTN_PrfJog` + `GTN_SetJogPrm` | `GTN_SetVel` → `GTN_UpdatePro` ★ |
| **Electronic gearing** | `GTN_PrfGear(core, slave)` | `GTN_SetGearPrm(slave)` → `GTN_Update` |
| **Electronic cam** | `GTN_PrfFollowEx` | `GTN_FollowData` → `GTN_UpdatePro` |
| **Multi-axis interpolation** | `GTN_CrdLine` / `GTN_ArcXYC` / `GTN_ArcXYR` | CrdData → CrdStart |
| **PT/PVT motion** | `GTN_PrfPt` / `GTN_PrfPvt` | `GTN_PtData` → `GTN_PtStartPro` |
| **Wait for motion done** | `GTN_GetSts(core, axis, &sts, 1, &clock)` | `sts & 0x400` (planner running bit 10) |
| **Read actual position** | `GTN_GetPrfPos` (profile) / `GTN_GetEncPos` / `GTN_GetAxisEncPos` (encoder) | |
| **Stop motion** | `GTN_StopPro(core, &axis, 1, smooth)` ★ — or `GTN_Stop(core, mask, smooth)` | Smooth=0: emergency decel; smooth=1: decelerate to stop |
| **Multi-axis enable** | `GTN_MultiAxisOn(core, axisArray, count)` ★ | `GTN_MultiAxisOff` to disable |
| **Use GPI as Home trigger** | `GTN_SetTriggerPrm` → `CAPTURE_PROBE` → `GTN_GoHome`(triggerIndex) | See patterns.md "通用 DI（GPI）替代专用 Home DI" |
| **Home an axis** | `GTN_ExecuteStandardHome(core, axis, &prm)` ★ — 36种CANopen DS402标准模式 | `prm.mode`: 3-6/19-22=Home(DI)触发, 23-30=Home+限位, 33-34=仅Index, 35=当前位置即零点 |
| **Home status & params** | `GTN_GetStandardHomeStatus` / `GTN_GetStandardHomePrm` ★ | Legacy: `GTN_GetHomeStatus` / `GTN_GetHomePrm` (不推荐) |
| **DI-edge home (rotary axis)** | Standard mode 19-22 (Home only) or 3-6 (Home+Index) | Edge polarity via MotionStudio DI reverse setting |
| **Legacy Smart Home (不推荐)** | `GTN_GoHome(core, axis, &homePrm)` — 保留用于兼容，`THomePrm.edge`可直接设边沿 | 新项目应使用 `GTN_ExecuteStandardHome` |
| **Use GPI as Home trigger** | `GTN_SetTriggerPrm` → `CAPTURE_PROBE` → `GTN_ExecuteStandardHome` or `GTN_GoHome` | See patterns.md "通用 DI（GPI）替代专用 Home DI" |
| **Laser on/off** | `GTN_LaserOn` / `GTN_LaserOff` | `GTN_LaserPowerMode` → `GTN_LaserPrfCmd` |
| **PSO (position output)** | `GTN_SetPosCompareModeEx` | `GTN_PosCompareStart` |
| **Event→Task (interrupt)** | `GTN_AddEvent` → `GTN_AddTaskPro` → `GTN_AddEventTaskLink` | `GTN_EventOn` |
| **Command list (batch)** | Use `TListInfo.list > 0` in API calls | `GTN_StartCommandList` |
| **Read IO state** | `GTN_GetDo` / `GTN_GetDi` | `GTN_SetDo` / `GTN_SetDoBit` for output |
| **Set soft limits** | `GTN_SetSoftLimitEx` | `GTN_LmtsOnEx` |
| **Backlash comp** | `GTN_SetBacklash` | Direction-dependent, added directly to output |
| **Close controller** | `GTN_Close` | Call only once at app exit |

## Mandatory Initialization Sequence

Every application follows this exact order:

```c
#include "gxn.h"
#pragma comment(lib, "gxn.lib")

// 1. Open card (once per app)
short rtn = GTN_OpenCard(CHANNEL_PCIE, NULL, NULL);   // PCIe cards
// Or for EtherCAT: GTN_Open(core, &cardInfo)

// 2. Init network — maps slave devices via MotionStudio-generated XML
long status;
rtn = GTN_NetInit(NET_INIT_MODE_XML_STRICT, "network.xml", 120, &status);

// 3. Load config — maps software resources (axes, profiles) to hardware
rtn = GTN_LoadConfig(core, "config.cfg");

// 4. Clear status (MANDATORY after LoadConfig)
rtn = GTN_ClrSts(core, 1, axisCount);

// 5. Enable axes (for real motors)
rtn = GTN_AxisOn(core, axis);

// ... motion operations ...

// 6. Close (once at exit)
GTN_Close();
```

**Critical rules:**
- `GTN_OpenCard` and `GTN_Close` — call **exactly once** per application, at start and exit
- `GTN_ClrSts` **must** be called after `GTN_LoadConfig` or config won't take effect
- The `.cfg` file path **cannot exceed 125 characters**
- The `.xml` and `.cfg` files are generated by the **MotionStudio** tool

## Core API Naming Convention

| Pattern | Purpose | Examples |
|---|---|---|
| `GTN_Prf*` | Select motion profile/mode | `GTN_PrfTrap`, `GTN_PrfJog`, `GTN_PrfPt` |
| `GTN_Set*` / `GTN_Get*` | Parameter get/set | `GTN_SetVel`, `GTN_GetSts`, `GTN_SetPos` |
| `GTN_Move*` | One-call motion | `GTN_MoveAbsoluteEx`, `GTN_MoveVelocity` |
| `GTN_Update*` | Start buffered motion | `GTN_Update(mask)`, `GTN_UpdatePro(axisArr, n)` |
| `GTN_Axis*` | Axis control | `GTN_AxisOn`, `GTN_AxisOff`, `GTN_ZeroPos` |
| `GTN_Buf*` | Interpolation buffer cmd | `GTN_BufLaserOnEx`, `GTN_BufDelay` |
| `GTN_Crd*` | Coordinated (interpolation) | `GTN_CrdLine`, `GTN_CrdArcXYC` |
| `*Ex` / `*Pro` | Extended/pro variant | More parameters, axis arrays instead of bitmasks |

## Standard Error Handling Pattern

```c
short CommandHandler(const char* cmd, short err)
{
    printf("%s = %d\n", cmd, err);
    getchar();  // pause for operator
    return err;
}

// Usage: check every API call
short rtn = GTN_SomeFunction(core, axis, ...);
if (CMD_SUCCESS != rtn)  // or: if (0 != rtn)
    return CommandHandler("GTN_SomeFunction", rtn);
```

**Return value ranges (from `附录/返回值/`):**
- `0` = success / `CMD_SUCCESS`
- `-1 ~ -20` = host↔controller communication failure (PCIe timeout, double-open, etc.)
- `1 ~ 20` = execution error (parameter error, slave not connected, firmware mismatch)
- `11001-11091` = command list / group / axis errors
- `11700-11763` = look-ahead library errors
- `11901-11923` = EtherCAT / network init errors
- `20001-20138` = XML config mismatch errors

## Core Programming Models

### Model 1: Set-then-Update (Trap, Jog, Gear, Follow)
```
PrfSelect → SetParams → SetTargets → Update → PollStatus
```

### Model 2: One-Call Motion (MoveAbsolute, MoveVelocity)
```
MoveXxx(core, axis, &params) → PollStatus
```
No `Prf*` or `Update` needed — the function handles mode switch internally.

### Model 3: Data Buffer (PT, PVT, Follow FIFO)
```
PrfSelect → PushData (loop) → Start → PollStatus
```
Data is pushed to a FIFO. Multi-segment motion in one command.

### Model 4: Interpolation Channel
```c
GTN_CrdLine(crd, &crdPrm, mask, opt, sense);  // queue segment
// ... queue more segments ...
GTN_CrdStart(crd, mask, opt);                   // start all
// Poll: GTN_CrdGetSpace / GTN_CrdGetStatus while adding more
```
Channel-based with dynamic buffer management.

## Axis Addressing

- Axes: **1-indexed** (1 = first axis)
- Cores: 1 for single-core; multi-core uses `HS_CORE` / `GP_CORE` / `EC_CORE` macros
- Multi-axis: `(core, startAxis, axisCount)` for consecutive axes
- Bitmask: `GTN_Update(core, 1 << (axis-1))` for selecting which axes start

## Axis Status Bitfield (`GTN_GetSts` output)

| Bit | Hex | Decimal | Meaning |
|---|---|---|---|
| 0 | 0x001 | 1 | Emergency stop (alarm) |
| 1 | 0x002 | 2 | Drive alarm |
| 2 | 0x004 | 4 | Reserved |
| 3 | 0x008 | 8 | Reserved |
| 4 | 0x010 | 16 | Following error exceeded |
| 5 | 0x020 | 32 | Positive limit triggered |
| 6 | 0x040 | 64 | Negative limit triggered |
| 7 | 0x080 | 128 | Smooth stop IO |
| 8 | 0x100 | 256 | Emergency stop IO |
| 9 | 0x200 | 512 | Motor enabled (AxisOn) |
| **10** | **0x400** | **1024** | **Planner running** ← poll this for motion done |
| 11 | 0x800 | 2048 | In position |
| 12-31 | — | — | Extended status (use `GTN_GetStsEx` for full details) |

## Detailed Reference

- **[workflows.md](workflows.md)** — **Start here for coding.** 20+ complete, copy-paste-ready workflows for every motion mode and homing scenario (including GPI-as-Home), each with init → motion → poll-loop → verify. Includes shared helpers and a full application template.
- **[api-core.md](api-core.md)** — Core motion API signatures: Trap, Jog, PT/PVT, Gear, Follow, Interpolation, Homing (Smart Home + Standard Home + GPI), Axis ops, Hardware IO, Trigger
- **[api-advanced.md](api-advanced.md)** — Advanced API: Laser, PSO, Event-Task, Command List, Compensation, Galvo, Safety, EtherCAT, High-Speed Capture (Trigger/Probe/TouchProbe), Lua
- **[patterns.md](patterns.md)** — Cross-cutting patterns: polling, multi-axis, DMA, hardware binding, homing mode selection, GPI-as-Home workaround, common mistakes, debugging tips, MotionStudio workflow

For the full API reference, consult the local documentation pages under `编程手册/` and `指令说明/`.
