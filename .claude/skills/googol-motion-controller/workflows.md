# Complete Per-Mode Workflows

Each workflow below is a self-contained function — copy, paste, fill in your parameters, and run. All examples assume the shared init/teardown helpers at the top.

## Shared Helpers

```c
#include "gxn.h"
#pragma comment(lib, "gxn.lib")
#include <stdio.h>
#include <windows.h>

// --- Error handler ---
short Check(const char* fn, short err) {
    if (CMD_SUCCESS != err) printf("FAIL: %s = %d\n", fn, err);
    return err;
}
#define CK(fn, call) do { short _r = (call); if (CMD_SUCCESS != _r) return Check(fn, _r); } while(0)

// --- One-time init (call at app start, not per-move) ---
short SysInit(short core, const char* xml, const char* cfg) {
    long netSts;
    CK("GTN_OpenCard",         GTN_OpenCard(CHANNEL_PCIE, NULL, NULL));
    CK("GTN_NetInit",          GTN_NetInit(NET_INIT_MODE_XML_STRICT, xml, 120, &netSts));
    CK("GTN_LoadConfig",       GTN_LoadConfig(core, cfg));
    return CMD_SUCCESS;
}

// --- Per-axis enable (call once per axis after LoadConfig) ---
short AxisEnable(short core, short axis) {
    CK("GTN_ClrSts",  GTN_ClrSts(core, axis, 1));
    CK("GTN_AxisOn",  GTN_AxisOn(core, axis));
    return CMD_SUCCESS;
}

// --- Wait for planner stop on one axis, with timeout (ms) ---
short WaitStop(short core, short axis, int timeoutMs) {
    long sts; unsigned long clk;
    int elapsed = 0;
    do {
        Sleep(1); elapsed++;
        CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk));
        if (sts & (1<<1 | 1<<4 | 1<<5 | 1<<6 | 1<<8)) {  // alarm, follow-err, limit, emg
            printf("Axis %d fault! sts=0x%08lX\n", axis, sts);
            return -1;
        }
    } while ((sts & 0x400) && elapsed < timeoutMs);
    if (elapsed >= timeoutMs) {
        printf("Axis %d timeout!\n", axis);
        GTN_Stop(core, 1<<(axis-1), 0);
        return -2;
    }
    return CMD_SUCCESS;
}

// --- Wait for multiple axes simultaneously ---
short WaitStopMulti(short core, short* axes, short n, int timeoutMs) {
    long sts[8]; unsigned long clk;
    int elapsed = 0;
    while (elapsed < timeoutMs) {
        Sleep(1); elapsed++;
        int running = 0;
        for (int i = 0; i < n; i++) {
            GTN_GetSts(core, axes[i], &sts[i], 1, &clk);
            if (sts[i] & 0x400) running++;
            if (sts[i] & (1<<1 | 1<<4 | 1<<5 | 1<<6 | 1<<8)) {
                printf("Axis %d fault! sts=0x%08lX\n", axes[i], sts[i]);
                return -1;
            }
        }
        if (!running) return CMD_SUCCESS;
    }
    printf("Timeout! Some axes still running.\n");
    return -2;
}
```

---

## 1. Trap — 梯形/S-Curve 点位运动

```c
short Demo_Trap(short core, short axis) {
    // --- Select profile + set parameters ---
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, axis));

    TTrapPrm prm;
    prm.acc        = 1.0;     // pulse/ms²
    prm.dec        = 1.0;
    prm.velStart   = 0.0;     // start velocity
    prm.smoothTime = 50.0;    // S-curve smoothing (ms), 0 = pure trapezoidal
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, axis, &prm));

    // --- Set target ---
    CK("GTN_SetVel", GTN_SetVel(core, axis, 100.0));   // max velocity (pulse/ms)
    CK("GTN_SetPos", GTN_SetPos(core, axis, 500000));  // target position (pulse)

    // --- Start ---
    CK("GTN_Update",  GTN_Update(core, 1 << (axis - 1)));

    // --- Wait loop: poll position while running ---
    long sts, prfPos; unsigned long clk;
    do {
        CK("GTN_GetSts",    GTN_GetSts(core, axis, &sts, 1, &clk));
        CK("GTN_GetPrfPos", GTN_GetPrfPos(core, axis, &prfPos, 1, &clk));
        printf("\rPos=%ld sts=0x%04lX", prfPos, sts);
    } while (sts & 0x400);

    printf("\nTrap done. Final pos=%ld\n", prfPos);
    return CMD_SUCCESS;
}
```

---

## 2. MoveAbsolute — 单函数绝对运动

```c
short Demo_MoveAbsolute(short core, short axis) {
    TMoveAbsolutePrmEx prm = {0};
    prm.pos              = 200000.0;
    prm.vel              = 80.0;
    prm.acc              = 1.0;
    prm.dec              = 1.0;
    prm.percent          = 0.0;    // S-curve % [0-100]
    prm.velStart         = 0.0;
    prm.velEnd           = 0.0;
    prm.accStartPercent  = 0.0;    // asymmetric acc begin [0-100]
    prm.decEndPercent    = 0.0;    // asymmetric dec end [0-100]

    CK("GTN_MoveAbsoluteEx", GTN_MoveAbsoluteEx(core, axis, &prm));

    // Wait loop
    long sts; unsigned long clk;
    do {
        CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk));
    } while (sts & 0x400);

    printf("MoveAbsolute done.\n");
    return CMD_SUCCESS;
}
```

---

## 3. Jog — 连续速度运动

