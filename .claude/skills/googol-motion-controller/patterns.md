# Coding Patterns & Workflows

## Complete Motion Workflow (Trap Example)

```c
#include "gxn.h"
#pragma comment(lib, "gxn.lib")

short CommandHandler(const char* cmd, short err) {
    if (CMD_SUCCESS != err) printf("%s = %d\n", cmd, err);
    return err;
}

int main() {
    short rtn, core = 1, axis = 1;

    // 1. Open
    rtn = GTN_OpenCard(CHANNEL_PCIE, NULL, NULL);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_OpenCard", rtn);

    // 2. Init network
    long netStatus;
    rtn = GTN_NetInit(NET_INIT_MODE_XML_STRICT, "network.xml", 120, &netStatus);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_NetInit", rtn);

    // 3. Load config
    rtn = GTN_LoadConfig(core, "config.cfg");
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_LoadConfig", rtn);

    // 4. Clear status (MANDATORY)
    rtn = GTN_ClrSts(core, axis, 1);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_ClrSts", rtn);

    // 5. Enable
    rtn = GTN_AxisOn(core, axis);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_AxisOn", rtn);

    // 6. Select Trap mode + set params
    rtn = GTN_PrfTrap(core, axis);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_PrfTrap", rtn);

    TTrapPrm trapPrm = {0.1, 0.1, 0, 50};  // acc, dec, velStart, smoothTime
    rtn = GTN_SetTrapPrm(core, axis, &trapPrm);
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_SetTrapPrm", rtn);

    // 7. Set target
    rtn = GTN_SetVel(core, axis, 50.0);     // 50 pulse/ms
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_SetVel", rtn);
    rtn = GTN_SetPos(core, axis, 100000);   // 100000 pulse
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_SetPos", rtn);

    // 8. Start motion
    rtn = GTN_Update(core, 1 << (axis - 1));
    if (CMD_SUCCESS != rtn) return CommandHandler("GTN_Update", rtn);

    // 9. Wait for completion
    long sts; unsigned long clock;
    do {
        rtn = GTN_GetSts(core, axis, &sts, 1, &clock);
        if (CMD_SUCCESS != rtn) return CommandHandler("GTN_GetSts", rtn);
    } while (sts & 0x400);  // Bit 10 = planner running

    // 10. Read final position
    long prfPos;
    rtn = GTN_GetPrfPos(core, axis, &prfPos, 1, &clock);
    printf("Motion complete. Profile pos: %ld\n", prfPos);

    // 11. Cleanup
    GTN_AxisOff(core, axis);
    GTN_Close();
    return 0;
}
```

## Multi-Axis Synchronized Motion

Use `Update` mask for simultaneous start:
```c
// Configure both axes independently
GTN_PrfTrap(core, 1);
GTN_SetTrapPrm(core, 1, &prm1);
GTN_SetVel(core, 1, 50);
GTN_SetPos(core, 1, 100000);

GTN_PrfTrap(core, 2);
GTN_SetTrapPrm(core, 2, &prm2);
GTN_SetVel(core, 2, 30);
GTN_SetPos(core, 2, 50000);

// Start BOTH simultaneously
GTN_Update(core, (1 << 0) | (1 << 1));  // Axes 1 and 2 start together

// Wait for BOTH complete
long sts1, sts2; unsigned long clk;
do {
    GTN_GetSts(core, 1, &sts1, 1, &clk);
    GTN_GetSts(core, 2, &sts2, 1, &clk);
} while ((sts1 & 0x400) || (sts2 & 0x400));
```

Or use `GTN_UpdatePro` with axis array (preferred for non-consecutive axes):
```c
short axes[] = {1, 3, 5};
GTN_UpdatePro(core, axes, 3);
```

## Polling Pattern Options

### Polling with timeout
```c
long sts; unsigned long clock;
int timeout = 30000;  // 30 seconds
int elapsed = 0;
do {
    Sleep(1);  // 1ms polling interval
    GTN_GetSts(core, axis, &sts, 1, &clock);
    elapsed++;
} while ((sts & 0x400) && elapsed < timeout);

if (elapsed >= timeout) {
    printf("Motion timeout!\n");
    GTN_Stop(core, 1 << (axis - 1), 0);  // Emergency stop
}
```

### Wait for in-position (more precise)
```c
do {
    GTN_GetSts(core, axis, &sts, 1, &clock);
} while (!(sts & 0x800));  // Bit 11 = in-position
```

## Hardware Channel Binding Pattern

Laser and PSO share HSO (High Speed Output) channels. Always set control rights before use:
```c
// Default GNM403:
//   HSO00/01 → Laser
//   HSO02/03 → Position Compare

// Bind laser to HSO0/1
GTN_SetTerminalPermitEx(core, MC_LASER, laserIndex, terminalIndex, 1);
// Bind PSO to HSO2/3
GTN_SetTerminalPermitEx(core, MC_POS_COMPARE, psoIndex, terminalIndex, 1);
```

