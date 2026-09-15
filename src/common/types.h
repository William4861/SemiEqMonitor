#ifndef SEMIEQ_TYPES_H
#define SEMIEQ_TYPES_H

// =============================================================================
//  领域数据模型（半导体设备语境）
//
//  术语对照（面试可用）：
//     Lot   批次        —— 半导体按批次组织生产
//     Wafer 晶圆        —— 一个 Lot 含多片晶圆（片号 slot）
//     Die   芯片/晶粒    —— Wafer 上的最小单位，用 (x, y) 坐标定位
//     Yield 良率        —— 良品 Die / 总 Die
//     USL / LSL         —— 规格上限 / 规格下限（Upper/Lower Specification Limit）
//     PM    Preventive Maintenance —— 预防性维护（设备非故障的计划停机）
// =============================================================================

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace semieq {

// ---------------------------------------------------------------- 设备状态 ---
enum class DeviceState {
    Unknown = 0,
    Idle,       ///< 待机
    Running,    ///< 运行中
    Down,       ///< 故障停机
    PM          ///< 预防性维护
};

inline QString deviceStateName(DeviceState state)
{
    switch (state) {
    case DeviceState::Idle:    return QStringLiteral("待机");
    case DeviceState::Running: return QStringLiteral("运行");
    case DeviceState::Down:    return QStringLiteral("故障");
    case DeviceState::PM:      return QStringLiteral("维护");
    case DeviceState::Unknown: break;
    }
    return QStringLiteral("未知");
}

// ---------------------------------------------------------------- 采集参数 ---
/// 一路参数采集点（对应 Modbus 寄存器，或模拟器上报通道）
struct Parameter {
    int       id         = -1;
    QString   code;                 ///< 采集点编码，如 "TEMP_CH1"
    QString   name;                 ///< 中文名，如 "腔体温度"
    QString   unit;                 ///< 单位，如 "℃"
    double    value      = 0.0;     ///< 当前值
    double    lowerLimit = 0.0;     ///< LSL 下限规格线
    double    upperLimit = 0.0;     ///< USL 上限规格线
    QDateTime timestamp;
};

// ------------------------------------------------------------------ 报警 ---
enum class AlarmLevel {
    Info = 0,
    Warning,
    Critical
};

inline QString alarmLevelName(AlarmLevel level)
{
    switch (level) {
    case AlarmLevel::Info:     return QStringLiteral("提示");
    case AlarmLevel::Warning:  return QStringLiteral("警告");
    case AlarmLevel::Critical: return QStringLiteral("严重");
    }
    return QStringLiteral("未知");
}

struct Alarm {
    qint64     id          = 0;
    QString    paramCode;
    AlarmLevel level       = AlarmLevel::Warning;
    QString    message;
    double     triggerValue = 0.0;
    QDateTime  raisedAt;
    QDateTime  clearedAt;            ///< 无效(isNull)表示尚未恢复
    bool       acknowledged = false;
    QString    ackBy;

    bool isActive() const { return clearedAt.isNull(); }
};

// ------------------------------------------------------------------ 芯片 ---
enum class DieResult {
    Untested = 0,
    Pass,
    Fail
};

inline QString dieResultName(DieResult result)
{
    switch (result) {
    case DieResult::Pass: return QStringLiteral("良品");
    case DieResult::Fail: return QStringLiteral("不良");
    case DieResult::Untested: break;
    }
    return QStringLiteral("未测");
}

struct Die {
    int       x      = 0;
    int       y      = 0;
    DieResult result = DieResult::Untested;
    QString   failCode;              ///< 不良代码，如 "SHORT" / "OPEN"
};

// ------------------------------------------------------------------ 晶圆 ---
struct Wafer {
    QString      waferId;
    QString      lotId;
    int          slot = 0;           ///< 片号
    int          cols = 0;           ///< Die 阵列列数
    int          rows = 0;           ///< Die 阵列行数
    QVector<Die> dies;
    QDateTime    startedAt;
    QDateTime    finishedAt;

    int totalCount() const { return dies.size(); }

    int passCount() const
    {
        int n = 0;
        for (const Die &d : dies) {
            if (d.result == DieResult::Pass) {
                ++n;
            }
        }
        return n;
    }

    int failCount() const
    {
        int n = 0;
        for (const Die &d : dies) {
            if (d.result == DieResult::Fail) {
                ++n;
            }
        }
        return n;
    }

    /// 良率 = 良品 / 总数（总数为 0 时返回 0，避免除零）
    double yield() const
    {
        const int total = totalCount();
        return total > 0 ? static_cast<double>(passCount()) / total : 0.0;
    }
};

// ------------------------------------------------------------------ 批次 ---
struct Lot {
    QString   lotId;
    QString   productModel;
    int       plannedWafers = 0;
    QDateTime startedAt;
    QDateTime finishedAt;
};

} // namespace semieq

// ---------------------------------------------------------------------------
//  注册自定义类型，使它们能通过 QueuedConnection 跨线程传递。
//  注意：这里只是"让类型可被注册"，运行时还要在 main.cpp 调用 qRegisterMetaType()，
//        否则采集线程 → UI 线程的信号会在运行时被丢弃并打印告警。
// ---------------------------------------------------------------------------
Q_DECLARE_METATYPE(semieq::Parameter)
Q_DECLARE_METATYPE(semieq::Alarm)
Q_DECLARE_METATYPE(semieq::Wafer)
Q_DECLARE_METATYPE(QVector<semieq::Parameter>)
Q_DECLARE_METATYPE(QVector<semieq::Alarm>)

#endif // SEMIEQ_TYPES_H