```c
short Demo_Jog(short core, short axis) {
    CK("GTN_PrfJog", GTN_PrfJog(core, axis));

    TJogPrm jogPrm = {0.5, 0.5};  // acc, dec
    CK("GTN_SetJogPrm", GTN_SetJogPrm(core, axis, &jogPrm));

    // Phase 1: accelerate to speed
    CK("GTN_SetVel",    GTN_SetVel(core, axis, 30.0));
    CK("GTN_UpdatePro", GTN_UpdatePro(core, &axis, 1));

    printf("Jog running at 30 pulse/ms. Hit Enter to change speed...\n");
    getchar();

    // Phase 2: on-the-fly speed change
    CK("GTN_SetVel", GTN_SetVel(core, axis, 60.0));
    printf("Speed changed to 60 pulse/ms. Hit Enter to smooth stop...\n");
    getchar();

    // Phase 3: smooth stop
    CK("GTN_StopPro", GTN_StopPro(core, &axis, 1, 1));  // smooth=1, decel to stop

    // Wait for stop
    long sts; unsigned long clk;
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    printf("Jog stopped.\n");
    return CMD_SUCCESS;
}
```

---

## 4. MoveVelocity — 单函数连续速度

```c
short Demo_MoveVelocity(short core, short axis) {
    TMoveVelocityPrm prm = {0};
    prm.vel = 50.0;
    prm.acc = 1.0;
    prm.dec = 1.0;

    CK("GTN_MoveVelocity", GTN_MoveVelocity(core, axis, &prm));

    // Let it run for 3 seconds
    printf("Running at 50 pulse/ms for 3s...\n");
    long sts; unsigned long clk;
    for (int i = 0; i < 3000; i++) {
        Sleep(1);
        CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk));
        if (!(sts & 0x400)) { printf("Stopped early. sts=0x%04lX\n", sts); break; }
    }

    // Smooth stop
    CK("GTN_Stop", GTN_Stop(core, 1<<(axis-1), 1));
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    printf("MoveVelocity done.\n");
    return CMD_SUCCESS;
}
```

---

## 5. GEAR — 电子齿轮

```c
short Demo_Gear(short core, short masterAxis, short slaveAxis) {
    // Master: simple Trap move
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, masterAxis));
    TTrapPrm masterPrm = {0.1, 0.1, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, masterAxis, &masterPrm));
    CK("GTN_SetVel",     GTN_SetVel(core, masterAxis, 20.0));
    CK("GTN_SetPos",     GTN_SetPos(core, masterAxis, 100000));

    // Slave: gear mode
    CK("GTN_PrfGear",    GTN_PrfGear(core, slaveAxis));
    TGearPrm gearPrm;
    gearPrm.masterAxis   = masterAxis;
    gearPrm.ratio        = 2.0;      // slave = master * 2
    gearPrm.masterSource = 0;        // 0=encoder, 1=profile
    CK("GTN_SetGearPrm", GTN_SetGearPrm(core, slaveAxis, &gearPrm));

    // Start both simultaneously
    CK("GTN_Update", GTN_Update(core, (1<<(masterAxis-1)) | (1<<(slaveAxis-1))));

    // Poll both positions while running
    long stsM, stsS, posM, posS; unsigned long clk;
    do {
        GTN_GetSts(core, masterAxis, &stsM, 1, &clk);
        GTN_GetSts(core, slaveAxis,  &stsS, 1, &clk);
        GTN_GetPrfPos(core, masterAxis, &posM, 1, &clk);
        GTN_GetPrfPos(core, slaveAxis,  &posS, 1, &clk);
        printf("\rMaster=%ld Slave=%ld Ratio=%.2f", posM, posS, (double)posS/posM);
    } while (stsM & 0x400);

    // Slave may still be catching up — wait for it too
    do { GTN_GetSts(core, slaveAxis, &stsS, 1, &clk); }
    while (stsS & 0x400);

    printf("\nGear done.\n");
    return CMD_SUCCESS;
}
```

---

## 6. FOLLOW (Single FIFO) — 电子凸轮

```c
short Demo_FollowSingleFifo(short core, short masterAxis, short slaveAxis) {
    // Build cam table: slavePos[i] at every masterInterval encoder counts
    const int N = 100;
    const double masterInterval = 1000.0;  // every 1000 master encoder counts
    double slaveTable[N];
    for (int i = 0; i < N; i++)
        slaveTable[i] = 500.0 * i;         // linear cam

    CK("GTN_PrfFollowEx", GTN_PrfFollowEx(core, slaveAxis));

    // Load FIFO (must complete before Update)
    CK("GTN_FollowData", GTN_FollowData(core, slaveAxis, masterInterval, slaveTable, N));

    // Master motion
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, masterAxis));
    TTrapPrm prm = {0.1, 0.1, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, masterAxis, &prm));
    CK("GTN_SetVel",     GTN_SetVel(core, masterAxis, 20.0));
    CK("GTN_SetPos",     GTN_SetPos(core, masterAxis, N * masterInterval));

    // Start both
    CK("GTN_UpdatePro", GTN_UpdatePro(core, &slaveAxis, 1));
    CK("GTN_Update",    GTN_Update(core, 1 << (masterAxis - 1)));

    WaitStopMulti(core, (short[]){masterAxis, slaveAxis}, 2, 30000);
    printf("Follow (single FIFO) done.\n");
    return CMD_SUCCESS;
}
```

---

## 7. FOLLOW (Double FIFO / Ping-Pong) — 电子凸轮双缓冲

