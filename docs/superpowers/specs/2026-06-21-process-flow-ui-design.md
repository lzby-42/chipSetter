# 固晶机工艺流程操作 GUI — 设计规格书

**日期:** 2026-06-21
**目标平台:** Win10 工控机 (6GB存储, 2GB内存)
**分辨率:** 1200×800 (可适配)
**技术栈:** Qt 5.15.2 + C++, 固高 GNC SDK
**前置设计:** [[2026-06-13-chipsetter-ui-design]]

---

## 1. 概述

在现有调试模式（手动控制面板）基础上，新增**生产模式（工艺流程界面）**。操作员在生产模式下通过大块流程图直观看到当前工艺步骤、点击查看步骤详情。生产/调试通过底部按钮一键切换。

### 1.1 与现有设计的关系

本设计是对 `2026-06-13-chipsetter-ui-design.md` 的扩展。现有设计中 `ProductionWidget` 仅为一个状态看板（产量数字+轴灯+报警），本次将其替换为完整的工艺流程交互界面。调试模式面板保持不变。

## 2. 工艺流程定义

### 2.1 工艺步骤

固晶机九步工艺流程：

| 序号 | 步骤 | 英文 | 说明 |
|------|------|------|------|
| ① | 初始化 | Init | 13轴归零、IO检测、参数加载、气压检测 |
| ② | 上料 | Load | 晶圆盘移动到位、吸嘴上料 |
| ③ | 定位 | Align | 视觉对位、晶圆X/Y位置修正 |
| ④ | 顶晶 | Eject | 顶针Z1/Z2上升顶出晶粒 |
| ⑤ | 拾取 | Pick | Z轴下降、吸嘴开启真空拾取晶粒 |
| ⑥ | 点胶 | Dispense | 点胶X/Y移动到目标位置、点胶阀开启 |
| ⑦ | 贴装 | Place | Z轴下降到贴装高度、关闭真空释放晶粒 |
| ⑧ | 下料 | Unload | Z轴回升、基板移出、完成一件 |
| ⑨ | 收尾 | Finish | 所有轴回安全位置、关闭使能、保存统计数据 |

### 2.2 流程结构

```
        ┌───────────────────────────────────────┐
        │         生产循环 (Cycle)               │
        │                                       │
① 初始化 →│ ②上料 → ③定位 → ④顶晶 → ⑤拾取     │→ ⑨ 收尾
        │    → ⑥点胶 → ⑦贴装 → ⑧下料 ──┐       │
        │                          ↑  │       │
        │                          └──┘       │
        └───────────────────────────────────────┘
```

- 初始化(①)和收尾(⑨)位于循环框**外部**
- ②~⑧用绿色框体包裹，底部虚线箭头表示下料→上料的回路
- 正常生产时自动循环②~⑧；收尾只在停止生产时触发

## 3. 双模式架构

### 3.1 模式定义

| 模式 | 界面 | 使用者 | 用途 |
|------|------|--------|------|
| 生产模式 | 工艺流程UI | 操作员 | 日常生产运行、监控 |
| 调试模式 | 手动控制UI (现有) | 工程师 | 单轴调试、参数配置、IO测试 |

### 3.2 切换方式

- 底部操作栏右侧提供 `⇄ 切换` 按钮
- 使用 `QStackedWidget`: 第0页=生产, 第1页=调试
- 两个界面共享底层 Core 数据（MotorManager、IoManager、AlarmLogger、StatsCollector）
- 模式切换由 `BottomBarWidget::modeSwitchClicked` 信号触发

### 3.3 共用组件

- **状态栏 (StatusBarWidget):** 双模式都显示在顶部，内容一致
- **操作栏 (BottomBarWidget):** 双模式都显示在底部，按钮一致 + 模式切换
- **Core 层:** 完全共用，UI切换不影响硬件控制逻辑

## 4. 生产模式布局

### 4.1 整体布局 (1200×800)

```
┌──────────────────────────────────────────────────────────┐
│ 1. 状态栏: 标题 | 模式 | 运行状态 | 产量 | 节拍 | 时长 │ 32px
├─────────────────────────────────────────┬────────────────┤
│                                         │                │
│ 2. 工艺流程 (FlowStepBar)               │                │
│  ① 初始化 → [②上料→③定位→④顶晶→⑤拾取  │ 5. 报警列表    │
│              →⑥点胶→⑦贴装→⑧下料]       │ (AlarmList)   │
│              ↰__________回路________↲   │                │
│              → ⑨ 收尾                  │                │
│                                         │                │
├─────────────────────────────────────────┤                │
│ 3. 步骤详情 (StepDetailPanel)            │                │
│  子步骤列表 | 步骤参数 | 实时状态         │                │
├─────────────────────────────────────────┤                │
│ 4. 设备状态 (DeviceStatus)               │                │
│  13轴指示灯 · IO关键信号 · 通讯状态      │                │
├─────────────────────────────────────────┴────────────────┤
│ 6. 操作栏: 急停 | 启动 | 暂停 | 复位 | [⇄ 调试] | GNC  │ 36px
└──────────────────────────────────────────────────────────┘
```

