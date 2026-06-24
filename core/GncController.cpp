/**
 * @file GncController.cpp
 * @brief MockGncController 实现 — 模拟GNC控制器行为
 */

#include "GncController.h"
#include "HardwareConfig.h"
#include <QDebug>
#include <cstring>

// ============================================================
// MockGncController 构造/析构
// ============================================================

MockGncController::MockGncController(QObject *parent)
    : IGncController(parent)
    , m_connected(false)
    , m_tickCount(0)
{
    // 初始化13轴状态
    m_axes.resize(AXIS_COUNT_MOCK);
    for (int i = 0; i < AXIS_COUNT_MOCK; ++i) {
        MockAxisState& ax = m_axes[i];
        ax.enabled     = false;
        ax.currentPos  = 100.0 + i * 50.0;      // 初始位置分散
        ax.targetPos   = ax.currentPos;
        ax.velocity    = 0.0;
        ax.moving      = false;
        ax.homing      = false;
        ax.homeStage   = 0;
        ax.homeError   = false;
    }

    // 初始化DI (1~19, 对应GPI1-GPI19 / X0-X18)
    memset(m_diValues, 0, sizeof(m_diValues));
    // 模拟: Home传感器默认触发(高电平), 限位不触发(低电平)
    m_diValues[3]  = 1;   // GPI3  X2  点胶平台X_Home
    m_diValues[6]  = 1;   // GPI6  X5  点胶平台Y_Home
    m_diValues[9]  = 1;   // GPI9  X8  取晶X_Home
    m_diValues[15] = 1;   // GPI15 X14 吸嘴Home
    // 其余为0 (限位不触发 = 正常状态)

    // 初始化DO (索引9-12对应Y9-Y12), 默认全低
    memset(m_doValues, 0, sizeof(m_doValues));

    // 启动模拟定时器
    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &MockGncController::onSimulationTick);
}

MockGncController::~MockGncController()
{
    if (m_simTimer->isActive())
        m_simTimer->stop();
    closeCard();
}

// ============================================================
// 生命周期
// ============================================================

bool MockGncController::openCard()
{
    qDebug() << "[MockGnc] openCard() — 模拟开卡成功, m_connected=true";
    m_connected = true;
    qDebug() << "[MockGnc] DI初始化状态: DI[3]=" << m_diValues[3]
             << " DI[6]=" << m_diValues[6]
             << " DI[9]=" << m_diValues[9]
             << " DI[15]=" << m_diValues[15]
             << " (Home传感器=1, 其余=" << m_diValues[1] << ")";
    return true;
}

bool MockGncController::netInit(const QString& xmlFile, int overTimeSec)
{
    Q_UNUSED(xmlFile);
    Q_UNUSED(overTimeSec);
    qDebug() << "[MockGnc] netInit() — 模拟网络初始化成功";
    return true;
}

bool MockGncController::loadConfig(short core, const QString& cfgFile)
{
    Q_UNUSED(core);
    Q_UNUSED(cfgFile);
    qDebug() << "[MockGnc] loadConfig() — 模拟配置加载成功";
    return true;
}

bool MockGncController::closeCard()
{
    m_connected = false;
    if (m_simTimer->isActive())
        m_simTimer->stop();
    qDebug() << "[MockGnc] closeCard() — 模拟关卡";
    return true;
}

bool MockGncController::isConnected() const
{
    return m_connected;
}

// ============================================================
// 轴操作
// ============================================================

bool MockGncController::axisOn(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].enabled = true;
    qDebug() << "[MockGnc] axisOn(" << axis << ")";
    return true;
}

bool MockGncController::axisOff(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].enabled = false;
    return true;
}

bool MockGncController::clearStatus(short core, short axis, short count)
{
    Q_UNUSED(core);
    for (short i = 0; i < count && (axis + i - 1) < AXIS_COUNT_MOCK; ++i) {
        m_axes[axis + i - 1].moving = false;
        m_axes[axis + i - 1].homing = false;
    }
    return true;
}

bool MockGncController::zeroPosition(short core, short axis)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;
    m_axes[axis - 1].currentPos = 0.0;
    return true;
}

// ============================================================
// 点位运动
// ============================================================

bool MockGncController::moveAbsolute(short core, short axis, const TMoveAbsolutePrmEx& prm)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    if (!ax.enabled) return false;

    ax.targetPos  = prm.pos;
    ax.velocity   = prm.vel;
    ax.moving     = true;
    ax.homing     = false;

    qDebug() << "[MockGnc] moveAbsolute axis=" << axis
             << " target=" << prm.pos
             << " vel=" << prm.vel
             << " acc=" << prm.acc
             << " dec=" << prm.dec;
    return true;
}

// ============================================================
// 回零
// ============================================================

bool MockGncController::executeHome(short core, short axis, const TStandardHomePrm& prm)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    ax.homing      = true;
    ax.moving      = true;
    ax.homeStage   = 1; // STANDARD_HOME_STAGE_START
    ax.homePrm     = prm;
    ax.homeError   = false;
    ax.homeCapturePos = 0;

    qDebug() << "[MockGnc] executeHome axis=" << axis << " mode=" << prm.mode;
    return true;
}

