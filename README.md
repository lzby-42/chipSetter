# 固晶机控制系统 (chipSetter)

基于 Qt 5.15.2 的固晶机 (Die Bonder) 上位机控制系统，运行于固高 GNC 控制器内置 Win10 工控机。

## 系统架构

```
┌─────────────────────────────────────────────────────┐
│                  MainWindow                          │
│  ┌──────────────────────────────────────────────┐   │
│  │  StatusBarWidget (双模式共享)                  │   │
│  ├──────────────────────────────────────────────┤   │
│  │  QStackedWidget                               │   │
│  │  ┌─ 第0页: 生产模式 ──────────────────────┐  │   │
│  │  │  ProductionWidget                        │  │   │
│  │  │    ├─ FlowStepBar     (工艺流程步骤条)   │  │   │
│  │  │    ├─ StepDetailPanel  (步骤详情三栏)    │  │   │
│  │  │    ├─ DeviceStatusWidget (设备状态)      │  │   │
│  │  │    └─ 报警列表                           │  │   │
│  │  └──────────────────────────────────────────┘  │   │
│  │  ┌─ 第1页: 调试模式 ──────────────────────┐  │   │
│  │  │  MotorPtpWidget | IoMonitorWidget        │  │   │
│  │  │  MotorParamWidget | StatsWidget          │  │   │
│  │  │  AlarmListWidget | 视觉预留               │  │   │
│  │  └──────────────────────────────────────────┘  │   │
│  ├──────────────────────────────────────────────┤   │
│  │  BottomBarWidget (双模式共享)                 │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘

UI层 (widgets/) → Signal/Slot → Core层 (core/) → GNC SDK
```

**铁律: UI 层不直接调用 GNC SDK。所有硬件操作通过 Core 层 Signal/Slot。**

## 目录结构

```
chipSetter/
├── main.cpp                      # 应用入口
├── mainwindow.h/.cpp             # 主窗口, 信号连线, 模式切换
├── chipSetter.pro                # Qt 工程文件
├── resources.qrc                 # Qt 资源文件
├── README.md                     # 本文件
│
├── core/                         # Core 层 — 业务逻辑
│   ├── HardwareConfig.h          # 轴ID/IO宏/默认参数 (集中修改点)
│   ├── GncController.h/.cpp      # GTS SDK 封装 (GT_Open/GT_GetDi/GT_SetDoBit...)
│   ├── MotorManager.h/.cpp       # 13轴状态管理, 运动协调
│   ├── IoManager.h/.cpp          # DI/DO 轮询, 变化检测
│   ├── AlarmLogger.h/.cpp        # 报警触发/恢复, 历史记录
│   ├── StatsCollector.h/.cpp     # 产量/时长/节拍统计
│   ├── ProcessManager.h/.cpp     # 9步工艺状态机, 自动循环
│   ├── DispensingPlatformController.h/.cpp  # 点胶平台 (轴2+3)
│   └── PickupPlatformController.h/.cpp      # 取晶平台 (轴10+11)
│
├── models/                       # 数据模型 — 纯结构
│   ├── MotorAxis.h               # 轴状态
│   ├── IoSignal.h                # IO信号
│   └── AlarmRecord.h             # 报警记录
│
├── widgets/                      # UI 层 — 面板组件
│   ├── StatusBarWidget.h/.cpp    # 顶部状态栏
│   ├── BottomBarWidget.h/.cpp    # 底部操作栏 (急停/启动/暂停/复位/切换)
│   ├── ProductionWidget.h/.cpp   # 生产模式 — 工艺流程界面组合根
│   ├── FlowStepBar.h/.cpp        # 工艺流程步骤条 (9块+循环框+回路箭头)
│   ├── StepDetailPanel.h/.cpp    # 步骤详情 (子步骤/参数/实时状态三栏)
│   ├── DeviceStatusWidget.h/.cpp # 设备状态 (13轴灯+IO+通讯)
│   ├── MotorPtpWidget.h/.cpp     # 电机点位运动 (调试模式)
│   ├── MotorParamWidget.h/.cpp   # 电机参数管理 (调试模式)
│   ├── IoMonitorWidget.h/.cpp    # IO信号监视 (调试模式)
│   ├── AlarmListWidget.h/.cpp    # 报警列表 (调试模式)
│   └── StatsWidget.h/.cpp        # 生产统计 (调试模式)
│
├── resources/
│   └── style.qss                 # 全局深色工业主题样式表
│
├── tests/                        # 单元测试
│   ├── tests.pro                 # 测试工程 (subdirs)
│   ├── test_process_manager.pro  # ProcessManager 测试
│   ├── test_process_manager.cpp
│   ├── test_hardware_config.pro  # HardwareConfig 测试
│   ├── test_hardware_config.cpp
│   ├── test_motor_axis.pro       # MotorAxis 测试
│   └── test_motor_axis.cpp
│
├── scripts/                      # 编译脚本
│   ├── build_debug.bat           # 一键编译 (双击运行)
│   ├── build_debug.ps1           # PowerShell 包装
│   ├── build_debug.sh            # MSYS2 Bash 脚本
│   └── workflow.ps1              # 统一调度 (编译→打包→部署→启动)
│
└── docs/                         # 设计文档
    └── superpowers/
        ├── specs/                # 设计规格书
        └── plans/                # 实现计划
```

