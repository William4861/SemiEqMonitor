#ifndef SEMIEQ_MODBUSTCPCLIENT_H
#define SEMIEQ_MODBUSTCPCLIENT_H

// =============================================================================
//  Modbus-TCP 设备客户端
//
//  协议结构（本项目只用到功能码 0x03 = 读保持寄存器）：
//
//    请求帧：  MBAP 头（7 字节）              + PDU（5 字节）
//      ┌──────────────┬──────────────┬──────────┬─────────────┬────────┬───────────┬───────────┐
//      │ 事务标识 2B  │ 协议标识 2B  │ 长度 2B  │ 单元标识 1B │ 功能码 │ 起始地址  │ 寄存器数  │
//      │ (自增)       │ (=0)         │ (=6)     │ (从站号)    │ (0x03) │   2B      │    2B     │
//      └──────────────┴──────────────┴──────────┴─────────────┴────────┴───────────┴───────────┘
//
//    响应帧：  MBAP 头（7 字节）              + PDU（2 + 2N 字节）
//      │ 事务标识 2B │ 协议标识 2B │ 长度 2B │ 单元标识 1B │ 功能码 │ 字节数 1B │ 数据 2N 字节 │
//
//  ⚠️ 所有多字节字段均为【大端】（网络字节序）—— 这是最常踩的坑，已用单元测试锁住。
// =============================================================================

#include "acquisition/ideviceclient.h"

#include <QByteArray>
#include <QTcpSocket>
#include <QVector>

namespace semieq {

class ModbusTcpClient : public IDeviceClient
{
    Q_OBJECT

public:
    struct Config {
        QString host          = QStringLiteral("127.0.0.1");
        quint16 port          = 502;
        int     slaveId       = 1;      ///< 从站地址（MBAP 单元标识符）
        int     startAddress  = 0;      ///< 起始寄存器地址
        int     registerCount = 8;      ///< 单次读取的寄存器个数
        int     timeoutMs     = 1000;   ///< 单次请求超时
    };

    explicit ModbusTcpClient(QObject *parent = nullptr);
    explicit ModbusTcpClient(const Config &config, QObject *parent = nullptr);

    QString name() const override;
    bool    isConnected() const override;

    void   setConfig(const Config &config);
    Config config() const { return m_config; }

    /// 设置采集点定义（与寄存器顺序一一对应）
    void setParameters(const QVector<Parameter> &params);

    // ------------------------------------------------------------ 纯函数区 ---
    // 这两个函数不依赖网络，可直接单元测试（tests/test_modbus.cpp）

    /// 组装 0x03 读保持寄存器请求帧
    static QByteArray buildReadHoldingRegistersRequest(int transactionId,
                                                       int slaveId,
                                                       int startAddress,
                                                       int registerCount);

    /// 解析 0x03 响应帧 → 寄存器值列表。
    /// @param ok    传出：是否解析成功
    /// @param error 传出：失败原因（成功时为未修改）
    static QVector<quint16> parseReadHoldingRegistersResponse(const QByteArray &frame,
                                                              bool            *ok,
                                                              QString         *error);

    /// 响应帧的期望总长度（用于粘包判断）
    static int expectedResponseLength(int registerCount);

public slots:
    void connectToDevice() override;
    void disconnectFromDevice() override;
    void poll() override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    Config             m_config;
    QTcpSocket        *m_socket     = nullptr;
    QByteArray         m_rxBuffer;              ///< 收包缓冲（解决 TCP 粘包/拆包）
    int                m_transactionId = 0;
    QVector<Parameter> m_parameters;            ///< 采集点定义
    bool               m_waitingResponse = false;
};

} // namespace semieq

#endif // SEMIEQ_MODBUSTCPCLIENT_H
