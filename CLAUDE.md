# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# chipSetter — 固晶机控制系统

Qt 5.15.2 C++ 上位机，运行于固高 GNC-C610 工控机 (Win10, 192.168.1.2)。

## 最重要的事

1. **双模式编译**：Mock（本地无硬件）和 Real GNC（工控机真实硬件），通过 `CONFIG+=real_gnc` 切换
2. **IO信号来源**：Real模式通过 GTS SDK (`GT_GetDi(MC_GPI=4)`) 读 GPI 寄存器；Mock 模式用 `MockGncController` 返回假数据
3. **不要直接 include gts.h**：只有 `GncController.h`（在 Real 模式下）才 include，其他文件通过 `IGncController` 接口访问硬件
4. **日志位置**：工作目录下的 `chipsetter_debug.log`

## 硬件架构

```
UI (widgets/) ──Signal/Slot──> Core (core/) ──IGncController──> GTS SDK (googol/gts.dll)
                                                                    └── GNC-C610 运动控制器
                                                                        └── 19路DI (X0-X18, GPI1-19)
                                                                        └── 4路DO (Y9-Y12, GPO9-12)
                                                                        └── 13轴 (胶盘, 点胶, 取晶...)
```

### IO 信号映射 (HardwareConfig.h)

| GPI | 端口 | 信号名称 | 用途 |
|-----|------|----------|------|
| 1 | X0 | 点胶平台X轴正限位 | axis2 limit+ |
| 2 | X1 | 点胶平台X轴负限位 | axis2 limit- |
| 3 | X2 | 点胶平台X轴HOME | axis2 home |
| 4 | X3 | 点胶平台Y轴正限位 | axis3 limit+ |
| 5 | X4 | 点胶平台Y轴负限位 | axis3 limit- |
| 6 | X5 | 点胶平台Y轴HOME | axis3 home |
| 7 | X6 | 取晶X轴正限位 | axis10 limit+ |
| 8 | X7 | 取晶X轴负限位 | axis10 limit- |
| 9 | X8 | 取晶X轴HOME | axis10 home |
| 10 | X9 | 取晶Y轴负限位 | axis11 limit- |
| 11 | X10 | 取晶Y轴正限位 | axis11 limit+ |
| 12 | X11 | 取晶臂旋转 | axis4 状态 |
| 13 | X12 | 点胶臂旋转 | axis5 状态 |
| 14 | X13 | 取晶平台旋转 | axis12 状态 |
| 15 | X14 | 吸嘴HOME | axis13 home |
| 16 | X15 | 顶针1 | axis8 状态 |
| 17 | X16 | 顶针2 | axis9 状态 |
| 18 | X17 | 点胶上下气缸到位 | axis6 状态 |
| 19 | X18 | 取晶臂上下 | axis7 状态 |

| GPO | 端口 | 信号名称 |
|-----|------|----------|
| 9 | Y9 | 红灯 |
| 10 | Y10 | 黄灯 |
| 11 | Y11 | 绿灯 |
| 12 | Y12 | 蜂鸣器 |

## 编译

### 统一调度脚本 (推荐)

```bash
# 所有操作用一个脚本搞定:
powershell -File scripts/workflow.ps1 <命令> [选项]

# 命令:
#   test     实机测试全流程: Real GNC编译 → 打包 → 部署 → 远端启动 (默认)
#   debug    F5调试: 同上
#   quick    跳过编译: 打包 → 部署 → 启动 (已编译过时用)
#   build    Mock 编译
#   build-real Real GNC 编译
#   package  仅打包
#   deploy   仅部署
#   start    仅远端启动
#   stop     仅远端停止
#   full     Mock 全流程 (无部署)
```

### Mock 模式（本地开发）
```bash
powershell -File scripts/workflow.ps1 build
# 输出: debug/chipSetter.exe (无硬件依赖)
```

### Real GNC 模式（部署到工控机）
```bash
powershell -File scripts/workflow.ps1 test
# 完整链路: 编译 → 打包 → 部署 → 启动
```

### 测试
```bash
cd tests && qmake && make && ./debug/test_hardware_config.exe
```
3 个测试：test_hardware_config, test_motor_axis, test_process_manager

## 关键文件

