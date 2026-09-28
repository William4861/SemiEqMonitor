// =============================================================================
//  D2 第 2 步 · 连接部分集成测试
//
//  为什么单独一个可执行：
//    Qt Test 一个可执行只能有一个 main（一个 QTEST_*_MAIN），所以 TCP 这组
//    单独编译成 semieq_tcp_tests.exe。
//
//  为什么用"进程内 QTcpServer"而不是起 SemiEqSimulator：
//    这是【自动验收】用的。测试自己当"设备侧"占住一个端口，跑完就释放，
//    不依赖你先手动开模拟器、也不依赖端口 1502 没被占用 → 可重复、CI 友好。
//    （真机联调仍然用 scripts\run.bat sim + scripts\run.bat monitor，
//      那是等 D6 菜单做完之后的事。）
//
//  运行： scripts\run.bat tcptest
//     或： build\bin\semieq_tcp_tests.exe
// =============================================================================

#include "acquisition/modbustcpclient.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QtTest/QtTest>

using namespace semieq;

class TestTcpConnect : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void connectsSuccessfully();
    void emitsErrorWhenPortClosed();
    void disconnectEmitsDisconnected();

private:
    /// 向操作系统要一个"当前空闲"的端口号，拿到后立刻释放，用来构造"连不上"的场景
    static quint16 reserveFreePort();

    QTcpServer m_server;         ///< 扮演设备侧：只监听、不回应业务数据
};

void TestTcpConnect::initTestCase()
{
    // 端口 0 = 让操作系统分配一个空闲端口，避免和本机别的服务撞车
    QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
}

void TestTcpConnect::cleanupTestCase()
{
    m_server.close();
}

quint16 TestTcpConnect::reserveFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const quint16 port = probe.serverPort();
    probe.close();               // 立刻放掉 → 这个端口此刻是空的
    return port;
}

// ---------------------------------------------------------------------------
//  ① 连得上：connected 信号发出，isConnected() 为真
// ---------------------------------------------------------------------------
void TestTcpConnect::connectsSuccessfully()
{
    ModbusTcpClient::Config config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = m_server.serverPort();

    ModbusTcpClient client(config);

    QSignalSpy connectedSpy(&client, &IDeviceClient::connected);
    QSignalSpy errorSpy(&client, &IDeviceClient::connectionError);

    client.connectToDevice();    // ⚠️ 异步：这一行返回时通常还没连上

    // 信号没来就说明 connectToDevice() 还没实现好（或信号没连上）
    QVERIFY2(connectedSpy.wait(3000),
             "3 秒内没等到 connected 信号：确认 connectToDevice() 里做了 "
             "new QTcpSocket / 连信号 / connectToHost 三件事");

    QCOMPARE(errorSpy.count(), 0);
    QVERIFY(client.isConnected());
}

// ---------------------------------------------------------------------------
//  ② 连不上：必须通过 connectionError 把原因报出来（不能静默失败）
// ---------------------------------------------------------------------------
void TestTcpConnect::emitsErrorWhenPortClosed()
{
    const quint16 deadPort = reserveFreePort();
    QVERIFY(deadPort != 0);

    ModbusTcpClient::Config config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = deadPort;

    ModbusTcpClient client(config);

    QSignalSpy errorSpy(&client, &IDeviceClient::connectionError);
    QSignalSpy connectedSpy(&client, &IDeviceClient::connected);

    client.connectToDevice();

    QVERIFY2(errorSpy.wait(3000),
             "连一个没人监听的端口时必须发 connectionError —— "
             "检查 error 信号有没有消歧后连到 onSocketError()");

    QCOMPARE(connectedSpy.count(), 0);
    QVERIFY(!client.isConnected());

    const QString reason = errorSpy.first().at(0).toString();
    QVERIFY2(!reason.isEmpty(), "connectionError 的原因字符串不能是空的");
}

// ---------------------------------------------------------------------------
//  ③ 断开：disconnected 信号发出，状态回到未连接
// ---------------------------------------------------------------------------
void TestTcpConnect::disconnectEmitsDisconnected()
{
    ModbusTcpClient::Config config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = m_server.serverPort();

    ModbusTcpClient client(config);

    QSignalSpy connectedSpy(&client, &IDeviceClient::connected);
    client.connectToDevice();
    QVERIFY(connectedSpy.wait(3000));

    QSignalSpy disconnectedSpy(&client, &IDeviceClient::disconnected);
    client.disconnectFromDevice();

    QVERIFY2(disconnectedSpy.wait(3000),
             "disconnectFromDevice() 之后要收到 disconnected 信号");

    QVERIFY(!client.isConnected());
}

QTEST_GUILESS_MAIN(TestTcpConnect)

#include "test_tcp_connect.moc"