Without binding, laser/PSO operations will fail or use wrong outputs.

## Command List + Laser Pattern

Sequential buffered commands for precise position-synchronized laser control:
```c
TListInfo list;
list.list = 1;
list.modal = 1;
list.segNum = 1;

// Build command sequence in buffer
GTN_BufLaserOnEx(core, laser, &list);         // Laser on at current position
GTN_BufDelay(core, &delay, &list);            // Wait
GTN_BufLaserOffEx(core, laser, &list);        // Laser off
GTN_CommandListDataEnd(core, 1);              // Flush

// Queue interpolation path
GTN_CrdLine(crd, &crdPrm, mask, &line1, opt, sense);
GTN_CrdLine(crd, &crdPrm, mask, &line2, opt, sense);

// Start motion + command list simultaneously
GTN_StartCommandList(core, 1);
GTN_CrdStart(crd, mask, opt);
```

## Event-Task Pattern (Interrupt-Driven DO)

GPI triggers GPO without host intervention:
```c
// Clear previous
GTN_ClearEvent(core);
GTN_ClearTask(core);
GTN_ClearEventTaskLink(core);

// Event: GPI 1 == 0 (e.g., sensor triggered)
TEvent event = {0};
event.loop = 0;                          // Infinite
event.var.type = WATCH_VAR_GPI;
event.var.index = 1;
event.condition = WATCH_CONDITION_EQ;
event.value = 0;
short eventId;
GTN_AddEvent(core, &event, &eventId);

// Task: Set GPO 2 = 1
TTaskSetDoBit task = {0};
task.doType = MC_GPO;
task.doIndex = 2;
task.doValue = 1;
task.mode = TASK_SET_DO_BIT_MODE_NONE;
short taskId;
GTN_AddTaskPro(core, &task, &taskId);

// Link
GTN_AddEventTaskLink(core, eventId, taskId);

// Activate
GTN_EventOn(core);

// ... controller handles it in real-time interrupt ...

// Eventually disable
GTN_EventOff(core);
```

## Multi-Core Pattern

Dual-core controllers (HS_CORE + GP_CORE) let you split high-speed and general-purpose tasks:
```c
// HS core: motion control
GTN_LoadConfig(HS_CORE, "hs_config.cfg");
GTN_PrfTrap(HS_CORE, 1);
// ...

// GP core: IO and auxiliary
GTN_LoadConfig(GP_CORE, "gp_config.cfg");
GTN_SetDo(GP_CORE, 1, 1);
// ...
```

EtherCAT variant uses `EC_CORE` instead of `HS_CORE`.

## DMA Pattern (All Subsystems)

DMA prevents buffer underrun for high-speed data. Three subsystems: Interpolation (`GTN_CrdHsOn`), PSO (`GTN_PosCompareHsOn`), Scan (`GTN_ScanHsOn`). Each must use a **unique link ID** when DMA is enabled simultaneously.

```c
// Enable DMA
GTN_PosCompareHsOn(core, pso, 1);  // link=1 for PSO
GTN_CrdHsOn(crd, 2);                // link=2 for interpolation

// Push data faster than consumption
// DMA auto-flushes at threshold (default 200 seg)
while (hasMoreData) {
    GTN_PosCompareData2D(core, pso, x, y);
    // ...
}

// Disable
GTN_PosCompareHsOff(core, pso);
GTN_CrdHsOff(crd);
```

## Common Mistakes

| Mistake | Fix |
|---|---|
| Forgetting `GTN_ClrSts` after `GTN_LoadConfig` | Config silently ignored — **always call `GTN_ClrSts` after `GTN_LoadConfig`** |
| Calling `GTN_OpenCard` more than once | PCIe channel already in use error (-6). Open once at app start |
| Using same link ID for multiple DMA channels | They will conflict. Give each DMA a unique `link` parameter |
| Not checking every API return value | Always check `CMD_SUCCESS`. Silent failures cascade |
| `.cfg` path > 125 characters | Truncate or move config to shorter path |
| Starting motion without `GTN_AxisOn` | Motor won't move. Call `GTN_AxisOn` after `GTN_ClrSts` |
| Using `GTN_Update` bitmask for non-consecutive axes | Use `GTN_UpdatePro` with axis array instead |
| Clearing Events/Tasks while running | Stop detection first with `GTN_EventOff` |
| Assuming axes are 0-indexed | **Axes and cores are 1-indexed** |
| Forgetting `GTN_CommandListDataEnd` | Data may stay in buffer un-flushed. Always call before `StartCommandList` |
| Laser/PSO not working after config | Check `GTN_SetTerminalPermitEx` — hardware channels may not be bound |
| Compensations not showing in profile position | Compensations are added to OUTPUT, not profile. Read encoder for compensated position |
| GPI homing: GoHome returns error | `CAPTURE_PROBE` trigger may not be accepted. Fall back to manual Trap + `GTN_GetTriggerStatusEx` + Stop + ZeroPos |
| GPI homing: captured position wrong | `TTriggerPrm.sense` and `THomePrm.edge` must agree (both rising or both falling) |
| GPI homing: axis stops before DI triggers | Set `searchHomeDistance = 0` for rotary axes, or increase distance on linear axes |