| 文件 | 职责 |
|------|------|
| `chipSetter.pro` | qmake 工程，Mock/Real 互斥编译控制 |
| `core/HardwareConfig.h` | 常量定义：AXIS_COUNT=13, DI_COUNT=19, DO_COUNT=4, DO_INDEX_BASE=9 |
| `core/GncController.h` | **硬件隔离层**：IGncController 接口 + (Mock模式下) MockGncController；Real 模式下 include gts.h |
| `core/GncControllerImpl.h/.cpp` | **真实 GTS SDK 实现**（仅 Real 模式编译），调用 GT_GetDi/GT_SetDoBit/GT_Open 等 |
| `core/IoManager.h/.cpp` | IO 管理器：50ms 轮询 DI/DO，变化检测，发出 diChanged/doChanged 信号 |
| `core/MotorManager.h/.cpp` | 13 轴状态管理，运动控制 |
| `core/ProcessManager.h/.cpp` | 9 步工艺状态机 |
| `mainwindow.h/.cpp` | 主窗口：#ifdef USE_REAL_GNC 选择创建 GncControllerImpl 或 MockGncController |
| `main.cpp` | 入口：文件日志初始化，构建模式输出 |
| `googol/` | GTS SDK 文件：gts.h, gts.lib, gts.dll, gt_rn.dll, core1_20261212.cfg |
| `scripts/build_debug.bat` | Mock 一键编译 |
| `scripts/build_deploy.bat` | Real GNC 一键编译 |
| `tests/` | 3 个单元测试 |

## IO 信号流（Real 模式）

```
IoManager::onPollTimer()  [每50ms]
  └→ GncControllerImpl::readDI(core=1, diType=MC_GPI=4, diIndex=1, values, count=19)
       └→ GT_GetDi(4, &bitmask)          // GTS SDK: 读 GPI 32位掩码
            └→ values[i] = (bitmask >> i) & 1   // 逐位提取
  └→ IoManager::detectChanges()
       └→ emit diChanged(id, value)      // 信号通知 UI
            └→ IoMonitorWidget::onDiChanged(id, value)   // 调试面板
            └→ MainWindow lambda: DI→轴限位映射            // 生产面板
```

## UI 交互架构

### 窗口布局 (1024×710 + 标题栏 ≈ 768)

```
┌────────────────────────────────────────────────────────────┐
│ StatusBarWidget (26px, 双模式共享)                          │
│  固晶机控制系统 V1.0 | [模式灯(可点击)] | 产量 | 节拍 | 时长 │
├────────────────────────────────────────────────────────────┤
│ QStackedWidget                                              │
│ ┌[0] 生产模式 ─────────────────────────────────────────┐   │
│ │ ProductionWidget                                      │   │
│ │  ┌─ 工艺流程 ────────────────────────────────────┐   │   │
│ │  │ FlowStepBar: 9步按钮 + 循环框 + 回路箭头       │   │   │
│ │  │ ①(外) → [②→③→④→⑤→⑥→⑦→⑧](绿框) → ⑨(外)     │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │  ┌─ 步骤详情 (点击流程块切换) ───────────────────┐   │   │
│ │  │ StepDetailPanel: 子步骤 | 参数(可编辑) | 实时   │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │  ┌─ 设备状态 ────────────────────────────────────┐   │   │
│ │  │ DeviceStatusWidget: 13轴灯 ● + 急停 + GNC连接  │   │   │
│ │  └───────────────────────────────────────────────┘   │   │
│ │                                      ┌─ 报警列表 ─┐   │   │
│ │                                      │ 右栏        │   │   │
│ └──────────────────────────────────────┴─────────────┘   │   │
│ ┌[1] 调试模式 ─────────────────────────────────────────┐   │   │
│ │ 左(420px): MotorPtpWidget + MotorParamWidget          │   │
│ │ 中(stretch): IoMonitorWidget + StatsWidget            │   │
│ │ 右(220px): AlarmListWidget + 视觉预留                  │   │
│ └───────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────┤
│ BottomBarWidget (34px, 双模式共享)                          │
│  ▶启动 | ⏸暂停 | 收尾 | 立即收尾 | ↺复位 | [急停] | ⇄切换 │
└────────────────────────────────────────────────────────────┘
```