### 4.2 区域详细设计

#### 区域 1: 状态栏 — 共用 `StatusBarWidget`

保持不变。显示：应用标题、模式标签(自动/手动)、运行状态(运行中/暂停)、产量、节拍、运行时长、时间戳。

#### 区域 2: 工艺流程 — 新组件 `FlowStepBar`

**功能:**
- 水平排列9个步骤块，生产循环②~⑧用绿色框体包裹
- 框内底部显示虚线回路箭头（⑧ 回到 ②）
- 每个步骤块三种状态：
  - **已完成:** 绿色背景 `#1b5e20` + ✓ 标记
  - **运行中:** 蓝色背景 `#1565c0` + 发光边框 + ◉ 标记
  - **待执行:** 灰色背景 `#2a2a3a` + ○ 标记
- 每个步骤块可点击 → 选中后下方步骤详情面板更新
- 步骤块尺寸足够大(≥60px宽)，适合触屏点击

**信号:**
```cpp
signals:
    void stepClicked(int stepIndex);     // 用户点击步骤块 (0~8)
    void stepStateChanged(int stepIndex, int state);  // 0=待执行, 1=运行中, 2=已完成
```

**槽:**
```cpp
public slots:
    void setStepState(int stepIndex, int state);  // Core层更新步骤状态
    void setCurrentStep(int stepIndex);           // 设置当前活跃步骤
```

#### 区域 3: 步骤详情 — 新组件 `StepDetailPanel`

**功能:**
- 三栏布局，对应三个方面的详情
- 当用户点击流程块或当前步骤变化时自动切换内容

**左栏 — 子步骤列表:**
- 该工艺步骤的子步骤清单
- 每项带状态标记：✓ 完成 / ◉ 执行中 / ○ 待做
- 示例(拾取步骤): Z轴下降 ✓ → 开启真空 ◉ → 顶针退回 ○ → 上升安全高度 ○ → 真空检测 ○

**中栏 — 步骤参数:**
- 该步骤的可调参数，名值对显示
- 示例(拾取步骤): 拾取高度 -2.500mm / 真空延时 150ms / 顶针推力 0.8N / 安全高度 15mm

**右栏 — 实时状态:**
- 该步骤执行时的实时数据
- 示例: Z轴位置 -1.832mm / 真空压力 -85kPa / 步骤耗时 0.8s / 本件节拍 3.2s

**信号:**
```cpp
signals:
    void paramChanged(int stepIndex, const QString& paramName, double value);
```

**槽:**
```cpp
public slots:
    void showStepDetail(int stepIndex);                    // 切换显示的步骤
    void updateSubstep(int stepIndex, int substep, int state);
    void updateParam(int stepIndex, const QString& name, double value);
    void updateRealtimeStatus(int stepIndex, const QVariantMap& status);
```

#### 区域 4: 设备状态 — 新组件 `DeviceStatusWidget`

**功能:**
- 13轴指示灯（与现有 ProductionWidget 的轴灯一致）
- IO关键信号摘要：急停状态、当前触发的限位信号
- GNC通讯状态

**颜色规则:**
- 🟢 绿色 `#4caf50`: 轴使能就绪
- 🔵 蓝色 `#2196f3`: 运动中
- 🔴 红色 `#f44336`: 报警
- ⚫ 灰色 `#555`: 未使能

#### 区域 5: 报警列表 — 共用 `AlarmListWidget` (精简版)

生产模式下报警列表使用精简显示：每条报警一行(颜色条+级别标记+消息+时间)，最多显示8条，带"清除全部"按钮。

#### 区域 6: 操作栏 — 共用 `BottomBarWidget`

保持不变。仅需确认模式切换按钮在生产/调试两个界面中都可见。

## 5. 新增文件清单