## 模式切换

| 模式 | 界面 | 使用者 | QStackedWidget 索引 |
|------|------|--------|---------------------|
| 生产模式 | 工艺流程 UI | 操作员 | 0 |
| 调试模式 | 手动控制 UI | 工程师 | 1 |

- 底部操作栏 **⇄ 调试 / ⇄ 生产** 按钮切换
- 生产界面内也有 **调试** 按钮跳转
- 状态栏和操作栏为双模式共享

## 工艺流程

```
① 初始化 → ┌─────────────────────────────┐ → ⑨ 收尾
           │  生产循环                      │
           │  ②上料 → ③定位 → ④顶晶        │
           │  → ⑤拾取 → ⑥点胶 → ⑦贴装      │
           │  → ⑧下料 ──→ (循环回②)        │
           └─────────────────────────────┘
```

- 点击流程块 → 下方显示步骤详情（子步骤/参数/实时状态）
- 运行中当前步骤蓝色发光，已完成绿色，待执行灰色
- 绿色框体包裹循环步骤，虚线箭头表示回路
- 核心逻辑见 `core/ProcessManager.h/.cpp`

## 构建

### 环境要求

- Qt 5.15.2 MinGW 32-bit (`D:/tool/qt/5.15.2/mingw81_32`)
- MSYS2 with MINGW32 工具链 (`D:/Code_Languages/C/msys64`)
- 固高 GTS SDK (`googol/` 目录)

### 编译

```bash
# 方式1: 双击运行
scripts/build_debug.bat

# 方式2: PowerShell
powershell -File scripts/build_debug.ps1

# 方式3: MSYS2 MINGW32 终端
bash scripts/build_debug.sh

# 清理后重新编译
scripts/build_debug.bat clean
```

编译成功后输出 `debug/chipSetter.exe`。

### 构建并运行测试

```bash
cd tests
qmake tests.pro
make -f Makefile.Debug -j4
./debug/test_hardware_config.exe -txt
./debug/test_motor_axis.exe -txt
./debug/test_process_manager.exe -txt
```

### 部署到目标工控机

```bash
# 使用 windeployqt 收集依赖
windeployqt release/chipSetter.exe
```

## 关键接口

### ProcessManager (核心状态机)

```cpp
// 控制
void startCycle();     // 开始生产循环
void pauseCycle();     // 暂停
void resumeCycle();    // 继续
void finishCycle();    // 收尾
void emergencyStop();  // 急停

// 信号
stepStateChanged(int stepIndex, int state)
currentStepChanged(int stepIndex)
substepStateChanged(int stepIndex, int substepIndex, int state)
cycleCompleted(int totalCount)
allFinished()
```

### FlowStepBar (流程步骤条)

```cpp
void setSteps(const QStringList& names);       // 设置9步名称
void setStepState(int stepIndex, int state);   // 更新步骤块颜色
void setCurrentStep(int stepIndex);            // 标记当前步骤
signal stepClicked(int stepIndex);             // 用户点击
```

### 子组件访问 (MainWindow 连线用)

```cpp
auto* bar = productionWidget->flowStepBar();
auto* panel = productionWidget->stepDetailPanel();
auto* status = productionWidget->deviceStatusWidget();
```

## 测试

```bash
cd tests
./release/test_process_manager.exe -txt
# Totals: 10 passed, 0 failed
```

测试用例覆盖: 初始状态、启动循环、步骤推进、循环逻辑、暂停继续、收尾、急停、步骤定义。