### 模式切换

| 入口 | 机制 |
|------|------|
| 底部操作栏 `⇄ 调试/生产` 按钮 | `BottomBarWidget::modeSwitchClicked` → QStackedWidget::setCurrentIndex |
| 状态栏模式灯(可点击) | `StatusBarWidget::modeClicked` → 同上（注意：已改为 QPushButton，不是 QLabel） |
| 生产界面内 "调试" 按钮 | `ProductionWidget::switchToDebugMode` → 切换到 index 1 |

两种模式共享 StatusBar + BottomBar + 底层 Core 数据，仅 QStackedWidget 内容切换。

### 生产模式 — 9 步工艺流程

```
①初始化 → ┌[循环框: ②上料→③定位→④顶晶→⑤拾取→⑥点胶→⑦贴装→⑧下料]──(⤴回路箭头)──┐ → ⑨收尾
          └ 绿色边框 #2e7d32, 背景 #0d1a0d ───────────────────────────────────────┘
```

**FlowStepBar 视觉规范:**
- 9 个 QPushButton，固定 72×60px，clickable
- 步骤 1-7 包裹在绿色 QFrame (`#LoopFrame`) 内，框内底部有 ⤴ 回路箭头(黄色 `#ffd740`)
- 步骤 0(初始化) 和 8(收尾) 在循环框**外部**
- 步骤按钮颜色状态:
  - PENDING: `bg:#2a2a3a, fg:#666, border:#333`
  - RUNNING: `bg:#1565c0, fg:#fff, border:#64b5f6` (bold + glow)
  - COMPLETED: `bg:#1b5e20, fg:#81c784, border:#388e3c`
  - FAILED: `bg:#b71c1c, fg:#ef9a9a, border:#f44336`

**StepDetailPanel 三栏:**
| 栏 | 内容 | 控件类型 | 是否可编辑 |
|----|------|----------|-----------|
| 左 | 子步骤列表 (○/◉/✓/✕ + 名称) | QLabel | 否 |
| 中 | 步骤参数 (名值对) | QLabel(名) + **QLineEdit(值)** | **是** |
| 右 | 实时状态 (位置/压力/耗时等) | QLabel(名+值) | 否 |

- 参数值编辑完成触发 `paramEdited(stepIndex, name, value)` → ProcessManager 更新
- 存储变量名为 `m_paramValues` (QPair<QLabel*, QLineEdit*>)，不是老的 `m_paramLabels`

**DeviceStatusWidget:**
- 13 个轴指示灯 ● (灰色未使能/绿色就绪/蓝色运动中/红色报警)
- 急停状态标签 + IO 活跃信号摘要(最多3个) + GNC 连接状态

### 底部操作栏 6 按钮

| 按钮 | 样式 | 触发的 ProcessManager 方法 | 说明 |
|------|------|--------------------------|------|
| ▶ 启动 | 绿底 | `startCycle()` | 重置全部，从①初始化开始 |
| ⏸ 暂停 | 灰色 | `pauseCycle()` | 完成当前步骤后停止 |
| 收尾 | 橙色 | `requestFinish()` | **出循环关机**: 设置 pendingFinish 标志，当前循环跑完后自动进⑨ |
| 立即收尾 | 深橙 | `finishCycle()` | **立刻跳**⑨收尾，不等循环结束 |
| ↺ 复位 | 浅灰 | `emergencyStop()` + clearAlarms + resetStats | 全部重置 |
| ⏻ 急停 | 红色(右侧独立) | `emergencyStop()` + 报警 | 立刻全部重置灰色 |

### ProcessManager 行为细节

| 方法 | 行为 | 步骤复位 | 子步骤复位 |
|------|------|---------|-----------|
| startCycle() | m_currentStepIdx<0 时重置全部步骤，从①开始 | 全部 PENDING | 全部 PENDING |
| pauseCycle() | 停止定时器，m_paused=true | — | — |
| resumeCycle() | m_currentStepIdx>=0 时重新启动定时器；<0 时从循环起点开始 | — | — |
| finishCycle() | 立刻跳⑨收尾，完成后全部重置 | 全部 PENDING | **必须同步重置** |
| requestFinish() | 设 m_pendingFinish=true | (循环终点触发) | — |
| emergencyStop() | 定时器停，全部重置，m_currentStepIdx=-1 | 全部 PENDING | ⚠️ **未重置子步骤** (仅设 m_substepProgress=0) |

