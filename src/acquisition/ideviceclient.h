#ifndef SEMIEQ_IDEVICECLIENT_H
#define SEMIEQ_IDEVICECLIENT_H

// =============================================================================
//  设备通信客户端抽象接口
//
//  设计决策：为什么要有这一层？
//   上位机要面对"数据从哪来"的多种可能：真实设备（Modbus-TCP）、串口设备、
//   开发/演示期的模拟器。如果业务层直接 new QTcpSocket，换一种来源就要改业务代码。
//   把"取数"抽象成 IDeviceClient，业务层只依赖接口 → 这是策略模式，也是
//   "面向接口编程"的直接落地（面试高频考点）。
//
//  实现类：
//    ModbusTcpClient  —— TCP 直连（本项目主通道）
//    ModbusRtuClient  —— 串口 RS485 / Modbus-RTU（D2 实现）
//    SimulatorClient  —— 进程内模拟器（单元测试用，D2 实现）
// =============================================================================

#include "common/types.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace semieq {

class IDeviceClient : public QObject
{
    Q_OBJECT

public:
    explicit IDeviceClient(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IDeviceClient() override = default;

    /// 客户端名称（用于日志与界面显示）
    virtual QString name() const = 0;

    /// 当前是否已连接
    virtual bool isConnected() const = 0;

public slots:
    /// 发起连接（异步；结果通过 connected / connectionError 通知）
    virtual void connectToDevice() = 0;

    /// 断开连接
    virtual void disconnectFromDevice() = 0;

    /// 触发一次采集（由采集线程按周期调用）
    virtual void poll() = 0;

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &reason);

    /// 一批采集结果。
    /// ⚠️ 跨线程（QueuedConnection）传递需要先 qRegisterMetaType（见 main.cpp）
    void parametersRead(const QVector<semieq::Parameter> &params);
};

} // namespace semieq

#endif // SEMIEQ_IDEVICECLIENT_H
