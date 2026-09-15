#include "acquisition/modbustcpclient.h"

#include "common/logger.h"

#include <QDateTime>

namespace semieq {

namespace {
const char *kModule = "ModbusTcpClient";

/// 功能码
const quint8 kFuncReadHoldingRegisters = 0x03;
/// 异常响应时功能码最高位置 1
const quint8 kExceptionFlag = 0x80;

/// 追加一个大端 16 位整数（Modbus 所有多字节字段都是大端）
void appendBigEndian16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

/// 从大端字节流读 16 位整数
quint16 readBigEndian16(const QByteArray &data, int offset)
{
    return static_cast<quint16>((static_cast<quint8>(data.at(offset)) << 8)
                                | static_cast<quint8>(data.at(offset + 1)));
}
} // namespace

// ---------------------------------------------------------------- 构造/析构 ---

ModbusTcpClient::ModbusTcpClient(QObject *parent)
    : IDeviceClient(parent)
{
}

ModbusTcpClient::ModbusTcpClient(const Config &config, QObject *parent)
    : IDeviceClient(parent)
    , m_config(config)
{
}

QString ModbusTcpClient::name() const
{
    return QStringLiteral("Modbus/TCP");
}

bool ModbusTcpClient::isConnected() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState;
}

void ModbusTcpClient::setConfig(const Config &config)
{
    m_config = config;
}

void ModbusTcpClient::setParameters(const QVector<Parameter> &params)
{
    m_parameters = params;
}

// -------------------------------------------------------------- 纯函数实现 ---

QByteArray ModbusTcpClient::buildReadHoldingRegistersRequest(int transactionId,
                                                             int slaveId,
                                                             int startAddress,
                                                             int registerCount)
{
    QByteArray frame;
    frame.reserve(12);

    appendBigEndian16(frame, static_cast<quint16>(transactionId));  // 事务标识
    appendBigEndian16(frame, 0);                                     // 协议标识固定 0
    appendBigEndian16(frame, 6);                                     // 后续字节数 = 单元标识 + PDU = 1 + 5
    frame.append(static_cast<char>(slaveId & 0xFF));                 // 单元标识
    frame.append(static_cast<char>(kFuncReadHoldingRegisters));       // 功能码
    appendBigEndian16(frame, static_cast<quint16>(startAddress));     // 起始地址
    appendBigEndian16(frame, static_cast<quint16>(registerCount));    // 寄存器个数

    return frame;
}

int ModbusTcpClient::expectedResponseLength(int registerCount)
{
    // MBAP 头 7 + 功能码 1 + 字节数 1 + 数据 2N
    return 9 + registerCount * 2;
}

QVector<quint16> ModbusTcpClient::parseReadHoldingRegistersResponse(const QByteArray &frame,
                                                                   bool            *ok,
                                                                   QString         *error)
{
    const auto fail = [ok, error](const QString &reason) {
        if (ok) {
            *ok = false;
        }
        if (error) {
            *error = reason;
        }
        return QVector<quint16>();
    };

    if (frame.size() < 9) {
        return fail(QStringLiteral("响应帧长度不足：%1 字节（至少 9 字节）").arg(frame.size()));
    }

    // 单元标识位于第 7 字节（下标 6）
    const quint8 unitId = static_cast<quint8>(frame.at(6));
    Q_UNUSED(unitId)

    const quint8 funcCode = static_cast<quint8>(frame.at(7));

    // 异常响应：功能码最高位置 1，后跟 1 字节异常码
    if ((funcCode & kExceptionFlag) != 0) {
        const quint8 exceptionCode = frame.size() > 8 ? static_cast<quint8>(frame.at(8)) : 0;
        return fail(QStringLiteral("设备返回异常响应：功能码 0x%1，异常码 0x%2")
                        .arg(funcCode, 2, 16, QLatin1Char('0'))
                        .arg(exceptionCode, 2, 16, QLatin1Char('0')));
    }

    if (funcCode != kFuncReadHoldingRegisters) {
        return fail(QStringLiteral("功能码不匹配：期望 0x03，实际 0x%1")
                        .arg(funcCode, 2, 16, QLatin1Char('0')));
    }

    const int byteCount = static_cast<quint8>(frame.at(8));
    if (byteCount % 2 != 0) {
        return fail(QStringLiteral("字节数非法（应为偶数）：%1").arg(byteCount));
    }

    const int available = frame.size() - 9;
    if (available < byteCount) {
        return fail(QStringLiteral("数据段不完整：声明 %1 字节，实际只有 %2 字节")
                        .arg(byteCount)
                        .arg(available));
    }

    QVector<quint16> values;
    values.reserve(byteCount / 2);
    for (int i = 0; i < byteCount; i += 2) {
        values.append(readBigEndian16(frame, 9 + i));
    }

    if (ok) {
        *ok = true;
    }
    return values;
}

