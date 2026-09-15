#include "devicesimulator.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtMath>

#include <cstdio>
#include <iostream>
#include <string>

namespace semieq {

namespace {
const char  *kModule                = "DeviceSimulator";
const quint8 kFuncReadHoldingRegs   = 0x03;
const quint8 kExceptionIllegalFunc  = 0x01;   ///< 非法功能码
const quint8 kExceptionIllegalAddr  = 0x02;   ///< 非法数据地址
const int    kMbapHeaderSize        = 7;
const int    kRequestSize           = 12;     ///< MBAP 7 + 功能码 1 + 地址 2 + 个数 2

void appendBigEndian16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

quint16 readBigEndian16(const QByteArray &data, int offset)
{
    return static_cast<quint16>((static_cast<quint8>(data.at(offset)) << 8)
                                | static_cast<quint8>(data.at(offset + 1)));
}

/// 简单伪随机噪声（避免依赖 QRandomGenerator，便于复现问题）
double noise(double amplitude)
{
    static quint32 seed = 20260915u;
    seed = seed * 1103515245u + 12345u;
    const double unit = static_cast<double>((seed >> 16) & 0x7FFF) / 32767.0;   // [0,1]
    return (unit - 0.5) * 2.0 * amplitude;                                      // [-amp, amp]
}
} // namespace

// =========================================================== ConsoleReader ===

ConsoleReader::ConsoleReader(QObject *parent)
    : QThread(parent)
{
}

void ConsoleReader::run()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        const QString command = QString::fromStdString(line).trimmed();
        if (!command.isEmpty()) {
            emit commandEntered(command);
        }
    }
}

// ======================================================= DeviceSimulator ===

DeviceSimulator::DeviceSimulator(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_values.resize(m_config.registerCount);
}

DeviceSimulator::~DeviceSimulator()
{
    stop();
}

QVector<QString> DeviceSimulator::registerNames()
{
    return {QStringLiteral("腔体温度"),
            QStringLiteral("腔体压力"),
            QStringLiteral("冷却水流量"),
            QStringLiteral("主轴转速"),
            QStringLiteral("加热功率"),
            QStringLiteral("真空度"),
            QStringLiteral("传送带速度"),
            QStringLiteral("电源电流")};
}

QVector<QString> DeviceSimulator::registerUnits()
{
    return {QStringLiteral("℃"), QStringLiteral("Pa"), QStringLiteral("L/min"),
            QStringLiteral("rpm"), QStringLiteral("W"), QStringLiteral("kPa"),
            QStringLiteral("mm/s"), QStringLiteral("A")};
}

QVector<double> DeviceSimulator::registerLowerLimits()
{
    return {20.0, 90.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
}

QVector<double> DeviceSimulator::registerUpperLimits()
{
    return {80.0, 120.0, 12.0, 3000.0, 900.0, 1.0, 120.0, 16.0};
}

bool DeviceSimulator::start()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &DeviceSimulator::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, m_config.port)) {
        log(QStringLiteral("监听失败：%1（端口 %2 可能被占用）")
                .arg(m_server->errorString())
                .arg(m_config.port));
        return false;
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &DeviceSimulator::tick);
    m_timer->start(m_config.valueIntervalMs);

    tick();   // 先算出一组初值

    log(QStringLiteral("设备模拟器已启动：0.0.0.0:%1（从站 %2，寄存器 %3 个）")
            .arg(m_config.port)
            .arg(m_config.slaveId)
            .arg(m_config.registerCount));
    return true;
}

void DeviceSimulator::stop()
{
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_client != nullptr) {
        m_client->disconnectFromHost();
        m_client = nullptr;
    }
    if (m_server != nullptr && m_server->isListening()) {
        m_server->close();
    }
}

bool DeviceSimulator::isListening() const
{
    return m_server != nullptr && m_server->isListening();
}

void DeviceSimulator::tick()
{
    ++m_phase;

    const double t = static_cast<double>(m_phase) * m_config.valueIntervalMs / 1000.0;

    // 基准值 + 缓慢正弦 + 小幅噪声，让上位机的曲线看起来像真实设备
    // ⚠️ 注意：三目运算符别和加法混写（45.0 + flag ? a : b 会被解析成 (45.0+flag) ? a : b）
    m_values[0] = m_injectHighTemp ? 95.0
                                   : 45.0 + 8.0 * qSin(t / 6.0) + noise(1.2);
    m_values[1] = 101.0 + 3.0 * qSin(t / 9.0) + noise(0.4);
    m_values[2] = 6.0 + 1.5 * qSin(t / 4.0) + noise(0.2);
    m_values[3] = 1500.0 + 400.0 * qSin(t / 7.0) + noise(25.0);
    m_values[4] = 420.0 + 120.0 * qSin(t / 5.0) + noise(8.0);
    m_values[5] = 0.6 + 0.2 * qSin(t / 8.0) + noise(0.02);
    m_values[6] = 60.0 + 20.0 * qSin(t / 10.0) + noise(2.0);
    m_values[7] = 8.5 + 2.0 * qSin(t / 3.0) + noise(0.15);
}