bool MockGncController::getHomeStatus(short core, short axis, TStandardHomeStatus& sts)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];
    memset(&sts, 0, sizeof(sts));
    sts.run        = ax.homing ? 1 : 0;
    sts.stage      = ax.homeStage;
    sts.error      = ax.homeError ? 1 : 0;
    sts.capturePos = ax.homeCapturePos;
    return true;
}

// ============================================================
// 状态读取
// ============================================================

bool MockGncController::getAxisStatus(short core, short axis, long& status, unsigned long& clock)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    MockAxisState& ax = m_axes[axis - 1];

    // 构建GNC风格的轴状态字
    status = 0;
    if (ax.enabled)  status |= 0x001;    // 使能
    if (ax.moving)   status |= 0x400;    // 运动中 (bit10)
    if (ax.homing)   status |= 0x800;    // 回零中
    if (ax.homeError) status |= 0x1000;  // 错误

    clock = static_cast<unsigned long>(m_tickCount);
    return true;
}

bool MockGncController::getProfilePosition(short core, short axis, double& prfPos, unsigned long& clock)
{
    Q_UNUSED(core);
    if (axis < 1 || axis > AXIS_COUNT_MOCK) return false;

    prfPos = m_axes[axis - 1].currentPos;
    clock  = static_cast<unsigned long>(m_tickCount);
    return true;
}

bool MockGncController::getLimitInfo(short core, short axis, TLimitInfo& info)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    memset(&info, 0, sizeof(info));
    return true;
}

// ============================================================
// IO
// ============================================================

bool MockGncController::readDI(short core, short diType, short diIndex, short* values, short count)
{
    Q_UNUSED(core);
    Q_UNUSED(diType);
    static int callCount = 0;
    callCount++;
    for (short i = 0; i < count; ++i) {
        int idx = diIndex + i;
        if (idx >= 1 && idx <= DI_COUNT)
            values[i] = m_diValues[idx];
        else
            values[i] = 0;
    }
    if (callCount <= 3) {
        QString valStr;
        for (short i = 0; i < count; ++i) valStr += QString::number(values[i]);
        qDebug() << "[MockGnc] readDI #" << callCount << " diType=" << diType
                 << " count=" << count << " values=[" << valStr << "]";
    }
    return true;
}

bool MockGncController::writeDO(short core, short doType, short doIndex, short* values, short count)
{
    Q_UNUSED(core);
    Q_UNUSED(doType);
    for (short i = 0; i < count; ++i) {
        int idx = doIndex + i;
        if (idx >= DO_INDEX_BASE && idx < DO_INDEX_BASE + DO_COUNT)
            m_doValues[idx] = values[i];
    }
    return true;
}

// ============================================================
// 软限位
// ============================================================

bool MockGncController::setSoftLimit(short core, short axis, double positive, double negative)
{
    Q_UNUSED(core);
    Q_UNUSED(axis);
    Q_UNUSED(positive);
    Q_UNUSED(negative);
    return true;
}

// ============================================================
// Mock专用方法
// ============================================================

void MockGncController::setDIValue(int diIndex, int value)
{
    if (diIndex >= 1 && diIndex <= DI_COUNT)
        m_diValues[diIndex] = static_cast<short>(value);
}

int MockGncController::getDIValue(int diIndex) const
{
    if (diIndex >= 1 && diIndex <= DI_COUNT)
        return m_diValues[diIndex];
    return 0;
}

void MockGncController::toggleDI(int diIndex)
{
    if (diIndex >= 1 && diIndex <= DI_COUNT)
        m_diValues[diIndex] = m_diValues[diIndex] ? 0 : 1;
}

// ============================================================
// 模拟定时器回调 — 每50ms驱动一次模拟运动
// ============================================================

void MockGncController::onSimulationTick()
{
    m_tickCount++;
    double dt = SIM_TICK_MS / 1000.0;   // 秒

    for (int i = 0; i < AXIS_COUNT_MOCK; ++i) {
        MockAxisState& ax = m_axes[i];

        if (ax.homing) {
            // 模拟回零流程: 快速过阶段
            double homeSpeed = ax.homePrm.highSpeed;
            ax.currentPos += homeSpeed * dt * 0.5;
            ax.homeStage++;
            if (ax.homeStage > 9) {
                // 回零完成
                ax.homing = false;
                ax.moving = false;
                ax.currentPos = 0.0;
                ax.homeStage = 9; // STANDARD_HOME_STAGE_END
            }
        }
        else if (ax.moving) {
            // 模拟点位运动: 匀速趋近目标
            double diff = ax.targetPos - ax.currentPos;
            double step = ax.velocity * dt;
            if (qAbs(diff) <= step) {
                ax.currentPos = ax.targetPos;
                ax.moving = false;
            } else {
                ax.currentPos += (diff > 0 ? step : -step);
            }
        }
    }
}
