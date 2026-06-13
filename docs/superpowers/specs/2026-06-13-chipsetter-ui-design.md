# 固晶机控制系统 UI — 设计规格书

**日期:** 2026-06-13  
**目标平台:** Win10 工控机 (6GB存储, 2GB内存)  
**分辨率:** 1200×800 (可适配)  
**技术栈:** Qt 5.15.2 + C++, 固高 GNC SDK  

---

## 1. 项目概述

固晶机 (Die Bonder) 设备上位机控制系统，运行在固高GNC控制器内置的Win10系统上。核心功能为13轴电机点位运动控制、IO信号监视、报警管理、生产统计。

## 2. 架构原则

### 2.1 前后端分离

```
┌──────────────────────┐
│   UI 层 (widgets/)   │  ← QWidget子类, 纯展示+用户交互
│   信号/槽 通信        │  ← 通过 Qt Signal/Slot 解耦
│   Core 层 (core/)    │  ← 业务逻辑, GNC SDK封装, 数据管理
│   Model 层 (models/) │  ← 纯数据结构, 无逻辑
└──────────────────────┘
```

**铁律: UI层不直接调用GNC SDK。UI只发信号给Core层，Core层处理后通过信号返回结果。**

### 2.2 模块独立性

每个面板是独立QWidget子类，满足:
- 无直接依赖其他面板 (不 #include 其他 Widget)
- 通过 MainWindow 的信号/槽连接进行跨模块通信
- 可单独测试 (Mock Core层即可运行单个面板)

### 2.3 接口优先

- 每个模块公开 `.h` 头文件定义完整的 public API
- 数据模型使用独立头文件，GUI和Core共享
- 轴ID、IO编号等硬件标识使用宏定义，集中在 `core/HardwareConfig.h`

## 3. 模块设计

### 3.1 UI模块 (widgets/)

| 模块 | 文件 | 职责 | 依赖 |
|------|------|------|------|
| 状态栏 | `StatusBarWidget` | 模式显示、运行状态、时间 | models/ObservableData |
| 电机PTP | `MotorPtpWidget` | 轴选择、目标位置、加减速、运行/停止/回零 | core/MotorManager (信号/槽) |
| 电机参数 | `MotorParamWidget` | 参数表格、读写、保存加载 | core/MotorManager (信号/槽) |
| IO监视 | `IoMonitorWidget` | DI/DO实时状态、信号别名 | core/IoManager (信号/槽) |
| 报警列表 | `AlarmListWidget` | 实时报警、历史记录、级别筛选 | core/AlarmLogger (信号/槽) |
| 生产统计 | `StatsWidget` | 产量、时长、节拍 | core/StatsCollector (信号/槽) |
| 底部操作栏 | `BottomBarWidget` | 急停/启动/暂停/复位、模式切换、通讯状态 | core/GncController (信号/槽) |

### 3.2 Core模块 (core/)

| 模块 | 文件 | 职责 |
|------|------|------|
| GNC封装 | `GncController` | 固高SDK初始化、点位运动、回零、急停、IO读写 |
| 电机管理 | `MotorManager` | 13轴状态维护、参数缓存、运动协调 |
| IO管理 | `IoManager` | DI/DO状态轮询、信号别名映射 |
| 报警日志 | `AlarmLogger` | 报警触发/恢复、历史记录、级别分类 |
| 硬件配置 | `HardwareConfig.h` | 轴ID宏、IO编号宏、默认参数常量 |
| 统计收集 | `StatsCollector` | 产量计数、运行时长、节拍计算 |

### 3.3 Model (models/)

| 模块 | 文件 | 职责 |
|------|------|------|
| 电机轴 | `MotorAxis.h` | 轴ID、名称、当前位置、目标位置、速度、加减速、限位状态 |
| IO信号 | `IoSignal.h` | 信号ID、名称、类型(DI/DO)、当前状态、别名 |
| 报警记录 | `AlarmRecord.h` | 时间戳、级别、来源、消息、是否已恢复 |

## 4. 通信协议

### 4.1 内部通信: Qt Signal/Slot

```
UI Widget ──signal──→ Core Manager ──slot──→ GncController ──GNC SDK──→ 硬件
UI Widget ←──slot──── Core Manager ←──signal── GncController ←──GNC SDK── 硬件
```

- UI 发送用户操作信号 (如 `moveToPosition(axisId, pos, vel, acc, dec)`)
- Core 处理后通过信号返回结果 (如 `positionReached(axisId)`, `alarmRaised(record)`)
- UI 收到信号后更新显示

### 4.2 外部通信: GNC SDK

- 固高GNC控制器提供C++ SDK
- `GncController` 类封装所有SDK调用
- 其他模块不直接调用SDK

## 5. 布局方案

```
┌───────────────────────────────────────────────────┐ 1200
│  顶部状态栏: 模式 | 状态 | 产量 | 时间             │ 32px
├──────────────────────┬──────────┬─────────────────┤
│  电机点位运动 (PTP)   │ IO信号   │  报警列表        │
│  530×300             │ 自适应   │  280×280        │
│  ·轴选择(13轴)       │ ·DI限位  │  ·实时报警      │
│  ·当前位置/目标位置   │ ·DO输出  │  ·历史记录      │
│  ·运行速度/加减速     │          │                 │
│  ·运行/停止/回零/JOG  │          │                 │
├──────────────────────┤          ├─────────────────┤
│  电机参数管理         │ 生产统计 │  视觉预览(预留)  │
│  530×380             │ 自适应   │  280×420        │
│  ·轴选择+参数表      │ ·产量    │                 │
│  ·保存/加载          │ ·时长    │                 │
│                      │ ·节拍    │                 │
├──────────────────────┴──────────┴─────────────────┤
│  底部操作栏: 急停|启动|暂停|复位 | 通讯状态         │ 36px
└───────────────────────────────────────────────────┘
```

## 6. 数据流

### 6.1 电机运动流程

```
用户输入(目标位置/速度/加减速) → MotorPtpWidget
  → emit moveRequest(axisId, targetPos, vel, acc, dec)
  → MotorManager::onMoveRequest()
    → validateParams()       // 参数校验(软限位、速度范围)
    → GncController::ptpMove()
    → 定时轮询当前位置
    → emit positionUpdated(axisId, pos)
  → MotorPtpWidget::onPositionUpdated()  // 更新显示
  → GncController::moveComplete()
  → emit moveFinished(axisId, success)
```

### 6.2 IO刷新流程

```
定时器(50ms) → IoManager::pollIoState()
  → GncController::readDI() / readDO()
  → 对比上次状态, detectChange()
  → emit diChanged(id, state), doChanged(id, state)
  → IoMonitorWidget::onDiChanged() / onDoChanged()
  → 更新颜色显示
  → (同时) 限位信号变化 → MotorManager::onLimitChanged()
    → 触发报警或急停逻辑
```

## 7. 硬件配置集中管理

`core/HardwareConfig.h` 示例:

```cpp
// ===== 轴ID定义 (临时, 待GNC文档确认) =====
#define AXIS_COUNT          13

#define AXIS_WAFER_X        0
#define AXIS_WAFER_Y        1
#define AXIS_DISPENSE_X     2
#define AXIS_DISPENSE_Y     3
#define AXIS_EJECTOR_Z1     4
#define AXIS_EJECTOR_Z2     5
#define AXIS_ROTARY_WHEEL   6
// AXIS_7 ~ AXIS_12 待确认

// ===== IO定义 =====
#define DI_COUNT            16
#define DO_COUNT            4

// 数字输入 (DI)
#define DI_WAFER_X_HOME     0
#define DI_WAFER_Y_HOME     1
#define DI_WAFER_X_LIMIT_P  2
#define DI_WAFER_X_LIMIT_N  3
#define DI_WAFER_Y_LIMIT_P  4
#define DI_WAFER_Y_LIMIT_N  5
#define DI_DISPENSE_X_HOME  6
#define DI_DISPENSE_Y_HOME  7
#define DI_DISPENSE_X_LIMIT_P  8
#define DI_DISPENSE_X_LIMIT_N  9
#define DI_DISPENSE_Y_LIMIT_P  10
#define DI_DISPENSE_Y_LIMIT_N  11
#define DI_EJECTOR_Z1_HOME  12
#define DI_EJECTOR_Z2_HOME  13
#define DI_ROTARY_HOME      14
#define DI_EMERGENCY_STOP   15

// 数字输出 (DO)
#define DO_RED_LIGHT        0
#define DO_YELLOW_LIGHT     1
#define DO_GREEN_LIGHT      2
#define DO_BUZZER           3

// ===== 默认运动参数 =====
#define DEFAULT_VELOCITY    100.0f   // mm/s
#define DEFAULT_ACCEL       500.0f   // mm/s²
#define DEFAULT_DECEL       500.0f   // mm/s²
#define DEFAULT_JOG_STEP    0.010f   // mm
```

## 8. 远程调试方案

### 8.1 网络配置

```
开发PC (Qt Creator)  ←──网线直连──→  目标GNC工控机 (Win10)
   静态IP: 192.168.1.100              静态IP: 192.168.1.101
```

### 8.2 调试流程

1. **目标机准备:**
   - 安装 Qt 5.15.2 运行时 (MSVC 2019 x86/x64 匹配)
   - 或静态链接编译，减小部署体积
   - 开启 Windows Remote Debugging 服务

2. **部署方式 (按调试阶段选择):**

   | 阶段 | 方式 | 说明 |
   |------|------|------|
   | 初期 | Qt Creator Remote Debug | 网线直连, 远程启动+断点 |
   | 中期 | 网络共享部署 | 编译输出到目标机共享目录 |
   | 后期 | 单文件部署 | windeployqt 打包, 直接运行 |

3. **Qt Creator 配置:**
   - Device: 添加 Windows Device, IP 192.168.1.101
   - Kit: Qt 5.15.2 + MSVC, Device选目标机
   - Build: Release + Debug Info (Qt Remote Debug需要)

4. **备用方案:**
   - 如远程调试不稳定，使用 `qDebug()` 日志输出到文件
   - 目标机上启动时自动开启日志: `main.cpp` 中 `qInstallMessageHandler()`

## 9. 待确认项

| 项目 | 状态 | 说明 |
|------|------|------|
| 轴7-12的精确ID和名称 | ⚠ 待确认 | 目前用宏预留占位 |
| GNC SDK具体API函数名 | ⚠ 待确认 | GncController预留接口 |
| IO其余DI通道用途 | ⚠ 待确认 | 当前已分配16个DI宏 |
| 视觉对位模块 | 🔵 预留 | UI留空区域 |
| 气动/点胶控制 | 🔵 预留 | 暂不实现 |
| 权限管理 | 🔵 预留 | 暂不实现 |
| 系统设置 | 🔵 预留 | 暂不实现 |

---

**版本:** v1.0  
**作者:** Claude + 用户协作