## Debugging Tips

1. **Check return values**: Every function returns `short`. Print the code and look up in `附录/返回值/`
2. **Network init failures**: Check MotionStudio XML — controller/slave type mismatch, physical ID conflicts
3. **Motion doesn't start**: Check axis status bits (alarm=1, limit=5/6, planner bit=10)
4. **Motion starts then stops**: Following error (bit 4), soft limit triggered (bits 5/6), or safe mode active
5. **Config not loading**: Path length > 125 chars, or file not generated by correct MotionStudio version
6. **DMA data loss**: Threshold too high or push rate too slow. Increase PC buffer, decrease threshold
7. **Laser not firing**: Check HSO binding, power mode configuration, and `GTN_LaserOn` call
8. **Controller clock**: `unsigned long clock` increments at 1 ms. Use it to correlate events

## MotionStudio Workflow

The MotionStudio PC tool generates the two critical config files:
1. **Network topology XML** (from scanning) → `GTN_NetInit`
2. **System configuration CFG** (from axis/IO/control mapping) → `GTN_LoadConfig`

Typical workflow: MotionStudio → generate files → copy to app directory → app loads them via `GTN_NetInit` + `GTN_LoadConfig`.

## Homing Patterns (回零)

### Which homing API to choose?

> **官方推荐**: `GTN_ExecuteStandardHome` ★ | `GTN_GoHome` = 不推荐指令（保留用于兼容）

| Scenario | API ★ | Why |
|---|---|---|
| DI 做零点，边沿在 MotionStudio 配置 | `GTN_ExecuteStandardHome` mode 19-22 | 官方推荐，CANopen DS402 标准 |
| DI + Z 信号（高精度旋转轴） | `GTN_ExecuteStandardHome` mode 3-6 | DI→Index，重复精度最高 |
| 仅编码器 Index（无 DI） | `GTN_ExecuteStandardHome` mode 33-34 | 只需 Z 信号 |
| 当前位置直接做零点（不动轴） | `GTN_ExecuteStandardHome` mode 35 | 即时，无运动 |
| 需要在 API 里直接控制边沿极性 | `GTN_GoHome` (不推荐) | `THomePrm.edge` 字段 — 唯一使用场景 |
| GPI 替代 Home DI | `GTN_SetTriggerPrm(CAPTURE_PROBE)` + 手动 Trap | MotionStudio 不支持时才用 |

### 持续旋转轴的回零要点

1. **首选 `GTN_ExecuteStandardHome` mode 3-6**（Home+Index）— 重复定位精度可达编码器一个刻度
2. **次选 mode 19-22**（仅 Home/DI）— 如果没有 Z 信号
3. **`searchHomeDistance = 0`** — Standard Home 自动处理不限距
4. **边沿极性在 MotionStudio 配置** — 常开=reverse 1，常闭=reverse 0；从站 DI 默认低电平有效
5. **"左侧边沿"≠下降沿，"右侧边沿"≠上升沿** — 左/右指传感器在运动方向上的物理位置

### 通用 DI（GPI）替代专用 Home DI（MotionStudio 无法映射时）

当 MotionStudio 无法将通用 DI 配置为 Home 类型（`MC_HOME`）时，通过 API 两步走：

1. **用 `GTN_SetTriggerPrm` 把 Trigger 的捕获源改成 GPI**：`probeType = CAPTURE_PROBE(3)`, `probeIndex = <你的DI通道>`, `sense = 0/1`
2. **手动 Trap 搜索 + 轮询 `GTN_GetTriggerStatusEx`** → 捕获后 `GTN_Stop` + `GTN_ZeroPos`（推荐路径）
3. **备选（不推荐）**：`GTN_GoHome` 设置 `triggerIndex` 指向该 Trigger — 可能不接受 `CAPTURE_PROBE`

三种捕获模式的信号来源：

| `probeType` | 信号来源 | 需要 MotionStudio 配置？ |
|-------------|---------|------------------------|
| `CAPTURE_HOME` (1) | 专用 Home DI | **是** — MotionStudio 里映射 |
| `CAPTURE_INDEX` (2) | 编码器 Z 信号 | 否 |
| `CAPTURE_PROBE` (3) | **通用 GPI（任意 DI）** | **否** — `probeIndex` 直接指定 |