**循环逻辑 (completeCurrentStep 中):**
- ⑧下料完成 → 如 m_pendingFinish=true → 进⑨收尾
- ⑧下料完成 → 如 m_paused=true → 停止，m_currentStepIdx=-1
- ⑧下料完成 → 正常 → 循环步骤 ②~⑧ 重置 PENDING（含子步骤！），m_cycleCount++，回到②
- ⑨收尾完成 → 全部 9 步重置 PENDING（含子步骤！），m_running=false，emit allFinished()

### 核心信号链路 (MainWindow::connectSignals)

```
BottomBar 按钮 → ProcessManager 控制槽
  startClicked → startCycle()
  pauseClicked → pauseCycle()
  softFinishClicked → requestFinish()
  immediateFinishClicked → finishCycle()
  resetClicked → emergencyStop() + clearAll()
  emergencyStopClicked → emergencyStop() + alarm

ProcessManager 状态 → UI 更新
  stepStateChanged(int,int) → FlowStepBar::setStepState(int,int)
  currentStepChanged(int) → FlowStepBar::setCurrentStep(int)
  substepStateChanged(int,int,int) → StepDetailPanel::updateSubstepState(int,int,int)
  realtimeDataUpdated(int,QVariantMap) → lambda → StepDetailPanel::updateRealtimeData
  cycleCompleted(int) → lambda → StatsCollector::incrementCount()

FlowStepBar 用户交互
  stepClicked(int) → lambda(查询 ProcessManager) → StepDetailPanel::showStepDetail(...)

StepDetailPanel 参数编辑
  paramEdited(int,QString,double) → ProcessManager 更新参数

StatusBar 模式切换
  modeClicked() → QStackedWidget 切换 → switchToProductionMode/DebugMode
```

### UI 组件注意事项 (易错点)

1. **StatusBarWidget 的 modeLabel 已改为 QPushButton**：原来是 QLabel，现为 QPushButton，点击发射 `modeClicked()`。`setMode()` 需同时设置按钮文字和 stylesheet。
2. **StepDetailPanel 参数可编辑**：参数值为 QLineEdit，不要当成 QLabel 来用。清理时用 `m_paramValues` (QLabel*, QLineEdit* 对)，不是老的 `m_paramLabels`。
3. **子步骤状态必须同步重置**：循环重置和收尾重置时，除了 `m_stepStates[i] = PENDING`，还必须循环重置 `m_substepStates[i][j] = PENDING`。
4. **ProcessManager 信号用 int 不是 StepState 枚举**：避免 Widget 层依赖 Core 枚举类型。发射时用 `static_cast<int>(state)`。
5. **FlowStepBar::setSteps 可被多次调用**：ProductionWidget 构造和 MainWindow::initSystem 各调用一次，内部会 deleteLater 旧按钮并重建布局。
6. **窗口 resize(1024, 710)**：终端用户看到的实际窗口 1024×768，客户端 1024×710（标题栏占 ~58px）。
7. **BottomBar 的 setMode(0) 对应调试、setMode(1) 对应生产**：模式按钮文字是"接下来切到什么模式"，不是"当前模式"。
8. **颜色常量集中**：灰色 PENDING #2a2a3a/#666，蓝色 RUNNING #1565c0，绿色 COMPLETED #1b5e20，红色 FAILED #b71c1c，循环框 #0d1a0d/#2e7d32，回路箭头 #ffd740。

## 开发注意事项

### 添加新 IO 信号
1. 修改 `core/HardwareConfig.h`：更新 DI_COUNT/DO_COUNT，添加宏定义，更新 DI_NAMES/DI_AXIS_MAP
2. 如改变数量，更新 `core/GncController.cpp`（Mock 数组大小）
3. 如改变 DI 网格布局，更新 `widgets/IoMonitorWidget.cpp`

### MC_GPI/MC_GPO 的正确值
- **Real 模式**: MC_GPI=4, MC_GPO=12 (来自 gts.h)
- **Mock 模式**: MC_GPI=0, MC_GPO=1 (GncController.h 中定义)
- 两种模式互斥编译，值不会冲突