```c
short Demo_FollowDoubleFifo(short core, short masterAxis, short slaveAxis) {
    const int N = 500;
    const double interval = 1000.0;
    double table[N];
    for (int i = 0; i < N; i++) table[i] = 500.0 * i;

    CK("GTN_PrfFollowEx", GTN_PrfFollowEx(core, slaveAxis));

    // Load BOTH FIFOs before starting
    CK("GTN_FollowData", GTN_FollowData(core, slaveAxis, interval, table, N));
    CK("GTN_FollowData", GTN_FollowData(core, slaveAxis, interval, table, N));

    // Master runs a long move
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, masterAxis));
    TTrapPrm prm = {0.1, 0.1, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, masterAxis, &prm));
    CK("GTN_SetVel",     GTN_SetVel(core, masterAxis, 20.0));
    CK("GTN_SetPos",     GTN_SetPos(core, masterAxis, N * interval * 4));

    CK("GTN_UpdatePro", GTN_UpdatePro(core, &slaveAxis, 1));
    CK("GTN_Update",    GTN_Update(core, 1 << (masterAxis - 1)));

    // Re-fill FIFOs as they drain (ping-pong)
    long sts; unsigned long clk;
    int refillCount = 0;
    do {
        if (refillCount < 6) {  // refill 3 more pairs
            long space;
            GTN_GetFollowSpace(core, slaveAxis, &space);
            if (space > 0) {
                CK("GTN_FollowData", GTN_FollowData(core, slaveAxis, interval, table, N));
                refillCount++;
            }
        }
        GTN_GetSts(core, masterAxis, &sts, 1, &clk);
    } while (sts & 0x400);

    do { GTN_GetSts(core, slaveAxis, &sts, 1, &clk); }
    while (sts & 0x400);

    printf("Follow (double FIFO) done. Refilled %d times.\n", refillCount);
    return CMD_SUCCESS;
}
```

---

## 8. PT — Position-Time 模式

```c
short Demo_PT(short core, short axis) {
    CK("GTN_PrfPt", GTN_PrfPt(core, axis));

    // Push data points: (time_ms, position_pulse, smooth_percent)
    TPtData pts[] = {
        { 0,       0,      0 },
        { 100,  5000,      0 },
        { 200, 10000,     50 },  // 50% smoothing at this node
        { 300, 20000,     50 },
        { 400, 25000,      0 },
        { 500, 30000,      0 },
    };
    int N = sizeof(pts) / sizeof(pts[0]);

    for (int i = 0; i < N; i++)
        CK("GTN_PtData", GTN_PtData(core, axis, &pts[i]));

    CK("GTN_PtStartPro", GTN_PtStartPro(core, &axis, 1));

    // Poll with real-time display
    long sts, pos, seg; unsigned long clk;
    do {
        CK("GTN_GetSts",    GTN_GetSts(core, axis, &sts, 1, &clk));
        CK("GTN_GetPrfPos", GTN_GetPrfPos(core, axis, &pos, 1, &clk));
        printf("\rPT: pos=%ld time=%lu ms", pos, clk);
    } while (sts & 0x400);

    printf("\nPT done.\n");
    return CMD_SUCCESS;
}
```

---

## 9. PVT — Position-Velocity-Time 模式

```c
short Demo_PVT(short core, short axis) {
    CK("GTN_PrfPvt", GTN_PrfPvt(core, axis));

    // Each point: time, position AND velocity at that time
    TPvtData pts[] = {
        {   0,      0,    0 },    // start
        { 100,   3000,   50 },    // accelerating
        { 200,   9000,   70 },    // at speed
        { 300,  16000,   70 },    // constant velocity segment
        { 400,  21000,   30 },    // decelerating
        { 500,  24000,    0 },    // stop
    };
    int N = sizeof(pts) / sizeof(pts[0]);

    for (int i = 0; i < N; i++)
        CK("GTN_PvtData", GTN_PvtData(core, axis, &pts[i]));

    CK("GTN_PvtStartPro", GTN_PvtStartPro(core, &axis, 1));

    long sts, pos, vel; unsigned long clk;
    do {
        GTN_GetSts(core, axis, &sts, 1, &clk);
        GTN_GetPrfPos(core, axis, &pos, 1, &clk);
        GTN_GetPrfVel(core, axis, &vel, 1, &clk);
        printf("\rPVT: pos=%ld vel=%ld", pos, vel);
    } while (sts & 0x400);

    printf("\nPVT done.\n");
    return CMD_SUCCESS;
}
```

---

## 10. 回零 (Homing)

> **官方推荐**: `GTN_ExecuteStandardHome` ★ | **不推荐**: `GTN_GoHome` (保留用于兼容)

### 10a. Standard Home — Home(DI) 边沿回零 ★ 推荐

```c
// ★ 官方推荐方式。边沿极性在 MotionStudio 的 DI reverse 里配置，不需 API 参数
short Demo_StandardHome_DI(short core, short axis) {
    TStandardHomePrm prm = {0};
    prm.mode = 19;                   // Home(DI) 触发或离开 = 零点，正方向
    // 常用模式:
    //  3-6:  Home + Index (最高精度)
    // 19-22: Home only (仅 DI)
    // 23-26: Home + 正限位
    // 27-30: Home + 负限位
    // 33-34: 仅 Index (无 DI)
    //   35: 当前位置 = 零点 (即时)

    CK("GTN_ExecuteStandardHome", GTN_ExecuteStandardHome(core, axis, &prm));

    long sts; unsigned long clk;
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    long homeSts;
    GTN_GetStandardHomeStatus(core, axis, &homeSts);
    printf("Home done. Status=%ld\n", homeSts);

    CK("GTN_ZeroPos",       GTN_ZeroPos(core, axis, 1));
    CK("GTN_SetSoftLimitEx", GTN_SetSoftLimitEx(core, axis, 500000.0, -1000.0));
    CK("GTN_LmtsOnEx",      GTN_LmtsOnEx(core, axis));

    return CMD_SUCCESS;
}
```