| 文件 | 类型 | 职责 |
|------|------|------|
| `widgets/FlowStepBar.h/.cpp` | 新Widget | 工艺流程步骤条（9个步骤块+循环框+回路箭头） |
| `widgets/StepDetailPanel.h/.cpp` | 新Widget | 步骤详情三栏面板（子步骤+参数+实时状态） |
| `widgets/DeviceStatusWidget.h/.cpp` | 新Widget | 设备状态概览（轴灯+IO+通讯） |
| `widgets/ProductionWidget.h/.cpp` | 重写 | 从状态看板改为工艺流程完整界面（组合上述组件） |
| `core/ProcessManager.h/.cpp` | 新Core | 工艺流程管理器（步骤状态机、步骤切换、自动循环逻辑） |

## 6. 数据流

### 6.1 流程执行流程

```
用户点击"启动" → BottomBarWidget::startClicked()
  → ProcessManager::startCycle()
    → ProcessManager::executeStep(0)  // 初始化
      → 更新FlowStepBar: setCurrentStep(0), setStepState(0, RUNNING)
      → 执行子步骤 (通过MotorManager/IoManager)
      → 完成后: setStepState(0, COMPLETED)
    → executeStep(1)  // 上料
      → ...
    → executeStep(8)  // 下料
      → 完成后 → 循环回 executeStep(1) (生产循环)
    → 用户点击"暂停"/"收尾"
      → executeStep(9)  // 收尾
```

### 6.2 步骤详情更新流程

```
用户点击流程块③
  → FlowStepBar::stepClicked(3)
  → StepDetailPanel::showStepDetail(3)
    → 加载步骤③的子步骤列表
    → 加载步骤③的参数
    → 开始显示实时状态

ProcessManager 定时更新
  → StepDetailPanel::updateRealtimeStatus(3, {...})
  → StepDetailPanel::updateSubstep(3, 1, COMPLETED)
```

### 6.3 Core层新增: ProcessManager

```cpp
class ProcessManager : public QObject {
    Q_OBJECT
public:
    enum StepState { PENDING, RUNNING, COMPLETED, FAILED };
    
    // 9个步骤的静态定义
    struct StepDef {
        int index;
        QString name;
        QStringList substeps;       // 子步骤名称列表
        QVariantMap defaultParams;  // 默认参数
    };
    
    void startCycle();    // 开始生产循环
    void pauseCycle();    // 暂停（完成当前步骤后停止）
    void resumeCycle();   // 继续
    void finishCycle();   // 执行收尾后停止
    void skipToStep(int stepIndex);  // 跳步
    
signals:
    void stepStateChanged(int stepIndex, StepState state);
    void currentStepChanged(int stepIndex);
    void substepStateChanged(int stepIndex, int substepIndex, StepState state);
    void cycleCompleted();           // 一个完整循环完成(产量+1)
    void allFinished();              // 收尾完成
};
```

## 7. 颜色/样式规范

### 7.1 步骤状态颜色

| 状态 | 背景色 | 边框 | 文字色 | 标记 |
|------|--------|------|--------|------|
| 已完成 | `#1b5e20` | `#4caf50` | `#81c784` | ✓ |
| 运行中 | `#1565c0` | `#64b5f6` + glow | `#fff` | ◉ |
| 待执行 | `#2a2a3a` | `#333` | `#666` | ○ |
| 失败 | `#b71c1c` | `#f44336` | `#ef9a9a` | ✕ |

### 7.2 循环框

- 边框: `2px solid #2e7d32`
- 背景: `#0d1a0d` (半透明深绿)
- 回路箭头: `#ffd740` 虚线 + 箭头标记

### 7.3 区域标题

- 所有功能区域必须有标题标签
- 标题颜色: `#64b5f6`，加粗
- 标题字体: 10~11px

## 8. 实现注意事项

### 8.1 步骤块绘制

流程块用 QPushButton（setCheckable + 自定义 QSS）实现，方便点击交互。循环框用 QFrame + 自定义 paintEvent 绘制虚线和箭头。

### 8.2 架构一致性

遵循现有架构铁律：
- UI 不直接调用 GNC SDK
- ProcessManager 通过 MotorManager/IoManager 间接控制硬件
- 所有通信通过 Qt Signal/Slot

### 8.3 向后兼容

- 现有调试面板代码不做结构性修改
- ProductionWidget.cpp 重写，但保持.h 中已有信号槽签名不变
- BottomBarWidget 的现有信号保持不变

### 8.4 触屏友好

- 流程块最小点击区域: 60×60px
- 操作按钮最小尺寸: 80×36px
- 步骤详情面板可滚动

---

**版本:** v1.0
**作者:** Claude + 用户协作
**关联文档:** [[2026-06-13-chipsetter-ui-design]]