### 调试
- 所有 qDebug() 输出写入 `chipsetter_debug.log`
- 启动时的构建模式、日志路径、MC_GPI 值都会记录
- 前 3 次 IO 轮询结果详细记录（包括 bitmask 和提取的 values）
- GTS SDK 调用错误记录在 qWarning 中

### 已知问题
- `test_io_alarm_models` 测试缺失（.pro 存在但 .cpp 不存在），已从 tests.pro 移除
- Qt 5.15.2 存在 QButtonGroup::buttonClicked 弃用警告（预存问题）
- GncControllerImpl 的软限位设置暂未实现运行时修改（需通过 GT_SetMcConfig）
- TStandardHomePrm/TStandardHomeStatus 在 Real/Mock 模式下布局不同（各自独立编译，无 ABI 问题）
- **emergencyStop() 未重置子步骤状态**：仅设 `m_substepProgress=0`，但 `m_substepStates` 数组保留旧值。`startCycle()` 重新初始化时会覆盖，但急停后如果不走 startCycle 直接查询子步骤会看到过期数据。

### 远程调试 (VS Code)

详细踩坑记录见 `docs/remote-debug-guide.md`（15条）。以下为必知摘要：

**架构**: 开发机(192.168.1.100) → GDB(MSYS2 32-bit) → TCP :1234 → GNC(192.168.1.2) gdbserver → chipSetter.exe
**部署**: robocopy via SMB `\\192.168.1.2\share` → GNC 本地 `C:\Users\googol\Desktop\share\chipSetter\`
**远程管理**: WinRM (5985-5986) + schtasks /it (投递交互会话, 解决 Session 0 无 GUI)

---

### .vscode 工具链全景 (6个文件)

#### 文件清单

| 文件 | 职责 |
|------|------|
| `launch.json` | 3 种调试启动配置 (远程F5/远程仅连接/本地) |
| `tasks.json` | 8 个 Task, 含复合链路 `编译并部署 Debug 到目标机` |
| `package_debug.ps1` | windeployqt + googol/gts.dll + gdbserver + MinGW DLL → `debug_deploy/` |
| `deploy_to_target.ps1` | net use SMB 预建连接 → robocopy /E → GNC `\\192.168.1.2\share\chipSetter` |
| `start_gdbserver.ps1` | WinRM → PsExec → 手动 三级降级, 远端启动 gdbserver `--once 0.0.0.0:1234` |
| `stop_gdbserver.ps1` | WinRM 远端杀 chipSetter + gdbserver 进程, 删除计划任务 |

#### launch.json: 3 种调试配置

| 配置名 | 用途 | preLaunchTask | postDebugTask |
|--------|------|---------------|---------------|
| **远程调试 (GNC工控机)** | F5 一键: 编译→打包→部署→启动gdbserver→连接 | `编译并部署 Debug 到目标机` | `远端结束 gdbserver` |
| **远程调试 (仅连接,不编译)** | 仅连已有 gdbserver, 跳过编译部署 | `远端启动 gdbserver` | `远端结束 gdbserver` |
| **本地调试 (GDB)** | Mock 模式本地调试, 无硬件 | `编译 Debug 版本` | 无 |

**gdb setupCommands (防踩坑):**
- `set sysroot` — 清空 sysroot, 阻止 GDB 16.2 从远程读 exe 导致栈溢出
- `set auto-solib-add off` — 禁止自动加载远程 DLL

#### tasks.json: Task 依赖链

```
编译并部署 Debug 到目标机 (复合入口)
  ├── ① 远端结束 gdbserver    → stop_gdbserver.ps1 (WinRM)
  ├── ② 编译 Debug 版本
  │       └── make Debug (MSYS2)  → bash -lc "make -f Makefile.Debug -j4"
  │            (env: MSYSTEM=MINGW32  ← 关键! 写在env块不是命令里)
  ├── ③ 打包 Debug 部署包       → package_debug.ps1
  ├── ④ 部署到目标机            → deploy_to_target.ps1
  └── ⑤ 远端启动 gdbserver     → start_gdbserver.ps1
