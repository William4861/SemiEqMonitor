#ifndef SEMIEQ_DEVICESIMULATOR_H
#define SEMIEQ_DEVICESIMULATOR_H

// =============================================================================
//  设备模拟器（SemiEqSimulator）—— 扮演"设备侧"
//
//  为什么必须有它：
//    半导体设备几十万一台，学生不可能有真实硬件。但上位机软件的绝大多数问题
//    （粘包/拆包、断线重连、超限报警、数据入库）只有在对端真实存在时才暴露。
//    所以自己写一个对端：对外用 Modbus-TCP 提供数据，并且可以手动注入故障。
//
//  附带好处：
//    面试官拿到仓库后，不需要任何硬件就能跑起来看到完整效果 —— 可复现性本身就是加分项。
//
//  控制台命令（运行 SemiEqSimulator 后直接输入）：
//     h  → 注入故障：腔体温度飙升（触发上位机上限报警）
//     n  → 恢复正常
//     d  → 强制断开当前客户端（触发上位机的断线检测与重连）
//     l  → 打印当前模拟值
//     q  → 退出
// =============================================================================

#include <QObject>
#include <QThread>
#include <QVector>

class QTcpServer;
class QTcpSocket;
class QTimer;

namespace semieq {

/// 控制台输入读取线程。
/// Windows 上没有可靠的 stdin 事件通知机制，所以用一个阻塞读线程把输入转成信号。
class ConsoleReader : public QThread
{
    Q_OBJECT

public:
    explicit ConsoleReader(QObject *parent = nullptr);

signals:
    void commandEntered(const QString &command);

protected:
    void run() override;
};

class DeviceSimulator : public QObject
{
    Q_OBJECT

public:
    struct Config {
        quint16 port            = 1502;   ///< 默认用 1502（502 是标准端口，易与真实服务冲突）
        int     slaveId         = 1;      ///< 从站地址
        int     registerCount   = 8;      ///< 寄存器个数
        int     valueIntervalMs = 500;    ///< 模拟值刷新周期
    };

    explicit DeviceSimulator(const Config &config, QObject *parent = nullptr);
    ~DeviceSimulator() override;

    bool    start();
    void    stop();
    bool    isListening() const;
    quint16 port() const { return m_config.port; }

    /// 模拟值刷新一次（内部方法，单元测试也可直接调用）
    void tick();

    // ------------------------------------------------------- 采集点定义 ---
    static QVector<QString> registerNames();
    static QVector<QString> registerUnits();
    static QVector<double>  registerLowerLimits();
    static QVector<double>  registerUpperLimits();

public slots:
    /// 处理一条控制台命令（h/n/d/l/q）。设为 public 是为了让模拟器 main 能直接连接。
    void onCommand(const QString &command);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    void respondReadHoldingRegisters(QTcpSocket *socket,
                                     quint16      transactionId,
                                     quint16      startAddress,
                                     quint16      count);
    void log(const QString &message);

    Config          m_config;
    QTcpServer     *m_server = nullptr;
    QTcpSocket     *m_client = nullptr;
    QTimer         *m_timer  = nullptr;
    QVector<double> m_values;
    bool            m_injectHighTemp = false;   ///< 故障注入：腔体温度超上限
    qint64          m_phase          = 0;      ///< 正弦波相位（让曲线自然变化）
};

} // namespace semieq

#endif // SEMIEQ_DEVICESIMULATOR_H