### 10b. Standard Home — Home + Index ★ 旋转轴推荐（最高精度）

```c
short Demo_StandardHome_DI_Index(short core, short axis) {
    TStandardHomePrm prm = {0};
    prm.mode = 3;   // 正方向, Home 左侧边沿, 然后反方向搜 Index

    CK("GTN_ExecuteStandardHome", GTN_ExecuteStandardHome(core, axis, &prm));

    long sts; unsigned long clk;
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    CK("GTN_ZeroPos",       GTN_ZeroPos(core, axis, 1));
    CK("GTN_SetSoftLimitEx", GTN_SetSoftLimitEx(core, axis, 500000.0, -1000.0));
    CK("GTN_LmtsOnEx",      GTN_LmtsOnEx(core, axis));

    return CMD_SUCCESS;
}
```

### 10c. Standard Home — 当前位置直接做零点

```c
short Demo_StandardHome_CurrentPos(short core, short axis) {
    TStandardHomePrm prm = {0};
    prm.mode = 35;  // 当前位置 = 零点，不需要运动

    CK("GTN_ExecuteStandardHome", GTN_ExecuteStandardHome(core, axis, &prm));
    CK("GTN_ZeroPos",             GTN_ZeroPos(core, axis, 1));

    return CMD_SUCCESS;
}
```

### 10d. Smart Home (Legacy) — 带显式 edge 控制

```c
// 不推荐用于新项目。仅当需要在 API 中直接控制捕获边沿（而非 MotionStudio 配置）时使用
short Demo_SmartHome_DI(short core, short axis, short risingEdge) {
    THomePrm homePrm = {0};
    homePrm.mode              = HOME_MODE_HOME;          // 20
    homePrm.moveDir           = 1;
    homePrm.edge              = risingEdge ? 1 : 0;      // ★ 唯一优势：API 直接控边沿
    homePrm.triggerIndex      = -1;
    homePrm.velHigh           = 10.0;
    homePrm.acc               = 1.0;
    homePrm.dec               = 1.0;
    homePrm.searchHomeDistance = 0;

    CK("GTN_GoHome", GTN_GoHome(core, axis, &homePrm));

    long sts; unsigned long clk;
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    long homeSts;
    GTN_GetHomeStatus(core, axis, &homeSts);
    printf("Home done. Status=%ld\n", homeSts);

    CK("GTN_ZeroPos",       GTN_ZeroPos(core, axis, 1));
    CK("GTN_SetSoftLimitEx", GTN_SetSoftLimitEx(core, axis, 500000.0, -1000.0));
    CK("GTN_LmtsOnEx",      GTN_LmtsOnEx(core, axis));

    return CMD_SUCCESS;
}
```

### 10e. 限位回零 (Smart Home Legacy)

```c
short Demo_Home_Limit(short core, short axis) {
    THomePrm homePrm = {0};
    homePrm.mode       = HOME_MODE_LIMIT;      // 10
    homePrm.moveDir    = -1;
    homePrm.velHigh    = 20.0;
    homePrm.velLow     = 5.0;
    homePrm.acc        = 1.0;
    homePrm.dec        = 1.0;
    homePrm.escapeStep = 5000;

    CK("GTN_GoHome", GTN_GoHome(core, axis, &homePrm));

    long sts; unsigned long clk;
    do { CK("GTN_GetSts", GTN_GetSts(core, axis, &sts, 1, &clk)); }
    while (sts & 0x400);

    CK("GTN_ZeroPos", GTN_ZeroPos(core, axis, 1));
    return CMD_SUCCESS;
}
```

### 10f. 通用 DI（GPI）作为 Home 捕获源 ★

```c
// MotionStudio 无法将 GPI 映射为 Home 时的方案
// 配置 Trigger → 手动 Trap 搜索 → 硬件捕获 → 停止 → 清零
short Demo_Home_GPI(short core, short axis, short gpiChannel, short risingEdge)
{
    CK("GTN_ClrSts", GTN_ClrSts(core, axis, 1));
    CK("GTN_AxisOn", GTN_AxisOn(core, axis));

    // Step 1: configure Trigger with GPI as capture source
    TTriggerPrm triggerPrm = {0};
    triggerPrm.latchType  = MC_ENCODER;
    triggerPrm.latchIndex = 1;
    triggerPrm.probeType  = CAPTURE_PROBE;             // ★ 3
    triggerPrm.probeIndex = gpiChannel;
    triggerPrm.sense      = risingEdge ? 1 : 0;
    triggerPrm.loop       = 0;
    CK("GTN_SetTriggerPrm", GTN_SetTriggerPrm(core, axis, &triggerPrm));

    // Step 2: Trap move — slow sweep
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, axis));
    TTrapPrm trapPrm = {1.0, 1.0, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, axis, &trapPrm));
    CK("GTN_SetVel",     GTN_SetVel(core, axis, 5.0));
    CK("GTN_SetPos",     GTN_SetPos(core, axis, 99999999));
    CK("GTN_Update",     GTN_Update(core, 1 << (axis - 1)));

    // Step 3: wait for hardware capture
    long sts, capSts = 0; unsigned long clk;
    int timeout = 60000;
    while (!(capSts & 0x1) && timeout > 0) {
        Sleep(1); timeout--;
        GTN_GetSts(core, axis, &sts, 1, &clk);
        GTN_GetTriggerStatusEx(core, axis, &capSts, NULL);
        if (!(sts & 0x400)) break;
    }
    if (timeout <= 0) { printf("GPI capture timeout!\n"); return -1; }

    // Step 4: stop + zero
    GTN_Stop(core, 1 << (axis - 1), 0);
    Sleep(50);

    long latchPos = 0;
    GTN_GetTriggerStatusEx(core, axis, &capSts, &latchPos);
    printf("GPI edge captured at encoder: %ld\n", latchPos);

    CK("GTN_ZeroPos", GTN_ZeroPos(core, axis, 1));
    return CMD_SUCCESS;
}
```