// ------------------------------------------------------------------ 连接 ---

void ModbusTcpClient::connectToDevice()
{
    if (m_socket == nullptr) {
        m_socket = new QTcpSocket(this);

        connect(m_socket, &QTcpSocket::connected, this, &ModbusTcpClient::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &ModbusTcpClient::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &ModbusTcpClient::onReadyRead);
        connect(m_socket,
                QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                this,
                &ModbusTcpClient::onSocketError);
    }

    if (isConnected()) {
        return;
    }

    LOG_INFO(kModule,
             QStringLiteral("正在连接 %1:%2（从站 %3）")
                 .arg(m_config.host)
                 .arg(m_config.port)
                 .arg(m_config.slaveId));

    m_rxBuffer.clear();
    m_socket->connectToHost(m_config.host, m_config.port);
}

void ModbusTcpClient::disconnectFromDevice()
{
    if (m_socket == nullptr) {
        return;
    }
    m_socket->disconnectFromHost();
}

void ModbusTcpClient::onConnected()
{
    LOG_INFO(kModule, QStringLiteral("连接成功 %1:%2").arg(m_config.host).arg(m_config.port));
    emit connected();
}

void ModbusTcpClient::onDisconnected()
{
    m_waitingResponse = false;
    LOG_WARN(kModule, QStringLiteral("连接已断开"));
    emit disconnected();
}

void ModbusTcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    const QString reason = m_socket != nullptr ? m_socket->errorString()
                                              : QStringLiteral("未知套接字错误(%1)").arg(int(error));
    LOG_ERROR(kModule, QStringLiteral("通信错误：%1").arg(reason));
    emit connectionError(reason);
}

// ------------------------------------------------------------------ 采集 ---

void ModbusTcpClient::poll()
{
    if (!isConnected()) {
        emit connectionError(QStringLiteral("未连接，无法采集"));
        return;
    }

    // TODO(D5)：改为"上一帧未收到响应则跳过本次轮询"，避免慢设备下请求堆积
    ++m_transactionId;
    const QByteArray request = buildReadHoldingRegistersRequest(m_transactionId,
                                                                m_config.slaveId,
                                                                m_config.startAddress,
                                                                m_config.registerCount);

    m_waitingResponse = true;
    m_rxBuffer.clear();
    m_socket->write(request);
}

void ModbusTcpClient::onReadyRead()
{
    if (m_socket == nullptr) {
        return;
    }
    m_rxBuffer.append(m_socket->readAll());

    const int expected = expectedResponseLength(m_config.registerCount);
    if (m_rxBuffer.size() < expected) {
        return;                      // 拆包：还没收齐，等下一次 readyRead
    }

    // 取走一帧（多余字节暂不处理，模拟器/真实设备通常不回多帧）
    const QByteArray frame = m_rxBuffer.left(expected);
    m_rxBuffer.remove(0, expected);
    m_waitingResponse = false;

    bool    ok = false;
    QString error;
    const QVector<quint16> registers = parseReadHoldingRegistersResponse(frame, &ok, &error);
    if (!ok) {
        LOG_ERROR(kModule, QStringLiteral("解析失败：%1").arg(error));
        emit connectionError(error);
        return;
    }

    // 寄存器值 → 采集点（按定义顺序一一对应）
    const QDateTime now = QDateTime::currentDateTime();
    QVector<Parameter> out;
    out.reserve(registers.size());

    for (int i = 0; i < registers.size(); ++i) {
        Parameter p;
        if (i < m_parameters.size()) {
            p = m_parameters.at(i);          // 带上编码/名称/单位/上下限
        } else {
            p.code = QStringLiteral("REG_%1").arg(m_config.startAddress + i);
            p.name = p.code;
        }
        p.value     = registers.at(i);
        p.timestamp = now;
        out.append(p);
    }

    emit parametersRead(out);
}

} // namespace semieq