void DeviceSimulator::onNewConnection()
{
    if (m_client != nullptr) {
        log(QStringLiteral("已有客户端连接，拒绝新连接"));
        QTcpSocket *extra = m_server->nextPendingConnection();
        if (extra != nullptr) {
            extra->close();
            extra->deleteLater();
        }
        return;
    }

    m_client = m_server->nextPendingConnection();
    connect(m_client, &QTcpSocket::readyRead, this, &DeviceSimulator::onReadyRead);
    connect(m_client, &QTcpSocket::disconnected, this, &DeviceSimulator::onClientDisconnected);

    log(QStringLiteral("客户端已连接：%1:%2")
            .arg(m_client->peerAddress().toString())
            .arg(m_client->peerPort()));
}

void DeviceSimulator::onClientDisconnected()
{
    if (m_client != nullptr) {
        log(QStringLiteral("客户端已断开"));
        m_client->deleteLater();
        m_client = nullptr;
    }
}

void DeviceSimulator::onReadyRead()
{
    if (m_client == nullptr) {
        return;
    }

    static QByteArray buffer;      // 简化处理：本项目只有单个客户端
    buffer.append(m_client->readAll());

    while (buffer.size() >= kRequestSize) {
        const QByteArray frame = buffer.left(kRequestSize);
        buffer.remove(0, kRequestSize);

        const quint16 transactionId = readBigEndian16(frame, 0);
        const quint8  funcCode      = static_cast<quint8>(frame.at(7));
        const quint16 startAddress  = readBigEndian16(frame, 8);
        const quint16 count         = readBigEndian16(frame, 10);

        if (funcCode == kFuncReadHoldingRegs) {
            respondReadHoldingRegisters(m_client, transactionId, startAddress, count);
        } else {
            // 异常响应：功能码 | 0x80 + 异常码
            QByteArray response;
            appendBigEndian16(response, transactionId);
            appendBigEndian16(response, 0);
            appendBigEndian16(response, 3);
            response.append(static_cast<char>(m_config.slaveId & 0xFF));
            response.append(static_cast<char>(kFuncReadHoldingRegs | 0x80));
            response.append(static_cast<char>(kExceptionIllegalFunc));
            m_client->write(response);

            log(QStringLiteral("收到不支持的功能码 0x%1，已回异常响应")
                    .arg(funcCode, 2, 16, QLatin1Char('0')));
        }
    }
}

void DeviceSimulator::respondReadHoldingRegisters(QTcpSocket *socket,
                                                  quint16      transactionId,
                                                  quint16      startAddress,
                                                  quint16      count)
{
    if (startAddress + count > static_cast<quint16>(m_values.size())) {
        QByteArray response;
        appendBigEndian16(response, transactionId);
        appendBigEndian16(response, 0);
        appendBigEndian16(response, 3);
        response.append(static_cast<char>(m_config.slaveId & 0xFF));
        response.append(static_cast<char>(kFuncReadHoldingRegs | 0x80));
        response.append(static_cast<char>(kExceptionIllegalAddr));
        socket->write(response);

        log(QStringLiteral("请求地址越界（起始 %1，个数 %2）").arg(startAddress).arg(count));
        return;
    }

    const quint16 byteCount = count * 2;

    QByteArray response;
    response.reserve(9 + byteCount);
    appendBigEndian16(response, transactionId);          // 回显事务标识
    appendBigEndian16(response, 0);                      // 协议标识
    appendBigEndian16(response, 3 + byteCount);          // 后续字节数 = 单元标识 + PDU
    response.append(static_cast<char>(m_config.slaveId & 0xFF));
    response.append(static_cast<char>(kFuncReadHoldingRegs));
    response.append(static_cast<char>(byteCount & 0xFF));

    for (quint16 i = 0; i < count; ++i) {
        // 寄存器按 uint16 传输，先把 double 量化成整数
        const double value = m_values.at(startAddress + i);
        appendBigEndian16(response, static_cast<quint16>(qBound(0.0, value, 65535.0)));
    }

    socket->write(response);
}

void DeviceSimulator::onCommand(const QString &command)
{
    const QString cmd = command.toLower();

    if (cmd == QLatin1String("h")) {
        m_injectHighTemp = true;
        log(QStringLiteral("⚠️ 已注入故障：腔体温度飙升（将触发上位机上限报警）"));
    } else if (cmd == QLatin1String("n")) {
        m_injectHighTemp = false;
        log(QStringLiteral("✅ 故障已清除，参数恢复正常"));
    } else if (cmd == QLatin1String("d")) {
        if (m_client != nullptr) {
            m_client->disconnectFromHost();
            log(QStringLiteral("已强制断开客户端（用于验证上位机的断线重连）"));
        } else {
            log(QStringLiteral("当前没有客户端连接"));
        }
    } else if (cmd == QLatin1String("l")) {
        const QVector<QString> names = registerNames();
        const QVector<QString> units = registerUnits();
        for (int i = 0; i < m_values.size() && i < names.size(); ++i) {
            log(QStringLiteral("  [%1] %2 = %3 %4")
                    .arg(i)
                    .arg(names.at(i))
                    .arg(m_values.at(i), 0, 'f', 2)
                    .arg(i < units.size() ? units.at(i) : QString()));
        }
    } else if (cmd == QLatin1String("q")) {
        log(QStringLiteral("再见"));
        QCoreApplication::quit();
    } else {
        log(QStringLiteral("未知命令：%1（可用命令 h/n/d/l/q）").arg(command));
    }
}

void DeviceSimulator::log(const QString &message)
{
    const QString line = QStringLiteral("[%1] [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")),
                                  QString::fromLatin1(kModule),
                                  message);
    std::cout << line.toStdString() << std::endl;
}

} // namespace semieq