```

**独立 Task:**

| Task 名 | 类型 | 说明 |
|---------|------|------|
| `qmake Debug` | qmake | Mock 模式 qmake (无 `CONFIG+=real_gnc`) |
| `make Debug (MSYS2)` | bash | 增量编译, env 中设 `MSYSTEM=MINGW32` |
| `编译 Debug 版本` | 复合 | 仅编译 (依赖 `make Debug (MSYS2)`) |
| `打包 Debug 部署包` | powershell | package_debug.ps1 |
| `部署到目标机` | powershell | deploy_to_target.ps1 |
| `远端结束 gdbserver` | powershell | stop_gdbserver.ps1 |
| `远端启动 gdbserver` | powershell | start_gdbserver.ps1 |

**注意**: Task `qmake Debug` 编译的是 Mock 模式。如需 Real GNC 模式, 需手动运行 `qmake CONFIG+=real_gnc` 或用 `scripts/build_deploy.bat`。

#### package_debug.ps1: 打包流程

```
① 检查 debug/chipSetter.exe 存在
② 清空并重建 debug_deploy/
③ 复制 debug/chipSetter.exe
④ windeployqt --no-translations --no-compiler-runtime (Qt DLL)
⑤ 复制 gdbserver.exe + libgcc_s_dw2-1.dll + libstdc++-6.dll + libwinpthread-1.dll
⑥ 复制 googol/ (gts.dll, gts.lib, gt_rn.dll, config.h, core1_20261212.cfg)
⑦ 复制 Qt plugins/ (platforms/qwindows.dll 等)
输出: debug_deploy/ (约 90 文件, 118MB)
```

#### deploy_to_target.ps1: 部署机制

```
① 从 ~\.chipsetter_cred.xml 读取加密凭证
② net use \\192.168.1.2\share /user:googol <password> (预建 SMB, 解决 1326 错误)
③ robocopy debug_deploy/ → \\192.168.1.2\share\chipSetter /E /R:3 /W:5
```

- 目标路径: `C:\Users\googol\Desktop\share\chipSetter\` (GNC 本地)
- robocopy 返回值: 0-7 正常, ≥8 失败

#### start_gdbserver.ps1: 三级降级启动

```
Method 1 (WinRM): New-PSSession → Invoke-Command 远端创建 .bat → schtasks /create /it 投递交互会话
Method 2 (PsExec): PsExec \\192.168.1.2 -s -d cmd /c "gdbserver.exe --once 0.0.0.0:1234 chipSetter.exe"
Method 3 (手动):   打印说明让用户 RDP 到 GNC 手动运行
```

**关键参数**: `gdbserver.exe --once 0.0.0.0:1234 chipSetter.exe`
- `--once`: gdb 断开后自动退出 (配合 postDebugTask 清理)
- `0.0.0.0:1234`: 必须绑定 IPv4 全地址, `:1234` 只绑 IPv6

#### stop_gdbserver.ps1: 远端清理

```
WinRM → Invoke-Command:
  Stop-Process chipSetter -Force
  Stop-Process gdbserver -Force
  schtasks /delete /tn "chipSetter-gdbserver" /f