---

## 11. CrdLine — 2D/3D 直线插补 (动态喂数据)

```c
short Demo_CrdLine(short core, short xAxis, short yAxis) {
    short mask = (1 << (xAxis - 1)) | (1 << (yAxis - 1));

    // --- Interpolation channel config ---
    TCrdPrm crdPrm = {0};
    crdPrm.dimension    = 2;
    crdPrm.synVelMax    = 100.0;
    crdPrm.synAccMax    = 1.0;
    crdPrm.evenlyDistribute = 1;     // use even velocity distribution

    // --- Queue first segment ---
    TCrdDataLine line = {0};
    line.x = 50000.0;
    line.y = 50000.0;
    TCrdDataEnd end = {0};
    CK("GTN_CrdLine", GTN_CrdLine(1, &crdPrm, mask, &line, &end, 0));

    // --- Start motion + queue more dynamically ---
    CK("GTN_CrdStart", GTN_CrdStart(1, mask, 0));

    // Dynamic feeding loop: add segments while checking buffer space
    long space, crdSts;
    int segCount = 1;
    double x = 50000, y = 50000;

    while (segCount < 100) {
        // Check buffer space
        CK("GTN_CrdGetSpace", GTN_CrdGetSpace(1, &space));
        CK("GTN_CrdGetStatus", GTN_CrdGetStatus(1, &crdSts));

        if (space > 2) {  // at least 2 slots free to be safe
            x += 1000.0;
            y += (segCount % 2) ? 1000.0 : -1000.0;  // zig-zag
            line.x = x; line.y = y;
            CK("GTN_CrdLine", GTN_CrdLine(1, &crdPrm, mask, &line, &end, 0));
            segCount++;
        } else {
            Sleep(1);
        }

        // If motion finished, break
        if (!(crdSts & 0x01)) break;  // crd running bit
    }

    // Wait for all queued segments to complete
    do {
        CK("GTN_CrdGetStatus", GTN_CrdGetStatus(1, &crdSts));
        Sleep(1);
    } while (crdSts & 0x01);

    // Read final axis positions
    long px, py; unsigned long clk;
    GTN_GetPrfPos(core, xAxis, &px, 1, &clk);
    GTN_GetPrfPos(core, yAxis, &py, 1, &clk);
    printf("\nCrdLine done. %d segments. Final: x=%ld y=%ld\n", segCount, px, py);
    return CMD_SUCCESS;
}
```

---

## 12. CrdArc — 圆弧插补

```c
short Demo_CrdArc(short core, short xAxis, short yAxis) {
    short mask = (1 << (xAxis - 1)) | (1 << (yAxis - 1));

    TCrdPrm crdPrm = {0};
    crdPrm.dimension = 2;
    crdPrm.synVelMax = 50.0;
    crdPrm.synAccMax = 1.0;

    // --- Move to arc start point first ---
    TCrdDataLine line = {0, 0};
    CK("GTN_CrdLine", GTN_CrdLine(1, &crdPrm, mask, &line, NULL, 0));

    // --- Full circle via two 180° arcs (center mode) ---
    TCrdDataArc arc;
    arc.xEnd    = 20000.0;    // end X
    arc.yEnd    = 0.0;        // end Y
    arc.xCenter = 10000.0;    // circle center X
    arc.yCenter = 0.0;        // circle center Y
    arc.dir     = 1;          // 1=CW, -1=CCW
    CK("GTN_ArcXYC", GTN_ArcXYC(1, &crdPrm, mask, &arc, NULL, 0));

    // Second half
    arc.xEnd    = 0.0;
    arc.yEnd    = 0.0;
    arc.xCenter = 10000.0;
    arc.yCenter = 0.0;
    arc.dir     = 1;
    CK("GTN_ArcXYC", GTN_ArcXYC(1, &crdPrm, mask, &arc, NULL, 0));

    // Start
    CK("GTN_CrdStart", GTN_CrdStart(1, mask, 0));

    // Wait
    long crdSts;
    do { CK("GTN_CrdGetStatus", GTN_CrdGetStatus(1, &crdSts)); Sleep(1); }
    while (crdSts & 0x01);

    printf("CrdArc done.\n");
    return CMD_SUCCESS;
}
```

---

## 13. CrdHelix — 螺旋插补