```

---

#### F5 一键调试链路

```
①远端结束 gdbserver (杀旧进程, 解锁exe) → ②make -f Makefile.Debug -j4 (增量编译)
→ ③打包 (windeployqt + googol/gts.dll + gdbserver + MinGW DLL)
→ ④部署 (net use SMB → robocopy /E) → ⑤schtasks /it 远端启动 gdbserver
→ gdb 连接 → Shift+F5 (postDebugTask 自动远端杀进程)
```

#### 10 条铁律 (踩坑教训)

1. **gdb 配 `set sysroot`** — GDB 16.2 从远程读 exe 会栈溢出, 清空 sysroot 阻止
2. **不配 `sourceFileMap`** — debug info 已是 `D:\...` 格式, 映射反而导致断点 PENDING
3. **`stopAtEntry: false`** — 用 VS Code UI 断点, 避免停在 CRT 入口
4. **`cwd` 填本地路径** — 这是给本地 gdb 的, 不是远程的
5. **gdbserver 必须 `0.0.0.0:1234`** — `:1234` 只绑 IPv6, IPv4 连不上
6. **gdbserver 加 `--once`** — gdb 断开自动退出, 配合 postDebugTask 清理
7. **bash task 的 `MSYSTEM=MINGW32` 写在 `env` 块, 不写在命令里** — 登录 shell 的 /etc/profile 先于 -c 执行, 命令里 export 晚了 → 64位 g++ 编 32位 Qt → 链接静默失败 → exe 永不更新!
8. **qmake 不自动跑** — 每次跑重置 Makefile 时间戳, 破坏增量编译。**只有加新文件/改 .pro 才手动 qmake**
9. **package_debug.ps1 要跑 windeployqt + 复制 googol/** — 否则远程缺 Qt5*.dll 和 gts.dll
10. **deploy_to_target.ps1 用 `net use` 预建 SMB 连接** — VS Code 任务进程没有 GUI 凭证缓存, robocopy 报 1326

#### 日常操作

| 场景 | 操作 |
|------|------|
| 改代码调试 | 直接 F5 (增量编译自动部署, 默认 Mock 模式) |
| 实机测试 | `powershell -File scripts/workflow.ps1 test` (全链路) |
| 快速部署 | `powershell -File scripts/workflow.ps1 quick` (已编译过) |
| 仅远端启动 | `powershell -File scripts/workflow.ps1 start` |
| 仅远端停止 | `powershell -File scripts/workflow.ps1 stop` |
| 新增 .cpp/.h | 先跑 `qmake Debug` task 再编译 |
| 改 .pro | 先跑 `qmake Debug` task 再编译 |
| GNC 端 | 需保持登录 (RDP/本地), 否则 schtasks /it 无交互会话可投递 |

#### 脚本速查

| 脚本 | 用途 | 关键参数 |
|------|------|----------|
| `scripts/workflow.ps1` | **统一调度** (编译→打包→部署→启动) | `test`/`quick`/`build`/`start`/`stop` |
| `scripts/build_debug.bat` | Mock 编译 (MSYS2 终端内运行) | `clean` 清旧文件 |
| `scripts/build_deploy.bat` | Real GNC 编译 (MSYS2 终端内运行) | `clean` |
| `.vscode/package_debug.ps1` | windeployqt + googol/ + MinGW DLL → debug_deploy | 无参数, 全自动 |
| `.vscode/deploy_to_target.ps1` | net use SMB → robocopy /E → GNC | `-TargetShare` |
| `.vscode/start_gdbserver.ps1` | WinRM → PsExec → 手动 三级降级 | `-TargetIp -Port` |
| `.vscode/stop_gdbserver.ps1` | WinRM 远端杀 gdbserver + chipSetter | 需要凭证文件 |

凭证: `~\.chipsetter_cred.xml` (Export-Clixml 加密, 仅本机本用户可解密)
GNC 用户: `googol`, 需在 `Remote Management Users` 组
防火墙: TCP 1234 + 5985-5986 已放行

#### gdbserver 挂起陷阱

**问题**: gdbserver 启动目标进程后会将其**挂起**，等待 gdb 连接后发送 `continue`。如果不连 gdb，进程永远停在入口点——`MainWindowHandle=0`，看不到 GUI。

**区分**:
- **F5 调试**: 用 `start_gdbserver.ps1` → gdbserver 挂起进程 → gdb 连接 → continue → 窗口出现 ✅
- **实机测试**: 用 `start_direct.ps1` → 直接启动 chipSetter.exe → 无需 gdb → 窗口立即可见 ✅

#### Session 0 陷阱

**问题**: `schtasks /create /it` + `schtasks /run` 会把进程丢到 Session 0（无 GUI），即使有用户登录。

**根因**: `schtasks /run` 忽略 `/it` 标志，直接在非交互会话中执行。

**修复**: 不用 `/run`，调度时间设 5 秒后，等调度器自然触发。调度器会尊重 `/it` 并将进程放入用户会话 (Session 1+)。

```powershell
# 错误: schtasks /run → Session 0, 无 GUI
schtasks /create /tn "task" /tr "..." /sc once /st $time /it /f
schtasks /run /tn "task"   # ← 这行破坏了 /it

# 正确: 等调度器触发 → Session 1, 有 GUI
schtasks /create /tn "task" /tr "..." /sc once /st $time /it /f
Start-Sleep -Seconds 10    # 等调度器在用户会话中执行
```