```c
short Demo_CrdHelix(short core, short xAxis, short yAxis, short zAxis) {
    short mask = (1 << (xAxis - 1)) | (1 << (yAxis - 1)) | (1 << (zAxis - 1));

    TCrdPrm crdPrm = {0};
    crdPrm.dimension = 3;
    crdPrm.synVelMax = 50.0;
    crdPrm.synAccMax = 1.0;

    // Queue multiple helical segments
    TCrdDataHelix helix;
    const int turns = 5, segsPerTurn = 36;
    double radius = 10000.0, heightPerTurn = 5000.0;

    for (int t = 0; t < turns; t++) {
        for (int s = 0; s < segsPerTurn; s++) {
            double a0 = (t * segsPerTurn + s)       * 2 * 3.14159265 / segsPerTurn;
            double a1 = (t * segsPerTurn + s + 1)   * 2 * 3.14159265 / segsPerTurn;
            helix.xEnd    = radius * cos(a1);
            helix.yEnd    = radius * sin(a1);
            helix.zEnd    = (t + (s + 1.0) / segsPerTurn) * heightPerTurn;
            helix.xCenter = radius * cos(a0);
            helix.yCenter = radius * sin(a0);
            helix.zCenter = (t + (double)s / segsPerTurn) * heightPerTurn;
            CK("GTN_Helix", GTN_Helix(1, &crdPrm, mask, &helix, NULL, 0));
        }
    }

    CK("GTN_CrdStart", GTN_CrdStart(1, mask, 0));

    long crdSts;
    do {
        CK("GTN_CrdGetStatus", GTN_CrdGetStatus(1, &crdSts));
        long px, py, pz; unsigned long clk;
        GTN_GetPrfPos(core, xAxis, &px, 1, &clk);
        GTN_GetPrfPos(core, yAxis, &py, 1, &clk);
        GTN_GetPrfPos(core, zAxis, &pz, 1, &clk);
        printf("\rHelix: x=%ld y=%ld z=%ld", px, py, pz);
        Sleep(10);
    } while (crdSts & 0x01);

    printf("\nCrdHelix done.\n");
    return CMD_SUCCESS;
}
```

---

## 14. CrdDMA — DMA 插补 (海量数据)

```c
short Demo_CrdDma(short core, short xAxis, short yAxis) {
    short mask = (1 << (xAxis - 1)) | (1 << (yAxis - 1));

    TCrdPrm crdPrm = {0};
    crdPrm.dimension = 2;
    crdPrm.synVelMax = 200.0;
    crdPrm.synAccMax = 5.0;

    // Enable DMA
    CK("GTN_CrdHsOn", GTN_CrdHsOn(1, 1));

    // Push massive data (DMA auto-flushes at threshold=200 segments)
    TCrdDataLine line;
    TCrdDataEnd end = {0};
    const int totalSegs = 10000;
    double x = 0, y = 0;

    for (int i = 0; i < totalSegs; i++) {
        x += 10.0;
        y = 5000.0 * sin(i * 0.01);
        line.x = x; line.y = y;
        CK("GTN_CrdDma", GTN_CrdDma(1, &crdPrm, mask, &line, &end, 0));
    }

    // Start — DMA has already been flushing, just start the channel
    CK("GTN_CrdStart", GTN_CrdStart(1, mask, 0));

    // Monitor while running
    long crdSts, space;
    do {
        GTN_CrdGetStatus(1, &crdSts);
        GTN_CrdGetSpace(1, &space);
        printf("\rDMA: %d/%d segs queued, buffer space=%ld", totalSegs, totalSegs, space);
        Sleep(50);
    } while (crdSts & 0x01);

    CK("GTN_CrdHsOff", GTN_CrdHsOff(1));
    printf("\nCrdDma done.\n");
    return CMD_SUCCESS;
}
```

---

## 15. Laser + Motion — 激光随动

```c
short Demo_LaserWithMotion(short core, short axis, short laser) {
    // --- Bind laser hardware channel ---
    CK("GTN_SetTerminalPermitEx",
       GTN_SetTerminalPermitEx(core, MC_LASER, laser, 0, 1));  // HSO0

    // --- Configure laser: duty-cycle mode ---
    CK("GTN_LaserPowerMode",
       GTN_LaserPowerMode(core, laser, LASER_POWER_MODE_PWM, 0, 100));
    CK("GTN_LaserOutFrq", GTN_LaserOutFrq(core, laser, 20));   // 20 kHz PWM

    // --- Energy follow: power tracks velocity ---
    CK("GTN_LaserFollowMode", GTN_LaserFollowMode(core, laser, 1));
    CK("GTN_LaserFollowRatio", GTN_LaserFollowRatio(core, laser, 0.5));

    // --- Turn laser on ---
    CK("GTN_LaserPrfCmd", GTN_LaserPrfCmd(core, laser, 50));  // 50% base
    CK("GTN_LaserOn",     GTN_LaserOn(core, laser));

    // --- Run motion ---
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, axis));
    TTrapPrm prm = {1.0, 1.0, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, axis, &prm));
    CK("GTN_SetVel",     GTN_SetVel(core, axis, 50.0));
    CK("GTN_SetPos",     GTN_SetPos(core, axis, 200000));
    CK("GTN_Update",     GTN_Update(core, 1 << (axis - 1)));

    // Monitor
    long sts; unsigned long clk;
    long laserSts;
    do {
        CK("GTN_GetSts",         GTN_GetSts(core, axis, &sts, 1, &clk));
        CK("GTN_GetLaserStatus", GTN_GetLaserStatus(core, laser, &laserSts));
        printf("\rMotion+laser: sts=0x%04lX laser=0x%02lX", sts, laserSts);
    } while (sts & 0x400);

    // Laser off
    CK("GTN_LaserOff", GTN_LaserOff(core, laser));
    printf("\nLaser+Motion done.\n");
    return CMD_SUCCESS;
}
```

---

## 16. PSO + Motion — 位置比较输出随动

```c
short Demo_PSOWithMotion(short core, short axis, short pso) {
    // --- Bind PSO hardware channel ---
    CK("GTN_SetTerminalPermitEx",
       GTN_SetTerminalPermitEx(core, MC_POS_COMPARE, pso, 2, 1));  // HSO2

    // --- Configure PSO: 1D linear mode ---
    TPosCompareModeEx modePrm = {0};
    modePrm.mode = 0;  // linear mode
    CK("GTN_SetPosCompareModeEx", GTN_SetPosCompareModeEx(core, pso, &modePrm));
    CK("GTN_SetPosCompareReference", GTN_SetPosCompareReference(core, pso, axis));

    // Output a pulse every 5000 encoder counts, 1000 times
    CK("GTN_SetPosCompareLinear",
       GTN_SetPosCompareLinear(core, pso, 0.0, 5000.0, 1000, 3));

    // --- Start PSO BEFORE motion ---
    CK("GTN_PosCompareStart", GTN_PosCompareStart(core, pso));

    // --- Run motion ---
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, axis));
    TTrapPrm prm = {0.5, 0.5, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, axis, &prm));
    CK("GTN_SetVel",     GTN_SetVel(core, axis, 30.0));
    CK("GTN_SetPos",     GTN_SetPos(core, axis, 5000.0 * 1000));  // cover all pulses
    CK("GTN_Update",     GTN_Update(core, 1 << (axis - 1)));

    // Monitor PSO count + motion
    long sts, psoInfo, psoSts; unsigned long clk;
    do {
        GTN_GetSts(core, axis, &sts, 1, &clk);
        GTN_PosCompareInfo(core, pso, &psoInfo);
        GTN_PosCompareStatus(core, pso, &psoSts);
        printf("\rPSO: fired=%ld/%d sts=0x%04lX", psoInfo, 1000, sts);
    } while (sts & 0x400);

    // Verify: all 1000 pulses fired
    GTN_PosCompareInfo(core, pso, &psoInfo);
    printf("\nPSO done. Pulses fired: %ld\n", psoInfo);

    CK("GTN_PosCompareStop", GTN_PosCompareStop(core, pso));
    return CMD_SUCCESS;
}
```

---

## 17. PSO Wait-for-Arrival — 精密到位后输出

```c
short Demo_PSO_WaitArrival(short core, short axis, short pso) {
    // --- PSO mode 3: wait for encoder confirm before firing ---
    TPosCompareModeEx modePrm = {0};
    modePrm.mode = 3;  // PSO wait-for-arrival
    CK("GTN_SetPosCompareModeEx", GTN_SetPosCompareModeEx(core, pso, &modePrm));
    CK("GTN_SetPosCompareReference", GTN_SetPosCompareReference(core, pso, axis));

    // Set PSO spacing
    CK("GTN_SetPosComparePsoPrm", GTN_SetPosComparePsoPrm(core, pso, 1000.0));
    CK("GTN_PosCompareStart", GTN_PosCompareStart(core, pso));

    // Run motion with loop-back check: PSO guarantees encoder arrival per pulse
    CK("GTN_PrfTrap",    GTN_PrfTrap(core, axis));
    TTrapPrm prm = {0.5, 0.5, 0, 0};
    CK("GTN_SetTrapPrm", GTN_SetTrapPrm(core, axis, &prm));
    CK("GTN_SetVel",     GTN_SetVel(core, axis, 30.0));
    CK("GTN_SetPos",     GTN_SetPos(core, axis, 200000));
    CK("GTN_Update",     GTN_Update(core, 1 << (axis - 1)));

    long sts, psoInfo; unsigned long clk;
    do {
        GTN_GetSts(core, axis, &sts, 1, &clk);
        GTN_PosCompareInfo(core, pso, &psoInfo);
        printf("\rPSO-WFA: fired=%ld", psoInfo);
    } while (sts & 0x400);

    GTN_PosCompareInfo(core, pso, &psoInfo);
    printf("\nPSO Wait-for-Arrival done. Total pulses: %ld\n", psoInfo);
    CK("GTN_PosCompareStop", GTN_PosCompareStop(core, pso));
    return CMD_SUCCESS;
}
```

---

## 18. Event-Task + Motion — 事件触发运动

```c
short Demo_EventTaskMotion(short core, short axis) {
    // --- Clear previous ---
    GTN_ClearEvent(core);
    GTN_ClearTask(core);
    GTN_ClearEventTaskLink(core);

    // Event: GPI 1 goes high (external trigger)
    TEvent event = {0};
    event.loop       = 1;              // fire once
    event.var.type   = WATCH_VAR_GPI;
    event.var.index  = 1;
    event.condition  = WATCH_CONDITION_EQ;
    event.value      = 1;
    short evtId;
    CK("GTN_AddEvent", GTN_AddEvent(core, &event, &evtId));

    // Task: Move axis 10000 pulses
    TTaskStartMotion task = {0};
    task.axis  = axis;
    task.pos   = 10000;
    task.vel   = 50.0;
    task.acc   = 1.0;
    task.dec   = 1.0;
    task.mode  = TASK_START_MOTION_ABSOLUTE;
    short taskId;
    CK("GTN_AddTaskPro", GTN_AddTaskPro(core, &task, &taskId));

    // Link
    CK("GTN_AddEventTaskLink", GTN_AddEventTaskLink(core, evtId, taskId));

    // Enable event detection
    CK("GTN_EventOn", GTN_EventOn(core));

    printf("Waiting for GPI 1 trigger...\n");

    // Poll for event hit + motion completion
    TEventStatus evtSts;
    long sts; unsigned long clk;
    int triggered = 0;
    while (1) {
        GTN_GetEventStatus(core, &evtSts);
        if (evtSts.eventHit && !triggered) {
            printf("Event triggered! Motion starting...\n");
            triggered = 1;
        }
        GTN_GetSts(core, axis, &sts, 1, &clk);
        if (triggered && !(sts & 0x400)) {
            printf("Triggered motion complete.\n");
            break;
        }
        Sleep(1);
    }

    CK("GTN_EventOff", GTN_EventOff(core));
    return CMD_SUCCESS;
}
```

---

## 19. Command List — 批处理指令序列

```c
short Demo_CommandList(short core, short axis1, short axis2, short laser) {
    TListInfo list = {0};
    list.list   = 1;
    list.modal  = 1;
    list.segNum = 1;

    // --- Build command sequence ---
    // Step 1: Move axis1 to position
    TCmdListMoveAbs move1 = {0};
    move1.axis = axis1;
    move1.pos  = 50000;
    move1.vel  = 50.0;
    move1.acc  = 1.0;
    move1.dec  = 1.0;
    CK("GTN_CmdListMoveAbsolute", GTN_CmdListMoveAbsolute(core, &move1, &list));

    // Step 2: Laser on
    CK("GTN_BufLaserOnEx", GTN_BufLaserOnEx(core, laser, &list));

    // Step 3: Move axis2 + laser on simultaneously
    TCmdListMoveAbs move2 = {0};
    move2.axis = axis2;
    move2.pos  = 30000;
    move2.vel  = 30.0;
    move2.acc  = 1.0;
    move2.dec  = 1.0;
    CK("GTN_CmdListMoveAbsolute", GTN_CmdListMoveAbsolute(core, &move2, &list));

    // Step 4: Delay 500ms
    TDelay delay = {0};
    delay.time = 500;
    CK("GTN_SetDelay", GTN_SetDelay(core, &delay, &list));

    // Step 5: Laser off
    CK("GTN_BufLaserOffEx", GTN_BufLaserOffEx(core, laser, &list));

    // Step 6: Axis1 back to 0
    move1.pos = 0;
    CK("GTN_CmdListMoveAbsolute", GTN_CmdListMoveAbsolute(core, &move1, &list));

    // --- End push + start ---
    CK("GTN_CommandListDataEnd", GTN_CommandListDataEnd(core, 1));
    CK("GTN_StartCommandList",  GTN_StartCommandList(core, 1));

    // --- Wait loop: monitor list status + axis status ---
    long listSts, sts1, sts2, space; unsigned long clk;
    do {
        GTN_GetCommandListStatus(core, 1, &listSts);
        GTN_GetCommandListSpace(core, 1, &space);
        GTN_GetSts(core, axis1, &sts1, 1, &clk);
        GTN_GetSts(core, axis2, &sts2, 1, &clk);
        printf("\rList: sts=0x%04lX space=%ld  Axis1=0x%04lX Axis2=0x%04lX",
               listSts, space, sts1, sts2);
        Sleep(10);
    } while (listSts & 0x01);  // list executing bit

    // Check for errors
    if (listSts & 0x02) {
        printf("\nCommand list underrun or error! sts=0x%04lX\n", listSts);
    } else {
        printf("\nCommand list complete.\n");
    }
    return CMD_SUCCESS;
}
```

---

## 20. Full Application Template

```c
int main() {
    short core = 1;

    // --- One-time system init ---
    short rtn = SysInit(core, "network.xml", "config.cfg");
    if (CMD_SUCCESS != rtn) { printf("Init failed: %d\n", rtn); return rtn; }

    // --- Enable axes ---
    AxisEnable(core, 1);
    AxisEnable(core, 2);

    // --- Run any workflow ---
    Demo_Trap(core, 1);               // simple PT move
    Demo_MoveAbsolute(core, 2);       // one-call move
    Demo_Jog(core, 1);                // continuous jog
    Demo_Gear(core, 1, 2);            // gearing
    // ... any other demo ...

    // --- Cleanup ---
    GTN_AxisOff(core, 1);
    GTN_AxisOff(core, 2);
    GTN_Close();

    printf("All demos complete.\n");
    return 0;
}
```

## Quick Reference: Which Loop to Use

| Motion Type | Wait Condition | Loop Pattern |
|---|---|---|
| Trap / MoveAbsolute / PT / PVT | `sts & 0x400` | Poll `GTN_GetSts` until bit 10 clears |
| Jog / MoveVelocity | `sts & 0x400` | Same, but stop via `GTN_Stop` before waiting |
| Gear / Follow | Master + Slave both done | Poll both `GTN_GetSts`; slave may lag |
| Interpolation (Crd*) | `crdSts & 0x01` | Poll `GTN_CrdGetStatus`; also check buffer space |
| DMA (CrdDma) | `crdSts & 0x01` | Poll `GTN_CrdGetStatus`; DMA auto-flushes |
| Homing (Smart Home) | `sts & 0x400` | Poll `GTN_GetSts`; then check `GTN_GetHomeStatus` |
| Homing (GPI as Home) | `sts & 0x400` or manual | ★ 10d: `GTN_SetTriggerPrm` + `GTN_GoHome`. 10e: manual Trap + `GTN_GetTriggerStatusEx` |
| PSO | Axis `sts & 0x400` | PSO runs alongside motion; poll `GTN_PosCompareInfo` for count |
| Command List | `listSts & 0x01` | Poll `GTN_GetCommandListStatus`; check axis status too |
| Event-Task | `evtSts.eventHit` | Poll `GTN_GetEventStatus` + `GTN_GetSts` for triggered motion |
